// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <functional>
#include <memory>

#include "flutter/fml/macros.h"
#include "impeller/renderer/backend/gles/gles.h"
#include "impeller/renderer/context.h"
#include "impeller/renderer/surface.h"

namespace impeller {

class SurfaceGLES final : public Surface {
 public:
  using SwapCallback = std::function<bool(void)>;

  static std::unique_ptr<Surface> WrapFBO(
      const std::shared_ptr<Context>& context,
      SwapCallback swap_callback,
      GLuint fbo,
      PixelFormat color_format,
      ISize fbo_size);

  // |Surface|
  ~SurfaceGLES() override;

 private:
  SwapCallback swap_callback_;

  SurfaceGLES(SwapCallback swap_callback, const RenderTarget& target_desc);

  // |Surface|
  bool Present() const override;

  FML_DISALLOW_COPY_AND_ASSIGN(SurfaceGLES);
};

}  // namespace impeller
