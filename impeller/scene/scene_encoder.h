// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "flutter/fml/macros.h"
#include "impeller/renderer/command_buffer.h"
#include "impeller/scene/camera.h"
#include "impeller/scene/geometry.h"
#include "impeller/scene/material.h"

namespace impeller {
namespace scene {

class Scene;

struct SceneCommand {
  std::string label;
  Matrix transform;
  Geometry* geometry;
  Material* material;
};

class SceneEncoder {
 public:
  void Add(const SceneCommand& command);

 private:
  SceneEncoder();

  std::shared_ptr<CommandBuffer> BuildSceneCommandBuffer(
      const SceneContext& scene_context,
      const Matrix& camera_transform,
      RenderTarget render_target) const;

  std::vector<SceneCommand> commands_;

  friend Scene;

  FML_DISALLOW_COPY_AND_ASSIGN(SceneEncoder);
};

}  // namespace scene
}  // namespace impeller
