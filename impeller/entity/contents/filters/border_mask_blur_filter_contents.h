// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <optional>
#include "impeller/entity/contents/filters/filter_contents.h"
#include "impeller/entity/contents/filters/inputs/filter_input.h"

namespace impeller {

class BorderMaskBlurFilterContents final : public FilterContents {
 public:
  BorderMaskBlurFilterContents();

  ~BorderMaskBlurFilterContents() override;

  void SetSigma(Sigma sigma_x, Sigma sigma_y);

  void SetBlurStyle(BlurStyle blur_style);

  // |FilterContents|
  std::optional<Rect> GetFilterCoverage(
      const FilterInput::Vector& inputs,
      const Entity& entity,
      const Matrix& effect_transform) const override;

 private:
  // |FilterContents|
  std::optional<Entity> RenderFilter(const FilterInput::Vector& input_textures,
                                     const ContentContext& renderer,
                                     const Entity& entity,
                                     const Matrix& effect_transform,
                                     const Rect& coverage) const override;

  Sigma sigma_x_;
  Sigma sigma_y_;
  BlurStyle blur_style_ = BlurStyle::kNormal;
  bool src_color_factor_ = false;
  bool inner_blur_factor_ = true;
  bool outer_blur_factor_ = true;

  FML_DISALLOW_COPY_AND_ASSIGN(BorderMaskBlurFilterContents);
};

}  // namespace impeller
