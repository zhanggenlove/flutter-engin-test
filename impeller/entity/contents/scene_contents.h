// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "impeller/entity/contents/color_source_contents.h"

#include "impeller/geometry/matrix.h"
#include "impeller/geometry/rect.h"
#include "impeller/scene/node.h"

namespace impeller {

class SceneContents final : public ColorSourceContents {
 public:
  SceneContents();

  ~SceneContents() override;

  void SetCameraTransform(Matrix matrix);

  void SetNode(std::shared_ptr<scene::Node> node);

  // |Contents|
  bool Render(const ContentContext& renderer,
              const Entity& entity,
              RenderPass& pass) const override;

 private:
  Matrix camera_transform_;
  std::shared_ptr<scene::Node> node_;
};

}  // namespace impeller
