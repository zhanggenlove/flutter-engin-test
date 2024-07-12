// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/picture_recorder.h"

#include "flutter/lib/ui/painting/canvas.h"
#include "flutter/lib/ui/painting/picture.h"
#include "third_party/tonic/converter/dart_converter.h"
#include "third_party/tonic/dart_args.h"
#include "third_party/tonic/dart_binding_macros.h"
#include "third_party/tonic/dart_library_natives.h"

namespace flutter {

IMPLEMENT_WRAPPERTYPEINFO(ui, PictureRecorder);

void PictureRecorder::Create(Dart_Handle wrapper) {
  UIDartState::ThrowIfUIOperationsProhibited();
  auto res = fml::MakeRefCounted<PictureRecorder>();
  res->AssociateWithDartWrapper(wrapper);
}

PictureRecorder::PictureRecorder() {}

PictureRecorder::~PictureRecorder() {}

sk_sp<DisplayListBuilder> PictureRecorder::BeginRecording(SkRect bounds) {
  display_list_builder_ =
      sk_make_sp<DisplayListBuilder>(bounds, /*prepare_rtree=*/true);
  return display_list_builder_;
}

fml::RefPtr<Picture> PictureRecorder::endRecording(Dart_Handle dart_picture) {
  if (!canvas_) {
    return nullptr;
  }

  fml::RefPtr<Picture> picture;

  picture = Picture::Create(dart_picture, UIDartState::CreateGPUObject(
                                              display_list_builder_->Build()));
  display_list_builder_ = nullptr;

  canvas_->Invalidate();
  canvas_ = nullptr;
  ClearDartWrapper();
  return picture;
}

}  // namespace flutter
