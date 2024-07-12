// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "impeller/entity/contents/filters/inputs/filter_input.h"

namespace impeller {

class FilterContentsFilterInput final : public FilterInput {
 public:
  ~FilterContentsFilterInput() override;

  // |FilterInput|
  Variant GetInput() const override;

  // |FilterInput|
  std::optional<Snapshot> GetSnapshot(const ContentContext& renderer,
                                      const Entity& entity) const override;

  // |FilterInput|
  std::optional<Rect> GetCoverage(const Entity& entity) const override;

  // |FilterInput|
  Matrix GetLocalTransform(const Entity& entity) const override;

  // |FilterInput|
  Matrix GetTransform(const Entity& entity) const override;

 private:
  FilterContentsFilterInput(std::shared_ptr<FilterContents> filter);

  std::shared_ptr<FilterContents> filter_;
  mutable std::optional<Snapshot> snapshot_;

  friend FilterInput;
};

}  // namespace impeller
