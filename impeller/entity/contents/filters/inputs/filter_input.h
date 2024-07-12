// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "impeller/entity/contents/contents.h"
#include "impeller/geometry/rect.h"

namespace impeller {

class ContentContext;
class Entity;
class FilterContents;

/// `FilterInput` is a lazy/single eval `Snapshot` which may be shared across
/// filter parameters and used to evaluate input coverage.
///
/// A `FilterInput` can be re-used for any filter inputs across an entity's
/// filter graph without repeating subpasses unnecessarily.
///
/// Filters may decide to not evaluate inputs in situations where they won't
/// contribute to the filter's output texture.
class FilterInput {
 public:
  using Ref = std::shared_ptr<FilterInput>;
  using Vector = std::vector<FilterInput::Ref>;
  using Variant = std::variant<std::shared_ptr<FilterContents>,
                               std::shared_ptr<Contents>,
                               std::shared_ptr<Texture>>;

  virtual ~FilterInput();

  static FilterInput::Ref Make(Variant input, bool msaa_enabled = true);

  static FilterInput::Ref Make(std::shared_ptr<Texture> input,
                               Matrix local_transform);

  static FilterInput::Vector Make(std::initializer_list<Variant> inputs);

  virtual Variant GetInput() const = 0;

  virtual std::optional<Snapshot> GetSnapshot(const ContentContext& renderer,
                                              const Entity& entity) const = 0;

  std::optional<Rect> GetLocalCoverage(const Entity& entity) const;

  virtual std::optional<Rect> GetCoverage(const Entity& entity) const = 0;

  /// @brief  Get the local transform of this filter input. This transform is
  ///         relative to the `Entity` transform space.
  virtual Matrix GetLocalTransform(const Entity& entity) const;

  /// @brief  Get the transform of this `FilterInput`. This is equivalent to
  ///         calling `entity.GetTransformation() * GetLocalTransform()`.
  virtual Matrix GetTransform(const Entity& entity) const;
};

}  // namespace impeller
