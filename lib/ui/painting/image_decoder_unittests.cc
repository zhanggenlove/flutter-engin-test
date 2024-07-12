// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/common/task_runners.h"
#include "flutter/fml/mapping.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/impeller/core/allocator.h"
#include "flutter/impeller/core/device_buffer.h"
#include "flutter/impeller/geometry/size.h"
#include "flutter/impeller/renderer/context.h"
#include "flutter/lib/ui/painting/image_decoder.h"
#include "flutter/lib/ui/painting/image_decoder_impeller.h"
#include "flutter/lib/ui/painting/image_decoder_skia.h"
#include "flutter/lib/ui/painting/multi_frame_codec.h"
#include "flutter/runtime/dart_vm.h"
#include "flutter/runtime/dart_vm_lifecycle.h"
#include "flutter/testing/dart_isolate_runner.h"
#include "flutter/testing/elf_loader.h"
#include "flutter/testing/fixture_test.h"
#include "flutter/testing/post_task_sync.h"
#include "flutter/testing/test_dart_native_resolver.h"
#include "flutter/testing/test_gl_surface.h"
#include "flutter/testing/testing.h"
#include "third_party/skia/include/codec/SkCodecAnimation.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkEncodedImageFormat.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSize.h"

// CREATE_NATIVE_ENTRY is leaky by design
// NOLINTBEGIN(clang-analyzer-core.StackAddressEscape)

namespace impeller {

class TestImpellerTexture : public Texture {
 public:
  explicit TestImpellerTexture(TextureDescriptor desc) : Texture(desc) {}

  void SetLabel(std::string_view label) override {}
  bool IsValid() const override { return true; }
  ISize GetSize() const { return GetTextureDescriptor().size; }

  bool OnSetContents(const uint8_t* contents, size_t length, size_t slice) {
    return true;
  }
  bool OnSetContents(std::shared_ptr<const fml::Mapping> mapping,
                     size_t slice) {
    return true;
  }
};

class TestImpellerDeviceBuffer : public DeviceBuffer {
 public:
  explicit TestImpellerDeviceBuffer(DeviceBufferDescriptor desc)
      : DeviceBuffer(desc) {
    bytes_ = static_cast<uint8_t*>(malloc(desc.size));
  }

  ~TestImpellerDeviceBuffer() { free(bytes_); }

 private:
  std::shared_ptr<Texture> AsTexture(Allocator& allocator,
                                     const TextureDescriptor& descriptor,
                                     uint16_t row_bytes) const override {
    return nullptr;
  }

  bool SetLabel(const std::string& label) override { return true; }

  bool SetLabel(const std::string& label, Range range) override { return true; }

  uint8_t* OnGetContents() const override { return bytes_; }

  bool OnCopyHostBuffer(const uint8_t* source,
                        Range source_range,
                        size_t offset) override {
    for (auto i = source_range.offset; i < source_range.length; i++, offset++) {
      bytes_[offset] = source[i];
    }
    return true;
  }

  uint8_t* bytes_;
};

class TestImpellerAllocator : public impeller::Allocator {
 public:
  TestImpellerAllocator() {}

  ~TestImpellerAllocator() = default;

 private:
  uint16_t MinimumBytesPerRow(PixelFormat format) const override { return 0; }

  ISize GetMaxTextureSizeSupported() const override {
    return ISize{2048, 2048};
  }

  std::shared_ptr<DeviceBuffer> OnCreateBuffer(
      const DeviceBufferDescriptor& desc) override {
    return std::make_shared<TestImpellerDeviceBuffer>(desc);
  }

  std::shared_ptr<Texture> OnCreateTexture(
      const TextureDescriptor& desc) override {
    return std::make_shared<TestImpellerTexture>(desc);
  }
};

class TestImpellerContext : public impeller::Context {
 public:
  TestImpellerContext() = default;

  bool IsValid() const override { return true; }

  const std::shared_ptr<const Capabilities>& GetCapabilities() const override {
    return capabilities_;
  }

  std::shared_ptr<Allocator> GetResourceAllocator() const override {
    return std::make_shared<TestImpellerAllocator>();
  }

  std::shared_ptr<ShaderLibrary> GetShaderLibrary() const override {
    return nullptr;
  }

  std::shared_ptr<SamplerLibrary> GetSamplerLibrary() const override {
    return nullptr;
  }

  std::shared_ptr<PipelineLibrary> GetPipelineLibrary() const override {
    return nullptr;
  }

  std::shared_ptr<CommandBuffer> CreateCommandBuffer() const override {
    command_buffer_count_ += 1;
    return nullptr;
  }

  mutable size_t command_buffer_count_ = 0;

 private:
  std::shared_ptr<const Capabilities> capabilities_;
};

}  // namespace impeller

namespace flutter {
namespace testing {

class TestIOManager final : public IOManager {
 public:
  explicit TestIOManager(const fml::RefPtr<fml::TaskRunner>& task_runner,
                         bool has_gpu_context = true)
      : gl_surface_(SkISize::Make(1, 1)),
        impeller_context_(std::make_shared<impeller::TestImpellerContext>()),
        gl_context_(has_gpu_context ? gl_surface_.CreateGrContext() : nullptr),
        weak_gl_context_factory_(
            has_gpu_context
                ? std::make_unique<fml::WeakPtrFactory<GrDirectContext>>(
                      gl_context_.get())
                : nullptr),
        unref_queue_(fml::MakeRefCounted<SkiaUnrefQueue>(
            task_runner,
            fml::TimeDelta::FromNanoseconds(0),
            gl_context_)),
        runner_(task_runner),
        is_gpu_disabled_sync_switch_(std::make_shared<fml::SyncSwitch>()),
        weak_factory_(this) {
    FML_CHECK(task_runner->RunsTasksOnCurrentThread())
        << "The IO manager must be initialized its primary task runner. The "
           "test harness may not be set up correctly/safely.";
    weak_prototype_ = weak_factory_.GetWeakPtr();
  }

  ~TestIOManager() override {
    fml::AutoResetWaitableEvent latch;
    fml::TaskRunner::RunNowOrPostTask(runner_,
                                      [&latch, queue = unref_queue_]() {
                                        queue->Drain();
                                        latch.Signal();
                                      });
    latch.Wait();
  }

  // |IOManager|
  fml::WeakPtr<IOManager> GetWeakIOManager() const override {
    return weak_prototype_;
  }

  // |IOManager|
  fml::WeakPtr<GrDirectContext> GetResourceContext() const override {
    return weak_gl_context_factory_ ? weak_gl_context_factory_->GetWeakPtr()
                                    : fml::WeakPtr<GrDirectContext>{};
  }

  // |IOManager|
  fml::RefPtr<flutter::SkiaUnrefQueue> GetSkiaUnrefQueue() const override {
    return unref_queue_;
  }

  // |IOManager|
  std::shared_ptr<const fml::SyncSwitch> GetIsGpuDisabledSyncSwitch() override {
    did_access_is_gpu_disabled_sync_switch_ = true;
    return is_gpu_disabled_sync_switch_;
  }

  // |IOManager|
  std::shared_ptr<impeller::Context> GetImpellerContext() const override {
    return impeller_context_;
  }

  void SetGpuDisabled(bool disabled) {
    is_gpu_disabled_sync_switch_->SetSwitch(disabled);
  }

  bool did_access_is_gpu_disabled_sync_switch_ = false;

 private:
  TestGLSurface gl_surface_;
  std::shared_ptr<impeller::Context> impeller_context_;
  sk_sp<GrDirectContext> gl_context_;
  std::unique_ptr<fml::WeakPtrFactory<GrDirectContext>>
      weak_gl_context_factory_;
  fml::RefPtr<SkiaUnrefQueue> unref_queue_;
  fml::WeakPtr<TestIOManager> weak_prototype_;
  fml::RefPtr<fml::TaskRunner> runner_;
  std::shared_ptr<fml::SyncSwitch> is_gpu_disabled_sync_switch_;
  fml::WeakPtrFactory<TestIOManager> weak_factory_;

  FML_DISALLOW_COPY_AND_ASSIGN(TestIOManager);
};

static sk_sp<SkData> OpenFixtureAsSkData(const char* name) {
  auto fixtures_directory =
      fml::OpenDirectory(GetFixturesPath(), false, fml::FilePermission::kRead);
  if (!fixtures_directory.is_valid()) {
    return nullptr;
  }

  auto fixture_mapping =
      fml::FileMapping::CreateReadOnly(fixtures_directory, name);

  if (!fixture_mapping) {
    return nullptr;
  }

  SkData::ReleaseProc on_release = [](const void* ptr, void* context) -> void {
    delete reinterpret_cast<fml::FileMapping*>(context);
  };

  auto data = SkData::MakeWithProc(fixture_mapping->GetMapping(),
                                   fixture_mapping->GetSize(), on_release,
                                   fixture_mapping.get());

  if (!data) {
    return nullptr;
  }
  // The data is now owned by Skia.
  fixture_mapping.release();
  return data;
}

class ImageDecoderFixtureTest : public FixtureTest {};

TEST_F(ImageDecoderFixtureTest, CanCreateImageDecoder) {
  auto loop = fml::ConcurrentMessageLoop::Create();
  auto thread_task_runner = CreateNewThread();
  TaskRunners runners(GetCurrentTestName(),  // label
                      thread_task_runner,    // platform
                      thread_task_runner,    // raster
                      thread_task_runner,    // ui
                      thread_task_runner     // io

  );

  PostTaskSync(runners.GetIOTaskRunner(), [&]() {
    TestIOManager manager(runners.GetIOTaskRunner());
    Settings settings;
    auto decoder = ImageDecoder::Make(settings, runners, loop->GetTaskRunner(),
                                      manager.GetWeakIOManager(),
                                      std::make_shared<fml::SyncSwitch>());
    ASSERT_NE(decoder, nullptr);
  });
}

/// An Image generator that pretends it can't recognize the data it was given.
class UnknownImageGenerator : public ImageGenerator {
 public:
  UnknownImageGenerator() : info_(SkImageInfo::MakeUnknown()){};
  ~UnknownImageGenerator() = default;
  const SkImageInfo& GetInfo() { return info_; }

  unsigned int GetFrameCount() const { return 1; }

  unsigned int GetPlayCount() const { return 1; }

  const ImageGenerator::FrameInfo GetFrameInfo(unsigned int frame_index) {
    return {std::nullopt, 0, SkCodecAnimation::DisposalMethod::kKeep};
  }

  SkISize GetScaledDimensions(float scale) {
    return SkISize::Make(info_.width(), info_.height());
  }

  bool GetPixels(const SkImageInfo& info,
                 void* pixels,
                 size_t row_bytes,
                 unsigned int frame_index,
                 std::optional<unsigned int> prior_frame) {
    return false;
  };

 private:
  SkImageInfo info_;
};

TEST_F(ImageDecoderFixtureTest, InvalidImageResultsError) {
  auto loop = fml::ConcurrentMessageLoop::Create();
  auto thread_task_runner = CreateNewThread();
  TaskRunners runners(GetCurrentTestName(),  // label
                      thread_task_runner,    // platform
                      thread_task_runner,    // raster
                      thread_task_runner,    // ui
                      thread_task_runner     // io
  );

  fml::AutoResetWaitableEvent latch;
  thread_task_runner->PostTask([&]() {
    TestIOManager manager(runners.GetIOTaskRunner());
    Settings settings;
    auto decoder = ImageDecoder::Make(settings, runners, loop->GetTaskRunner(),
                                      manager.GetWeakIOManager(),
                                      std::make_shared<fml::SyncSwitch>());

    auto data = OpenFixtureAsSkData("ThisDoesNotExist.jpg");
    ASSERT_FALSE(data);

    fml::RefPtr<ImageDescriptor> image_descriptor =
        fml::MakeRefCounted<ImageDescriptor>(
            std::move(data), std::make_unique<UnknownImageGenerator>());

    ImageDecoder::ImageResult callback = [&](const sk_sp<DlImage>& image,
                                             const std::string& decode_error) {
      ASSERT_TRUE(runners.GetUITaskRunner()->RunsTasksOnCurrentThread());
      ASSERT_FALSE(image);
      latch.Signal();
    };
    decoder->Decode(image_descriptor, 0, 0, callback);
  });
  latch.Wait();
}

TEST_F(ImageDecoderFixtureTest, ValidImageResultsInSuccess) {
  auto loop = fml::ConcurrentMessageLoop::Create();
  TaskRunners runners(GetCurrentTestName(),         // label
                      CreateNewThread("platform"),  // platform
                      CreateNewThread("raster"),    // raster
                      CreateNewThread("ui"),        // ui
                      CreateNewThread("io")         // io
  );

  fml::AutoResetWaitableEvent latch;

  std::unique_ptr<TestIOManager> io_manager;

  auto release_io_manager = [&]() {
    io_manager.reset();
    latch.Signal();
  };
  auto decode_image = [&]() {
    Settings settings;
    std::unique_ptr<ImageDecoder> image_decoder = ImageDecoder::Make(
        settings, runners, loop->GetTaskRunner(),
        io_manager->GetWeakIOManager(), std::make_shared<fml::SyncSwitch>());

    auto data = OpenFixtureAsSkData("DashInNooglerHat.jpg");

    ASSERT_TRUE(data);
    ASSERT_GE(data->size(), 0u);

    ImageGeneratorRegistry registry;
    std::shared_ptr<ImageGenerator> generator =
        registry.CreateCompatibleGenerator(data);
    ASSERT_TRUE(generator);

    auto descriptor = fml::MakeRefCounted<ImageDescriptor>(
        std::move(data), std::move(generator));

    ImageDecoder::ImageResult callback = [&](const sk_sp<DlImage>& image,
                                             const std::string& decode_error) {
      ASSERT_TRUE(runners.GetUITaskRunner()->RunsTasksOnCurrentThread());
      ASSERT_TRUE(image && image->skia_image());
      EXPECT_TRUE(io_manager->did_access_is_gpu_disabled_sync_switch_);
      runners.GetIOTaskRunner()->PostTask(release_io_manager);
    };
    EXPECT_FALSE(io_manager->did_access_is_gpu_disabled_sync_switch_);
    image_decoder->Decode(descriptor, descriptor->width(), descriptor->height(),
                          callback);
  };

  auto setup_io_manager_and_decode = [&]() {
    io_manager = std::make_unique<TestIOManager>(runners.GetIOTaskRunner());
    runners.GetUITaskRunner()->PostTask(decode_image);
  };

  runners.GetIOTaskRunner()->PostTask(setup_io_manager_and_decode);
  latch.Wait();
}

namespace {
float HalfToFloat(uint16_t half) {
  switch (half) {
    case 0x7c00:
      return std::numeric_limits<float>::infinity();
    case 0xfc00:
      return -std::numeric_limits<float>::infinity();
  }
  bool negative = half >> 15;
  uint16_t exponent = (half >> 10) & 0x1f;
  uint16_t fraction = half & 0x3ff;
  float fExponent = exponent - 15.0f;
  float fFraction = static_cast<float>(fraction) / 1024.f;
  float pow_value = powf(2.0f, fExponent);
  return (negative ? -1.f : 1.f) * pow_value * (1.0f + fFraction);
}
}  // namespace

TEST_F(ImageDecoderFixtureTest, ImpellerUploadToSharedNoGpu) {
#if !IMPELLER_SUPPORTS_RENDERING
  GTEST_SKIP() << "Impeller only test.";
#endif  // IMPELLER_SUPPORTS_RENDERING

  auto no_gpu_access_context =
      std::make_shared<impeller::TestImpellerContext>();
  auto gpu_disabled_switch = std::make_shared<fml::SyncSwitch>(true);

  auto info = SkImageInfo::Make(10, 10, SkColorType::kRGBA_8888_SkColorType,
                                SkAlphaType::kPremul_SkAlphaType);
  auto bitmap = std::make_shared<SkBitmap>();
  bitmap->allocPixels(info, 10 * 4);
  impeller::DeviceBufferDescriptor desc;
  desc.size = bitmap->computeByteSize();
  auto buffer = std::make_shared<impeller::TestImpellerDeviceBuffer>(desc);

  auto result = ImageDecoderImpeller::UploadTextureToPrivate(
      no_gpu_access_context, buffer, info, bitmap, gpu_disabled_switch);
  ASSERT_EQ(no_gpu_access_context->command_buffer_count_, 0ul);
  ASSERT_EQ(result.second, "");

  result = ImageDecoderImpeller::UploadTextureToShared(
      no_gpu_access_context, bitmap, gpu_disabled_switch, true);
  ASSERT_EQ(no_gpu_access_context->command_buffer_count_, 0ul);
  ASSERT_EQ(result.second, "");
}

TEST_F(ImageDecoderFixtureTest, ImpellerNullColorspace) {
  auto info = SkImageInfo::Make(10, 10, SkColorType::kRGBA_8888_SkColorType,
                                SkAlphaType::kPremul_SkAlphaType);
  SkBitmap bitmap;
  bitmap.allocPixels(info, 10 * 4);
  auto data = SkData::MakeWithoutCopy(bitmap.getPixels(), 10 * 10 * 4);
  auto image = SkImages::RasterFromBitmap(bitmap);
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(SkISize::Make(10, 10), image->dimensions());
  ASSERT_EQ(nullptr, image->colorSpace());

  auto descriptor = fml::MakeRefCounted<ImageDescriptor>(
      std::move(data), image->imageInfo(), 10 * 4);

#if IMPELLER_SUPPORTS_RENDERING
  std::shared_ptr<impeller::Allocator> allocator =
      std::make_shared<impeller::TestImpellerAllocator>();
  std::optional<DecompressResult> decompressed =
      ImageDecoderImpeller::DecompressTexture(
          descriptor.get(), SkISize::Make(100, 100), {100, 100},
          /*supports_wide_gamut=*/true, allocator);
  ASSERT_TRUE(decompressed.has_value());
  ASSERT_EQ(decompressed->image_info.colorType(), kRGBA_8888_SkColorType);
  ASSERT_EQ(decompressed->image_info.colorSpace(), nullptr);
#endif  // IMPELLER_SUPPORTS_RENDERING
}

TEST_F(ImageDecoderFixtureTest, ImpellerWideGamutDisplayP3) {
  auto data = OpenFixtureAsSkData("DisplayP3Logo.png");
  auto image = SkImages::DeferredFromEncodedData(data);
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(SkISize::Make(100, 100), image->dimensions());

  ImageGeneratorRegistry registry;
  std::shared_ptr<ImageGenerator> generator =
      registry.CreateCompatibleGenerator(data);
  ASSERT_TRUE(generator);

  auto descriptor = fml::MakeRefCounted<ImageDescriptor>(std::move(data),
                                                         std::move(generator));

#if IMPELLER_SUPPORTS_RENDERING
  std::shared_ptr<impeller::Allocator> allocator =
      std::make_shared<impeller::TestImpellerAllocator>();
  std::optional<DecompressResult> wide_result =
      ImageDecoderImpeller::DecompressTexture(
          descriptor.get(), SkISize::Make(100, 100), {100, 100},
          /*supports_wide_gamut=*/true, allocator);
  ASSERT_TRUE(wide_result.has_value());
  ASSERT_EQ(wide_result->image_info.colorType(), kRGBA_F16_SkColorType);
  ASSERT_TRUE(wide_result->image_info.colorSpace()->isSRGB());

  const SkPixmap& wide_pixmap = wide_result->sk_bitmap->pixmap();
  const uint16_t* half_ptr = static_cast<const uint16_t*>(wide_pixmap.addr());
  bool found_deep_red = false;
  for (int i = 0; i < wide_pixmap.width() * wide_pixmap.height(); ++i) {
    float red = HalfToFloat(*half_ptr++);
    float green = HalfToFloat(*half_ptr++);
    float blue = HalfToFloat(*half_ptr++);
    half_ptr++;  // alpha
    if (fabsf(red - 1.0931f) < 0.01f && fabsf(green - -0.2268f) < 0.01f &&
        fabsf(blue - -0.1501f) < 0.01f) {
      found_deep_red = true;
      break;
    }
  }

  ASSERT_TRUE(found_deep_red);
  std::optional<DecompressResult> narrow_result =
      ImageDecoderImpeller::DecompressTexture(
          descriptor.get(), SkISize::Make(100, 100), {100, 100},
          /*supports_wide_gamut=*/false, allocator);

  ASSERT_TRUE(narrow_result.has_value());
  ASSERT_EQ(narrow_result->image_info.colorType(), kRGBA_8888_SkColorType);
#endif  // IMPELLER_SUPPORTS_RENDERING
}

TEST_F(ImageDecoderFixtureTest, ImpellerPixelConversion32F) {
  auto info = SkImageInfo::Make(10, 10, SkColorType::kRGBA_F32_SkColorType,
                                SkAlphaType::kUnpremul_SkAlphaType);
  SkBitmap bitmap;
  bitmap.allocPixels(info, 10 * 16);
  auto data = SkData::MakeWithoutCopy(bitmap.getPixels(), 10 * 10 * 16);
  auto image = SkImages::RasterFromBitmap(bitmap);
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(SkISize::Make(10, 10), image->dimensions());
  ASSERT_EQ(nullptr, image->colorSpace());

  auto descriptor = fml::MakeRefCounted<ImageDescriptor>(
      std::move(data), image->imageInfo(), 10 * 16);

#if IMPELLER_SUPPORTS_RENDERING
  std::shared_ptr<impeller::Allocator> allocator =
      std::make_shared<impeller::TestImpellerAllocator>();
  std::optional<DecompressResult> decompressed =
      ImageDecoderImpeller::DecompressTexture(
          descriptor.get(), SkISize::Make(100, 100), {100, 100},
          /*supports_wide_gamut=*/true, allocator);

  ASSERT_TRUE(decompressed.has_value());
  ASSERT_EQ(decompressed->image_info.colorType(), kRGBA_F16_SkColorType);
  ASSERT_EQ(decompressed->image_info.colorSpace(), nullptr);
#endif  // IMPELLER_SUPPORTS_RENDERING
}

namespace {
float DecodeBGR10(uint32_t x) {
  const float max = 1.25098f;
  const float min = -0.752941f;
  const float intercept = min;
  const float slope = (max - min) / 1024.0f;
  return (x * slope) + intercept;
}
}  // namespace

TEST_F(ImageDecoderFixtureTest, ImpellerWideGamutDisplayP3Opaque) {
  auto data = OpenFixtureAsSkData("DisplayP3Logo.jpg");
  auto image = SkImages::DeferredFromEncodedData(data);
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(SkISize::Make(100, 100), image->dimensions());

  ImageGeneratorRegistry registry;
  std::shared_ptr<ImageGenerator> generator =
      registry.CreateCompatibleGenerator(data);
  ASSERT_TRUE(generator);

  auto descriptor = fml::MakeRefCounted<ImageDescriptor>(std::move(data),
                                                         std::move(generator));

#if IMPELLER_SUPPORTS_RENDERING
  std::shared_ptr<impeller::Allocator> allocator =
      std::make_shared<impeller::TestImpellerAllocator>();
  std::optional<DecompressResult> wide_result =
      ImageDecoderImpeller::DecompressTexture(
          descriptor.get(), SkISize::Make(100, 100), {100, 100},
          /*supports_wide_gamut=*/true, allocator);

  ASSERT_TRUE(wide_result.has_value());
  ASSERT_EQ(wide_result->image_info.colorType(), kBGR_101010x_XR_SkColorType);
  ASSERT_TRUE(wide_result->image_info.colorSpace()->isSRGB());

  const SkPixmap& wide_pixmap = wide_result->sk_bitmap->pixmap();
  const uint32_t* pixel_ptr = static_cast<const uint32_t*>(wide_pixmap.addr());
  bool found_deep_red = false;
  for (int i = 0; i < wide_pixmap.width() * wide_pixmap.height(); ++i) {
    uint32_t pixel = *pixel_ptr++;
    float blue = DecodeBGR10((pixel >> 0) & 0x3ff);
    float green = DecodeBGR10((pixel >> 10) & 0x3ff);
    float red = DecodeBGR10((pixel >> 20) & 0x3ff);
    if (fabsf(red - 1.0931f) < 0.01f && fabsf(green - -0.2268f) < 0.01f &&
        fabsf(blue - -0.1501f) < 0.01f) {
      found_deep_red = true;
      break;
    }
  }
  ASSERT_TRUE(found_deep_red);

  std::optional<DecompressResult> narrow_result =
      ImageDecoderImpeller::DecompressTexture(
          descriptor.get(), SkISize::Make(100, 100), {100, 100},
          /*supports_wide_gamut=*/false, allocator);

  ASSERT_TRUE(narrow_result.has_value());
  ASSERT_EQ(narrow_result->image_info.colorType(), kRGBA_8888_SkColorType);
#endif  // IMPELLER_SUPPORTS_RENDERING
}

TEST_F(ImageDecoderFixtureTest, ImpellerNonWideGamut) {
  auto data = OpenFixtureAsSkData("Horizontal.jpg");
  auto image = SkImages::DeferredFromEncodedData(data);
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(SkISize::Make(600, 200), image->dimensions());

  ImageGeneratorRegistry registry;
  std::shared_ptr<ImageGenerator> generator =
      registry.CreateCompatibleGenerator(data);
  ASSERT_TRUE(generator);

  auto descriptor = fml::MakeRefCounted<ImageDescriptor>(std::move(data),
                                                         std::move(generator));

#if IMPELLER_SUPPORTS_RENDERING
  std::shared_ptr<impeller::Allocator> allocator =
      std::make_shared<impeller::TestImpellerAllocator>();
  std::optional<DecompressResult> result =
      ImageDecoderImpeller::DecompressTexture(
          descriptor.get(), SkISize::Make(600, 200), {600, 200},
          /*supports_wide_gamut=*/true, allocator);

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->image_info.colorType(), kRGBA_8888_SkColorType);
#endif  // IMPELLER_SUPPORTS_RENDERING
}

TEST_F(ImageDecoderFixtureTest, ExifDataIsRespectedOnDecode) {
  auto loop = fml::ConcurrentMessageLoop::Create();
  TaskRunners runners(GetCurrentTestName(),         // label
                      CreateNewThread("platform"),  // platform
                      CreateNewThread("raster"),    // raster
                      CreateNewThread("ui"),        // ui
                      CreateNewThread("io")         // io
  );

  fml::AutoResetWaitableEvent latch;

  std::unique_ptr<IOManager> io_manager;

  auto release_io_manager = [&]() {
    io_manager.reset();
    latch.Signal();
  };

  SkISize decoded_size = SkISize::MakeEmpty();
  auto decode_image = [&]() {
    Settings settings;
    std::unique_ptr<ImageDecoder> image_decoder = ImageDecoder::Make(
        settings, runners, loop->GetTaskRunner(),
        io_manager->GetWeakIOManager(), std::make_shared<fml::SyncSwitch>());

    auto data = OpenFixtureAsSkData("Horizontal.jpg");

    ASSERT_TRUE(data);
    ASSERT_GE(data->size(), 0u);

    ImageGeneratorRegistry registry;
    std::shared_ptr<ImageGenerator> generator =
        registry.CreateCompatibleGenerator(data);
    ASSERT_TRUE(generator);

    auto descriptor = fml::MakeRefCounted<ImageDescriptor>(
        std::move(data), std::move(generator));

    ImageDecoder::ImageResult callback = [&](const sk_sp<DlImage>& image,
                                             const std::string& decode_error) {
      ASSERT_TRUE(runners.GetUITaskRunner()->RunsTasksOnCurrentThread());
      ASSERT_TRUE(image && image->skia_image());
      decoded_size = image->skia_image()->dimensions();
      runners.GetIOTaskRunner()->PostTask(release_io_manager);
    };
    image_decoder->Decode(descriptor, descriptor->width(), descriptor->height(),
                          callback);
  };

  auto setup_io_manager_and_decode = [&]() {
    io_manager = std::make_unique<TestIOManager>(runners.GetIOTaskRunner());
    runners.GetUITaskRunner()->PostTask(decode_image);
  };

  runners.GetIOTaskRunner()->PostTask(setup_io_manager_and_decode);

  latch.Wait();

  ASSERT_EQ(decoded_size.width(), 600);
  ASSERT_EQ(decoded_size.height(), 200);
}

TEST_F(ImageDecoderFixtureTest, CanDecodeWithoutAGPUContext) {
  auto loop = fml::ConcurrentMessageLoop::Create();
  TaskRunners runners(GetCurrentTestName(),         // label
                      CreateNewThread("platform"),  // platform
                      CreateNewThread("raster"),    // raster
                      CreateNewThread("ui"),        // ui
                      CreateNewThread("io")         // io
  );

  fml::AutoResetWaitableEvent latch;

  std::unique_ptr<IOManager> io_manager;

  auto release_io_manager = [&]() {
    io_manager.reset();
    latch.Signal();
  };

  auto decode_image = [&]() {
    Settings settings;
    std::unique_ptr<ImageDecoder> image_decoder = ImageDecoder::Make(
        settings, runners, loop->GetTaskRunner(),
        io_manager->GetWeakIOManager(), std::make_shared<fml::SyncSwitch>());

    auto data = OpenFixtureAsSkData("DashInNooglerHat.jpg");

    ASSERT_TRUE(data);
    ASSERT_GE(data->size(), 0u);

    ImageGeneratorRegistry registry;
    std::shared_ptr<ImageGenerator> generator =
        registry.CreateCompatibleGenerator(data);
    ASSERT_TRUE(generator);

    auto descriptor = fml::MakeRefCounted<ImageDescriptor>(
        std::move(data), std::move(generator));

    ImageDecoder::ImageResult callback = [&](const sk_sp<DlImage>& image,
                                             const std::string& decode_error) {
      ASSERT_TRUE(runners.GetUITaskRunner()->RunsTasksOnCurrentThread());
      ASSERT_TRUE(image && image->skia_image());
      runners.GetIOTaskRunner()->PostTask(release_io_manager);
    };
    image_decoder->Decode(descriptor, descriptor->width(), descriptor->height(),
                          callback);
  };

  auto setup_io_manager_and_decode = [&]() {
    io_manager =
        std::make_unique<TestIOManager>(runners.GetIOTaskRunner(), false);
    runners.GetUITaskRunner()->PostTask(decode_image);
  };

  runners.GetIOTaskRunner()->PostTask(setup_io_manager_and_decode);

  latch.Wait();
}

TEST_F(ImageDecoderFixtureTest, CanDecodeWithResizes) {
  const auto image_dimensions = SkImages::DeferredFromEncodedData(
                                    OpenFixtureAsSkData("DashInNooglerHat.jpg"))
                                    ->dimensions();

  ASSERT_FALSE(image_dimensions.isEmpty());

  ASSERT_NE(image_dimensions.width(), image_dimensions.height());

  auto loop = fml::ConcurrentMessageLoop::Create();
  TaskRunners runners(GetCurrentTestName(),         // label
                      CreateNewThread("platform"),  // platform
                      CreateNewThread("raster"),    // raster
                      CreateNewThread("ui"),        // ui
                      CreateNewThread("io")         // io
  );

  fml::AutoResetWaitableEvent latch;
  std::unique_ptr<IOManager> io_manager;
  std::unique_ptr<ImageDecoder> image_decoder;

  // Setup the IO manager.
  PostTaskSync(runners.GetIOTaskRunner(), [&]() {
    io_manager = std::make_unique<TestIOManager>(runners.GetIOTaskRunner());
  });

  // Setup the image decoder.
  PostTaskSync(runners.GetUITaskRunner(), [&]() {
    Settings settings;
    image_decoder = ImageDecoder::Make(settings, runners, loop->GetTaskRunner(),
                                       io_manager->GetWeakIOManager(),
                                       std::make_shared<fml::SyncSwitch>());
  });

  // Setup a generic decoding utility that gives us the final decoded size.
  auto decoded_size = [&](uint32_t target_width,
                          uint32_t target_height) -> SkISize {
    SkISize final_size = SkISize::MakeEmpty();
    runners.GetUITaskRunner()->PostTask([&]() {
      auto data = OpenFixtureAsSkData("DashInNooglerHat.jpg");

      ASSERT_TRUE(data);
      ASSERT_GE(data->size(), 0u);

      ImageGeneratorRegistry registry;
      std::shared_ptr<ImageGenerator> generator =
          registry.CreateCompatibleGenerator(data);
      ASSERT_TRUE(generator);

      auto descriptor = fml::MakeRefCounted<ImageDescriptor>(
          std::move(data), std::move(generator));

      ImageDecoder::ImageResult callback =
          [&](const sk_sp<DlImage>& image, const std::string& decode_error) {
            ASSERT_TRUE(runners.GetUITaskRunner()->RunsTasksOnCurrentThread());
            ASSERT_TRUE(image && image->skia_image());
            final_size = image->skia_image()->dimensions();
            latch.Signal();
          };
      image_decoder->Decode(descriptor, target_width, target_height, callback);
    });
    latch.Wait();
    return final_size;
  };

  ASSERT_EQ(SkISize::Make(3024, 4032), image_dimensions);
  ASSERT_EQ(decoded_size(3024, 4032), image_dimensions);
  ASSERT_EQ(decoded_size(100, 100), SkISize::Make(100, 100));

  // Destroy the IO manager
  PostTaskSync(runners.GetIOTaskRunner(), [&]() { io_manager.reset(); });

  // Destroy the image decoder
  PostTaskSync(runners.GetUITaskRunner(), [&]() { image_decoder.reset(); });
}

// Verifies https://skia-review.googlesource.com/c/skia/+/259161 is present in
// Flutter.
TEST(ImageDecoderTest,
     VerifyCodecRepeatCountsForGifAndWebPAreConsistentWithLoopCounts) {
  auto gif_mapping = OpenFixtureAsSkData("hello_loop_2.gif");
  auto webp_mapping = OpenFixtureAsSkData("hello_loop_2.webp");

  ASSERT_TRUE(gif_mapping);
  ASSERT_TRUE(webp_mapping);

  ImageGeneratorRegistry registry;

  auto gif_generator = registry.CreateCompatibleGenerator(gif_mapping);
  auto webp_generator = registry.CreateCompatibleGenerator(webp_mapping);

  ASSERT_TRUE(gif_generator);
  ASSERT_TRUE(webp_generator);

  // Both fixtures have a loop count of 2.
  ASSERT_EQ(gif_generator->GetPlayCount(), static_cast<unsigned int>(2));
  ASSERT_EQ(webp_generator->GetPlayCount(), static_cast<unsigned int>(2));
}

TEST(ImageDecoderTest, VerifySimpleDecoding) {
  auto data = OpenFixtureAsSkData("Horizontal.jpg");
  auto image = SkImages::DeferredFromEncodedData(data);
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(SkISize::Make(600, 200), image->dimensions());

  ImageGeneratorRegistry registry;
  std::shared_ptr<ImageGenerator> generator =
      registry.CreateCompatibleGenerator(data);
  ASSERT_TRUE(generator);

  auto descriptor = fml::MakeRefCounted<ImageDescriptor>(std::move(data),
                                                         std::move(generator));

  ASSERT_EQ(ImageDecoderSkia::ImageFromCompressedData(
                descriptor.get(), 6, 2, fml::tracing::TraceFlow(""))
                ->dimensions(),
            SkISize::Make(6, 2));

#if IMPELLER_SUPPORTS_RENDERING
  std::shared_ptr<impeller::Allocator> allocator =
      std::make_shared<impeller::TestImpellerAllocator>();
  auto result_1 = ImageDecoderImpeller::DecompressTexture(
      descriptor.get(), SkISize::Make(6, 2), {100, 100},
      /*supports_wide_gamut=*/false, allocator);
  ASSERT_EQ(result_1.sk_bitmap->width(), 6);
  ASSERT_EQ(result_1.sk_bitmap->height(), 2);

  auto result_2 = ImageDecoderImpeller::DecompressTexture(
      descriptor.get(), SkISize::Make(60, 20), {10, 10},
      /*supports_wide_gamut=*/false, allocator);
  ASSERT_EQ(result_2.sk_bitmap->width(), 10);
  ASSERT_EQ(result_2.sk_bitmap->height(), 10);
#endif  // IMPELLER_SUPPORTS_RENDERING
}

TEST(ImageDecoderTest, VerifySubpixelDecodingPreservesExifOrientation) {
  auto data = OpenFixtureAsSkData("Horizontal.jpg");

  ImageGeneratorRegistry registry;
  std::shared_ptr<ImageGenerator> generator =
      registry.CreateCompatibleGenerator(data);
  ASSERT_TRUE(generator);
  auto descriptor =
      fml::MakeRefCounted<ImageDescriptor>(data, std::move(generator));

  auto image = SkImages::DeferredFromEncodedData(data);
  ASSERT_TRUE(image != nullptr);
  ASSERT_EQ(SkISize::Make(600, 200), image->dimensions());

  auto decode = [descriptor](uint32_t target_width, uint32_t target_height) {
    return ImageDecoderSkia::ImageFromCompressedData(
        descriptor.get(), target_width, target_height,
        fml::tracing::TraceFlow(""));
  };

  auto expected_data = OpenFixtureAsSkData("Horizontal.png");
  ASSERT_TRUE(expected_data != nullptr);
  ASSERT_FALSE(expected_data->isEmpty());

  auto assert_image = [&](auto decoded_image, const std::string& decode_error) {
    ASSERT_EQ(decoded_image->dimensions(), SkISize::Make(300, 100));
    ASSERT_TRUE(decoded_image->encodeToData(SkEncodedImageFormat::kPNG, 100)
                    ->equals(expected_data.get()));
  };

  assert_image(decode(300, 100), {});
}

TEST_F(ImageDecoderFixtureTest,
       MultiFrameCodecCanBeCollectedBeforeIOTasksFinish) {
  // This test verifies that the MultiFrameCodec safely shares state between
  // tasks on the IO and UI runners, and does not allow unsafe memory access if
  // the UI object is collected while the IO thread still has pending decode
  // work. This could happen in a real application if the engine is collected
  // while a multi-frame image is decoding. To exercise this, the test:
  //   - Starts a Dart VM
  //   - Latches the IO task runner
  //   - Create a MultiFrameCodec for an animated gif pointed to a callback
  //     in the Dart fixture
  //   - Calls getNextFrame on the UI task runner
  //   - Collects the MultiFrameCodec object before unlatching the IO task
  //     runner.
  //   - Unlatches the IO task runner
  auto settings = CreateSettingsForFixture();
  auto vm_ref = DartVMRef::Create(settings);
  auto vm_data = vm_ref.GetVMData();

  auto gif_mapping = OpenFixtureAsSkData("hello_loop_2.gif");

  ASSERT_TRUE(gif_mapping);

  ImageGeneratorRegistry registry;
  std::shared_ptr<ImageGenerator> gif_generator =
      registry.CreateCompatibleGenerator(gif_mapping);
  ASSERT_TRUE(gif_generator);

  TaskRunners runners(GetCurrentTestName(),         // label
                      CreateNewThread("platform"),  // platform
                      CreateNewThread("raster"),    // raster
                      CreateNewThread("ui"),        // ui
                      CreateNewThread("io")         // io
  );

  fml::AutoResetWaitableEvent io_latch;
  std::unique_ptr<TestIOManager> io_manager;

  // Setup the IO manager.
  PostTaskSync(runners.GetIOTaskRunner(), [&]() {
    io_manager = std::make_unique<TestIOManager>(runners.GetIOTaskRunner());
  });

  auto isolate = RunDartCodeInIsolate(vm_ref, settings, runners, "main", {},
                                      GetDefaultKernelFilePath(),
                                      io_manager->GetWeakIOManager());

  // Latch the IO task runner.
  runners.GetIOTaskRunner()->PostTask([&]() { io_latch.Wait(); });

  PostTaskSync(runners.GetUITaskRunner(), [&]() {
    fml::AutoResetWaitableEvent isolate_latch;
    fml::RefPtr<MultiFrameCodec> codec;
    EXPECT_TRUE(isolate->RunInIsolateScope([&]() -> bool {
      Dart_Handle library = Dart_RootLibrary();
      if (Dart_IsError(library)) {
        isolate_latch.Signal();
        return false;
      }
      Dart_Handle closure =
          Dart_GetField(library, Dart_NewStringFromCString("frameCallback"));
      if (Dart_IsError(closure) || !Dart_IsClosure(closure)) {
        isolate_latch.Signal();
        return false;
      }

      codec = fml::MakeRefCounted<MultiFrameCodec>(std::move(gif_generator));
      codec->getNextFrame(closure);
      codec = nullptr;
      isolate_latch.Signal();
      return true;
    }));
    isolate_latch.Wait();

    EXPECT_FALSE(codec);

    io_latch.Signal();
  });

  // Destroy the IO manager
  PostTaskSync(runners.GetIOTaskRunner(), [&]() { io_manager.reset(); });
}

TEST_F(ImageDecoderFixtureTest, MultiFrameCodecDidAccessGpuDisabledSyncSwitch) {
  auto settings = CreateSettingsForFixture();
  auto vm_ref = DartVMRef::Create(settings);
  auto vm_data = vm_ref.GetVMData();

  auto gif_mapping = OpenFixtureAsSkData("hello_loop_2.gif");

  ASSERT_TRUE(gif_mapping);

  ImageGeneratorRegistry registry;
  std::shared_ptr<ImageGenerator> gif_generator =
      registry.CreateCompatibleGenerator(gif_mapping);
  ASSERT_TRUE(gif_generator);

  TaskRunners runners(GetCurrentTestName(),         // label
                      CreateNewThread("platform"),  // platform
                      CreateNewThread("raster"),    // raster
                      CreateNewThread("ui"),        // ui
                      CreateNewThread("io")         // io
  );

  std::unique_ptr<TestIOManager> io_manager;
  fml::RefPtr<MultiFrameCodec> codec;
  fml::AutoResetWaitableEvent latch;

  auto validate_frame_callback = [&latch](Dart_NativeArguments args) {
    EXPECT_FALSE(Dart_IsNull(Dart_GetNativeArgument(args, 0)));
    latch.Signal();
  };

  AddNativeCallback("ValidateFrameCallback",
                    CREATE_NATIVE_ENTRY(validate_frame_callback));
  // Setup the IO manager.
  PostTaskSync(runners.GetIOTaskRunner(), [&]() {
    io_manager = std::make_unique<TestIOManager>(runners.GetIOTaskRunner());
  });

  auto isolate = RunDartCodeInIsolate(vm_ref, settings, runners, "main", {},
                                      GetDefaultKernelFilePath(),
                                      io_manager->GetWeakIOManager());

  PostTaskSync(runners.GetUITaskRunner(), [&]() {
    fml::AutoResetWaitableEvent isolate_latch;

    EXPECT_TRUE(isolate->RunInIsolateScope([&]() -> bool {
      Dart_Handle library = Dart_RootLibrary();
      if (Dart_IsError(library)) {
        isolate_latch.Signal();
        return false;
      }
      Dart_Handle closure =
          Dart_GetField(library, Dart_NewStringFromCString("frameCallback"));
      if (Dart_IsError(closure) || !Dart_IsClosure(closure)) {
        isolate_latch.Signal();
        return false;
      }

      EXPECT_FALSE(io_manager->did_access_is_gpu_disabled_sync_switch_);
      codec = fml::MakeRefCounted<MultiFrameCodec>(std::move(gif_generator));
      codec->getNextFrame(closure);
      isolate_latch.Signal();
      return true;
    }));
    isolate_latch.Wait();
  });

  PostTaskSync(runners.GetIOTaskRunner(), [&]() {
    EXPECT_TRUE(io_manager->did_access_is_gpu_disabled_sync_switch_);
  });

  latch.Wait();

  // Destroy the Isolate
  isolate = nullptr;

  // Destroy the MultiFrameCodec
  PostTaskSync(runners.GetUITaskRunner(), [&]() { codec = nullptr; });

  // Destroy the IO manager
  PostTaskSync(runners.GetIOTaskRunner(), [&]() { io_manager.reset(); });
}

TEST_F(ImageDecoderFixtureTest,
       MultiFrameCodecProducesATextureEvenIfGPUIsDisabledOnImpeller) {
  auto settings = CreateSettingsForFixture();
  settings.enable_impeller = true;
  auto vm_ref = DartVMRef::Create(settings);
  auto vm_data = vm_ref.GetVMData();

  auto gif_mapping = OpenFixtureAsSkData("hello_loop_2.gif");

  ASSERT_TRUE(gif_mapping);

  ImageGeneratorRegistry registry;
  std::shared_ptr<ImageGenerator> gif_generator =
      registry.CreateCompatibleGenerator(gif_mapping);
  ASSERT_TRUE(gif_generator);

  TaskRunners runners(GetCurrentTestName(),         // label
                      CreateNewThread("platform"),  // platform
                      CreateNewThread("raster"),    // raster
                      CreateNewThread("ui"),        // ui
                      CreateNewThread("io")         // io
  );

  std::unique_ptr<TestIOManager> io_manager;
  fml::RefPtr<MultiFrameCodec> codec;
  fml::AutoResetWaitableEvent latch;

  auto validate_frame_callback = [&latch](Dart_NativeArguments args) {
    EXPECT_FALSE(Dart_IsNull(Dart_GetNativeArgument(args, 0)));
    latch.Signal();
  };

  AddNativeCallback("ValidateFrameCallback",
                    CREATE_NATIVE_ENTRY(validate_frame_callback));

  // Setup the IO manager.
  PostTaskSync(runners.GetIOTaskRunner(), [&]() {
    io_manager = std::make_unique<TestIOManager>(runners.GetIOTaskRunner());
    // Mark GPU disabled.
    io_manager->SetGpuDisabled(true);
  });

  auto isolate = RunDartCodeInIsolate(vm_ref, settings, runners, "main", {},
                                      GetDefaultKernelFilePath(),
                                      io_manager->GetWeakIOManager());

  PostTaskSync(runners.GetUITaskRunner(), [&]() {
    fml::AutoResetWaitableEvent isolate_latch;

    EXPECT_TRUE(isolate->RunInIsolateScope([&]() -> bool {
      Dart_Handle library = Dart_RootLibrary();
      if (Dart_IsError(library)) {
        isolate_latch.Signal();
        return false;
      }
      Dart_Handle closure =
          Dart_GetField(library, Dart_NewStringFromCString("frameCallback"));
      if (Dart_IsError(closure) || !Dart_IsClosure(closure)) {
        isolate_latch.Signal();
        return false;
      }

      EXPECT_FALSE(io_manager->did_access_is_gpu_disabled_sync_switch_);
      codec = fml::MakeRefCounted<MultiFrameCodec>(std::move(gif_generator));
      codec->getNextFrame(closure);
      isolate_latch.Signal();
      return true;
    }));
    isolate_latch.Wait();
  });

  PostTaskSync(runners.GetIOTaskRunner(), [&]() {
    EXPECT_TRUE(io_manager->did_access_is_gpu_disabled_sync_switch_);
  });

  latch.Wait();

  // Destroy the Isolate
  isolate = nullptr;

  // Destroy the MultiFrameCodec
  PostTaskSync(runners.GetUITaskRunner(), [&]() { codec = nullptr; });

  // Destroy the IO manager
  PostTaskSync(runners.GetIOTaskRunner(), [&]() { io_manager.reset(); });
}

TEST_F(ImageDecoderFixtureTest, NullCheckBuffer) {
  auto context = std::make_shared<impeller::TestImpellerContext>();
  auto allocator = ImpellerAllocator(context->GetResourceAllocator());

  EXPECT_FALSE(allocator.allocPixelRef(nullptr));
}

}  // namespace testing
}  // namespace flutter

// NOLINTEND(clang-analyzer-core.StackAddressEscape)
