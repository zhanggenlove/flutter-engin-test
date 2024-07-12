// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <optional>

#include "flutter/fml/macros.h"

#include "impeller/core/allocator.h"
#include "impeller/core/texture.h"
#include "impeller/scene/importer/scene_flatbuffers.h"
#include "impeller/scene/node.h"

namespace impeller {
namespace scene {

class Skin final {
 public:
  static std::unique_ptr<Skin> MakeFromFlatbuffer(
      const fb::Skin& skin,
      const std::vector<std::shared_ptr<Node>>& scene_nodes);
  ~Skin();

  Skin(Skin&&);
  Skin& operator=(Skin&&);

  std::shared_ptr<Texture> GetJointsTexture(Allocator& allocator);

 private:
  Skin();

  std::vector<std::shared_ptr<Node>> joints_;
  std::vector<Matrix> inverse_bind_matrices_;

  FML_DISALLOW_COPY_AND_ASSIGN(Skin);
};

}  // namespace scene
}  // namespace impeller
