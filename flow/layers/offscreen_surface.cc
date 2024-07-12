// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/flow/layers/offscreen_surface.h"

#include "third_party/skia/include/core/SkImageEncoder.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceCharacterization.h"
#include "third_party/skia/include/utils/SkBase64.h"

namespace flutter {

static sk_sp<SkSurface> CreateSnapshotSurface(GrDirectContext* surface_context,
                                              const SkISize& size) {
  const auto image_info = SkImageInfo::MakeN32Premul(
      size.width(), size.height(), SkColorSpace::MakeSRGB());
  if (surface_context) {
    // There is a rendering surface that may contain textures that are going to
    // be referenced in the layer tree about to be drawn.
    return SkSurface::MakeRenderTarget(
        reinterpret_cast<GrRecordingContext*>(surface_context),
        skgpu::Budgeted::kNo, image_info);
  }

  // There is no rendering surface, assume no GPU textures are present and
  // create a raster surface.
  return SkSurface::MakeRaster(image_info);
}

/// Returns a buffer containing a snapshot of the surface.
///
/// If compressed is true the data is encoded as PNG.
static sk_sp<SkData> GetRasterData(const sk_sp<SkSurface>& offscreen_surface,
                                   bool compressed) {
  // Prepare an image from the surface, this image may potentially be on th GPU.
  auto potentially_gpu_snapshot = offscreen_surface->makeImageSnapshot();
  if (!potentially_gpu_snapshot) {
    FML_LOG(ERROR) << "Screenshot: unable to make image screenshot";
    return nullptr;
  }

  // Copy the GPU image snapshot into CPU memory.
  // TODO (https://github.com/flutter/flutter/issues/13498)
  auto cpu_snapshot = potentially_gpu_snapshot->makeRasterImage();
  if (!cpu_snapshot) {
    FML_LOG(ERROR) << "Screenshot: unable to make raster image";
    return nullptr;
  }

  // If the caller want the pixels to be compressed, there is a Skia utility to
  // compress to PNG. Use that.
  if (compressed) {
    return cpu_snapshot->encodeToData();
  }

  // Copy it into a bitmap and return the same.
  SkPixmap pixmap;
  if (!cpu_snapshot->peekPixels(&pixmap)) {
    FML_LOG(ERROR) << "Screenshot: unable to obtain bitmap pixels";
    return nullptr;
  }
  return SkData::MakeWithCopy(pixmap.addr32(), pixmap.computeByteSize());
}

OffscreenSurface::OffscreenSurface(GrDirectContext* surface_context,
                                   const SkISize& size) {
  offscreen_surface_ = CreateSnapshotSurface(surface_context, size);
  if (offscreen_surface_) {
    adapter_.set_canvas(offscreen_surface_->getCanvas());
  }
}

sk_sp<SkData> OffscreenSurface::GetRasterData(bool compressed) const {
  return flutter::GetRasterData(offscreen_surface_, compressed);
}

DlCanvas* OffscreenSurface::GetCanvas() {
  return &adapter_;
}

bool OffscreenSurface::IsValid() const {
  return offscreen_surface_ != nullptr;
}

}  // namespace flutter
