// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/image_encoding.h"
#include "flutter/lib/ui/painting/image_encoding_impl.h"

#include <memory>
#include <utility>

#include "flutter/common/task_runners.h"
#include "flutter/fml/build_config.h"
#include "flutter/fml/make_copyable.h"
#include "flutter/fml/trace_event.h"
#include "flutter/lib/ui/painting/image.h"
#if IMPELLER_SUPPORTS_RENDERING
#include "flutter/lib/ui/painting/image_encoding_impeller.h"
#endif  // IMPELLER_SUPPORTS_RENDERING
#include "flutter/lib/ui/painting/image_encoding_skia.h"
#include "third_party/skia/include/core/SkEncodedImageFormat.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/tonic/dart_persistent_value.h"
#include "third_party/tonic/logging/dart_invoke.h"
#include "third_party/tonic/typed_data/typed_list.h"

using tonic::DartInvoke;
using tonic::DartPersistentValue;
using tonic::ToDart;

namespace impeller {
class Context;
}  // namespace impeller
namespace flutter {
namespace {

// This must be kept in sync with the enum in painting.dart
enum ImageByteFormat {
  kRawRGBA,
  kRawStraightRGBA,
  kRawUnmodified,
  kRawExtendedRgba128,
  kPNG,
};

void FinalizeSkData(void* isolate_callback_data, void* peer) {
  SkData* buffer = reinterpret_cast<SkData*>(peer);
  buffer->unref();
}

void InvokeDataCallback(std::unique_ptr<DartPersistentValue> callback,
                        sk_sp<SkData> buffer) {
  std::shared_ptr<tonic::DartState> dart_state = callback->dart_state().lock();
  if (!dart_state) {
    return;
  }
  tonic::DartState::Scope scope(dart_state);
  if (!buffer) {
    DartInvoke(callback->value(), {Dart_Null()});
    return;
  }
  // Skia will not modify the buffer, and it is backed by memory that is
  // read/write, so Dart can be given direct access to the buffer through an
  // external Uint8List.
  void* bytes = const_cast<void*>(buffer->data());
  const intptr_t length = buffer->size();
  void* peer = reinterpret_cast<void*>(buffer.release());
  Dart_Handle dart_data = Dart_NewExternalTypedDataWithFinalizer(
      Dart_TypedData_kUint8, bytes, length, peer, length, FinalizeSkData);
  DartInvoke(callback->value(), {dart_data});
}

sk_sp<SkData> CopyImageByteData(const sk_sp<SkImage>& raster_image,
                                SkColorType color_type,
                                SkAlphaType alpha_type) {
  FML_DCHECK(raster_image);

  SkPixmap pixmap;

  if (!raster_image->peekPixels(&pixmap)) {
    FML_LOG(ERROR) << "Could not copy pixels from the raster image.";
    return nullptr;
  }

  // The color types already match. No need to swizzle. Return early.
  if (pixmap.colorType() == color_type && pixmap.alphaType() == alpha_type) {
    return SkData::MakeWithCopy(pixmap.addr(), pixmap.computeByteSize());
  }

  // Perform swizzle if the type doesnt match the specification.
  auto surface = SkSurface::MakeRaster(
      SkImageInfo::Make(raster_image->width(), raster_image->height(),
                        color_type, alpha_type, nullptr));

  if (!surface) {
    FML_LOG(ERROR) << "Could not set up the surface for swizzle.";
    return nullptr;
  }

  surface->writePixels(pixmap, 0, 0);

  if (!surface->peekPixels(&pixmap)) {
    FML_LOG(ERROR) << "Pixel address is not available.";
    return nullptr;
  }

  return SkData::MakeWithCopy(pixmap.addr(), pixmap.computeByteSize());
}

sk_sp<SkData> EncodeImage(const sk_sp<SkImage>& raster_image,
                          ImageByteFormat format) {
  TRACE_EVENT0("flutter", __FUNCTION__);

  if (!raster_image) {
    return nullptr;
  }

  switch (format) {
    case kPNG: {
      auto png_image =
          raster_image->encodeToData(SkEncodedImageFormat::kPNG, 0);

      if (png_image == nullptr) {
        FML_LOG(ERROR) << "Could not convert raster image to PNG.";
        return nullptr;
      };
      return png_image;
    }
    case kRawRGBA:
      return CopyImageByteData(raster_image, kRGBA_8888_SkColorType,
                               kPremul_SkAlphaType);

    case kRawStraightRGBA:
      return CopyImageByteData(raster_image, kRGBA_8888_SkColorType,
                               kUnpremul_SkAlphaType);

    case kRawUnmodified:
      return CopyImageByteData(raster_image, raster_image->colorType(),
                               raster_image->alphaType());
    case kRawExtendedRgba128:
      return CopyImageByteData(raster_image, kRGBA_F32_SkColorType,
                               kUnpremul_SkAlphaType);
  }

  FML_LOG(ERROR) << "Unknown error encoding image.";
  return nullptr;
}

void EncodeImageAndInvokeDataCallback(
    const sk_sp<DlImage>& image,
    std::unique_ptr<DartPersistentValue> callback,
    ImageByteFormat format,
    const fml::RefPtr<fml::TaskRunner>& ui_task_runner,
    const fml::RefPtr<fml::TaskRunner>& raster_task_runner,
    const fml::RefPtr<fml::TaskRunner>& io_task_runner,
    const fml::WeakPtr<GrDirectContext>& resource_context,
    const fml::TaskRunnerAffineWeakPtr<SnapshotDelegate>& snapshot_delegate,
    const std::shared_ptr<const fml::SyncSwitch>& is_gpu_disabled_sync_switch,
    const std::shared_ptr<impeller::Context>& impeller_context,
    bool is_impeller_enabled) {
  auto callback_task = fml::MakeCopyable(
      [callback = std::move(callback)](sk_sp<SkData> encoded) mutable {
        InvokeDataCallback(std::move(callback), std::move(encoded));
      });
  // The static leak checker gets confused by the use of fml::MakeCopyable in
  // EncodeImage.
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
  auto encode_task = [callback_task = std::move(callback_task), format,
                      ui_task_runner](const sk_sp<SkImage>& raster_image) {
    sk_sp<SkData> encoded = EncodeImage(raster_image, format);
    ui_task_runner->PostTask([callback_task = callback_task,
                              encoded = std::move(encoded)]() mutable {
      callback_task(std::move(encoded));
    });
  };

  FML_DCHECK(image);
#if IMPELLER_SUPPORTS_RENDERING
  if (is_impeller_enabled) {
    ImageEncodingImpeller::ConvertImageToRaster(
        image, encode_task, raster_task_runner, io_task_runner,
        is_gpu_disabled_sync_switch, impeller_context);
    return;
  }
#endif  // IMPELLER_SUPPORTS_RENDERING
  ConvertImageToRasterSkia(image, encode_task, raster_task_runner,
                           io_task_runner, resource_context, snapshot_delegate,
                           is_gpu_disabled_sync_switch);
}

}  // namespace

Dart_Handle EncodeImage(CanvasImage* canvas_image,
                        int format,
                        Dart_Handle callback_handle) {
  if (!canvas_image) {
    return ToDart("encode called with non-genuine Image.");
  }

  if (!Dart_IsClosure(callback_handle)) {
    return ToDart("Callback must be a function.");
  }

  ImageByteFormat image_format = static_cast<ImageByteFormat>(format);

  auto callback = std::make_unique<DartPersistentValue>(
      tonic::DartState::Current(), callback_handle);

  const auto& task_runners = UIDartState::Current()->GetTaskRunners();

  // The static leak checker gets confused by the use of fml::MakeCopyable.
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
  task_runners.GetIOTaskRunner()->PostTask(fml::MakeCopyable(
      [callback = std::move(callback), image = canvas_image->image(),
       image_format, ui_task_runner = task_runners.GetUITaskRunner(),
       raster_task_runner = task_runners.GetRasterTaskRunner(),
       io_task_runner = task_runners.GetIOTaskRunner(),
       io_manager = UIDartState::Current()->GetIOManager(),
       snapshot_delegate = UIDartState::Current()->GetSnapshotDelegate(),
       is_impeller_enabled =
           UIDartState::Current()->IsImpellerEnabled()]() mutable {
        EncodeImageAndInvokeDataCallback(
            image, std::move(callback), image_format, ui_task_runner,
            raster_task_runner, io_task_runner,
            io_manager->GetResourceContext(), snapshot_delegate,
            io_manager->GetIsGpuDisabledSyncSwitch(),
            io_manager->GetImpellerContext(), is_impeller_enabled);
      }));

  return Dart_Null();
}

}  // namespace flutter
