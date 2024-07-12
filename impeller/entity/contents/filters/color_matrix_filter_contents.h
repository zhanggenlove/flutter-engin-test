// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <optional>

#include "impeller/entity/contents/filters/color_filter_contents.h"
#include "impeller/entity/contents/filters/inputs/filter_input.h"

namespace impeller {

// Look at example at: https://github.com/flutter/impeller/pull/132

class ColorMatrixFilterContents final : public ColorFilterContents {
 public:
  ColorMatrixFilterContents();

  ~ColorMatrixFilterContents() override;

  void SetMatrix(const ColorMatrix& matrix);

 private:
  // |FilterContents|
  std::optional<Entity> RenderFilter(const FilterInput::Vector& input_textures,
                                     const ContentContext& renderer,
                                     const Entity& entity,
                                     const Matrix& effect_transform,
                                     const Rect& coverage) const override;

  ColorMatrix matrix_;

  FML_DISALLOW_COPY_AND_ASSIGN(ColorMatrixFilterContents);
};

}  // namespace impeller
