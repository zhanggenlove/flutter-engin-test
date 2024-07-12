// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <emscripten.h>
#include "export.h"
#include "helpers.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "wrappers.h"

using namespace Skwasm;

SKWASM_EXPORT SkPictureRecorder* pictureRecorder_create() {
  return new SkPictureRecorder();
}

SKWASM_EXPORT void pictureRecorder_dispose(SkPictureRecorder* recorder) {
  delete recorder;
}

SKWASM_EXPORT CanvasWrapper* pictureRecorder_beginRecording(
    SkPictureRecorder* recorder,
    const SkRect* cullRect) {
  return new CanvasWrapper{0, recorder->beginRecording(*cullRect)};
}

SKWASM_EXPORT SkPicture* pictureRecorder_endRecording(
    SkPictureRecorder* recorder) {
  return recorder->finishRecordingAsPicture().release();
}

SKWASM_EXPORT void picture_dispose(SkPicture* picture) {
  picture->unref();
}

SKWASM_EXPORT uint32_t picture_approximateBytesUsed(SkPicture* picture) {
  return static_cast<uint32_t>(picture->approximateBytesUsed());
}
