// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "impeller/entity/contents/filters/filter_contents.h"

namespace impeller {

class YUVToRGBFilterContents final : public FilterContents {
 public:
  YUVToRGBFilterContents();

  ~YUVToRGBFilterContents() override;

  void SetYUVColorSpace(YUVColorSpace yuv_color_space);

 private:
  // |FilterContents|
  std::optional<Entity> RenderFilter(const FilterInput::Vector& input_textures,
                                     const ContentContext& renderer,
                                     const Entity& entity,
                                     const Matrix& effect_transform,
                                     const Rect& coverage) const override;

  YUVColorSpace yuv_color_space_ = YUVColorSpace::kBT601LimitedRange;

  FML_DISALLOW_COPY_AND_ASSIGN(YUVToRGBFilterContents);
};

}  // namespace impeller
