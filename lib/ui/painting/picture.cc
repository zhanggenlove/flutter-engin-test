// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/picture.h"

#include <memory>
#include <utility>

#include "flutter/fml/make_copyable.h"
#include "flutter/lib/ui/painting/canvas.h"
#include "flutter/lib/ui/painting/display_list_deferred_image_gpu_skia.h"
#include "flutter/lib/ui/ui_dart_state.h"
#if IMPELLER_SUPPORTS_RENDERING
#include "flutter/lib/ui/painting/display_list_deferred_image_gpu_impeller.h"
#endif  // IMPELLER_SUPPORTS_RENDERING
#include "third_party/tonic/converter/dart_converter.h"
#include "third_party/tonic/dart_args.h"
#include "third_party/tonic/dart_binding_macros.h"
#include "third_party/tonic/dart_library_natives.h"
#include "third_party/tonic/dart_persistent_value.h"
#include "third_party/tonic/logging/dart_invoke.h"

namespace flutter {

IMPLEMENT_WRAPPERTYPEINFO(ui, Picture);

fml::RefPtr<Picture> Picture::Create(
    Dart_Handle dart_handle,
    flutter::SkiaGPUObject<DisplayList> display_list) {
  auto canvas_picture = fml::MakeRefCounted<Picture>(std::move(display_list));

  canvas_picture->AssociateWithDartWrapper(dart_handle);
  return canvas_picture;
}

Picture::Picture(flutter::SkiaGPUObject<DisplayList> display_list)
    : display_list_(std::move(display_list)) {}

Picture::~Picture() = default;

Dart_Handle Picture::toImage(uint32_t width,
                             uint32_t height,
                             Dart_Handle raw_image_callback) {
  if (!display_list_.skia_object()) {
    return tonic::ToDart("Picture is null");
  }
  return RasterizeToImage(display_list_.skia_object(), width, height,
                          raw_image_callback);
}

void Picture::toImageSync(uint32_t width,
                          uint32_t height,
                          Dart_Handle raw_image_handle) {
  FML_DCHECK(display_list_.skia_object());
  RasterizeToImageSync(display_list_.skia_object(), width, height,
                       raw_image_handle);
}

static sk_sp<DlImage> CreateDeferredImage(
    bool impeller,
    sk_sp<DisplayList> display_list,
    uint32_t width,
    uint32_t height,
    fml::TaskRunnerAffineWeakPtr<SnapshotDelegate> snapshot_delegate,
    fml::RefPtr<fml::TaskRunner> raster_task_runner,
    fml::RefPtr<SkiaUnrefQueue> unref_queue) {
#if IMPELLER_SUPPORTS_RENDERING
  if (impeller) {
    return DlDeferredImageGPUImpeller::Make(
        std::move(display_list), SkISize::Make(width, height),
        std::move(snapshot_delegate), std::move(raster_task_runner));
  }
#endif  // IMPELLER_SUPPORTS_RENDERING

  const SkImageInfo image_info = SkImageInfo::Make(
      width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  return DlDeferredImageGPUSkia::Make(
      image_info, std::move(display_list), std::move(snapshot_delegate),
      raster_task_runner, std::move(unref_queue));
}

// static
void Picture::RasterizeToImageSync(sk_sp<DisplayList> display_list,
                                   uint32_t width,
                                   uint32_t height,
                                   Dart_Handle raw_image_handle) {
  auto* dart_state = UIDartState::Current();
  if (!dart_state) {
    return;
  }
  auto unref_queue = dart_state->GetSkiaUnrefQueue();
  auto snapshot_delegate = dart_state->GetSnapshotDelegate();
  auto raster_task_runner = dart_state->GetTaskRunners().GetRasterTaskRunner();

  auto image = CanvasImage::Create();
  auto dl_image = CreateDeferredImage(
      dart_state->IsImpellerEnabled(), std::move(display_list), width, height,
      std::move(snapshot_delegate), std::move(raster_task_runner),
      std::move(unref_queue));
  image->set_image(dl_image);
  image->AssociateWithDartWrapper(raw_image_handle);
}

void Picture::dispose() {
  display_list_.reset();
  ClearDartWrapper();
}

size_t Picture::GetAllocationSize() const {
  if (auto display_list = display_list_.skia_object()) {
    return display_list->bytes() + sizeof(Picture);
  } else {
    return sizeof(Picture);
  }
}

Dart_Handle Picture::RasterizeToImage(const sk_sp<DisplayList>& display_list,
                                      uint32_t width,
                                      uint32_t height,
                                      Dart_Handle raw_image_callback) {
  return RasterizeToImage(display_list, nullptr, width, height,
                          raw_image_callback);
}

Dart_Handle Picture::RasterizeLayerTreeToImage(
    std::shared_ptr<LayerTree> layer_tree,
    uint32_t width,
    uint32_t height,
    Dart_Handle raw_image_callback) {
  return RasterizeToImage(nullptr, std::move(layer_tree), width, height,
                          raw_image_callback);
}

Dart_Handle Picture::RasterizeToImage(const sk_sp<DisplayList>& display_list,
                                      std::shared_ptr<LayerTree> layer_tree,
                                      uint32_t width,
                                      uint32_t height,
                                      Dart_Handle raw_image_callback) {
  if (Dart_IsNull(raw_image_callback) || !Dart_IsClosure(raw_image_callback)) {
    return tonic::ToDart("Image callback was invalid");
  }

  if (width == 0 || height == 0) {
    return tonic::ToDart("Image dimensions for scene were invalid.");
  }

  auto* dart_state = UIDartState::Current();
  auto image_callback = std::make_unique<tonic::DartPersistentValue>(
      dart_state, raw_image_callback);
  auto unref_queue = dart_state->GetSkiaUnrefQueue();
  auto ui_task_runner = dart_state->GetTaskRunners().GetUITaskRunner();
  auto raster_task_runner = dart_state->GetTaskRunners().GetRasterTaskRunner();
  auto snapshot_delegate = dart_state->GetSnapshotDelegate();

  // We can't create an image on this task runner because we don't have a
  // graphics context. Even if we did, it would be slow anyway. Also, this
  // thread owns the sole reference to the layer tree. So we do it in the
  // raster thread.

  auto picture_bounds = SkISize::Make(width, height);

  auto ui_task =
      // The static leak checker gets confused by the use of fml::MakeCopyable.
      // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
      fml::MakeCopyable([image_callback = std::move(image_callback),
                         unref_queue](sk_sp<DlImage> image) mutable {
        auto dart_state = image_callback->dart_state().lock();
        if (!dart_state) {
          // The root isolate could have died in the meantime.
          return;
        }
        tonic::DartState::Scope scope(dart_state);

        if (!image) {
          tonic::DartInvoke(image_callback->Get(), {Dart_Null()});
          return;
        }

        if (image->skia_image()) {
          image =
              DlImageGPU::Make({image->skia_image(), std::move(unref_queue)});
        }

        auto dart_image = CanvasImage::Create();
        dart_image->set_image(image);
        auto* raw_dart_image = tonic::ToDart(dart_image);

        // All done!
        tonic::DartInvoke(image_callback->Get(), {raw_dart_image});

        // image_callback is associated with the Dart isolate and must be
        // deleted on the UI thread.
        image_callback.reset();
      });

  // Kick things off on the raster rask runner.
  fml::TaskRunner::RunNowOrPostTask(
      raster_task_runner,
      [ui_task_runner, snapshot_delegate, display_list, picture_bounds, ui_task,
       layer_tree = std::move(layer_tree)] {
        sk_sp<DlImage> image;
        if (layer_tree) {
          auto display_list = layer_tree->Flatten(
              SkRect::MakeWH(picture_bounds.width(), picture_bounds.height()),
              snapshot_delegate->GetTextureRegistry(),
              snapshot_delegate->GetGrContext());

          image = snapshot_delegate->MakeRasterSnapshot(display_list,
                                                        picture_bounds);
        } else {
          image = snapshot_delegate->MakeRasterSnapshot(display_list,
                                                        picture_bounds);
        }

        fml::TaskRunner::RunNowOrPostTask(
            ui_task_runner, [ui_task, image]() { ui_task(image); });
      });

  return Dart_Null();
}

}  // namespace flutter
