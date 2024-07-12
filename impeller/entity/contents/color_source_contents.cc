// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/color_source_contents.h"

#include "impeller/entity/entity.h"
#include "impeller/geometry/matrix.h"
#include "impeller/geometry/point.h"

namespace impeller {

ColorSourceContents::ColorSourceContents() = default;

ColorSourceContents::~ColorSourceContents() = default;

void ColorSourceContents::SetGeometry(std::shared_ptr<Geometry> geometry) {
  geometry_ = std::move(geometry);
}

const std::shared_ptr<Geometry>& ColorSourceContents::GetGeometry() const {
  return geometry_;
}

void ColorSourceContents::SetOpacity(Scalar alpha) {
  opacity_ = alpha;
}

Scalar ColorSourceContents::GetOpacity() const {
  return opacity_ * inherited_opacity_;
}

void ColorSourceContents::SetEffectTransform(Matrix matrix) {
  inverse_matrix_ = matrix.Invert();
}

const Matrix& ColorSourceContents::GetInverseMatrix() const {
  return inverse_matrix_;
}

std::optional<Rect> ColorSourceContents::GetCoverage(
    const Entity& entity) const {
  return geometry_->GetCoverage(entity.GetTransformation());
};

bool ColorSourceContents::CanInheritOpacity(const Entity& entity) const {
  return true;
}

void ColorSourceContents::SetInheritedOpacity(Scalar opacity) {
  inherited_opacity_ = opacity;
}

bool ColorSourceContents::ShouldRender(
    const Entity& entity,
    const std::optional<Rect>& stencil_coverage) const {
  if (!stencil_coverage.has_value()) {
    return false;
  }
  return Contents::ShouldRender(entity, stencil_coverage);
}

}  // namespace impeller
