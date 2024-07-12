// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <functional>
#include <memory>
#include <variant>
#include <vector>

#include "flutter/fml/macros.h"
#include "impeller/entity/contents/contents.h"
#include "impeller/geometry/color.h"
#include "impeller/typographer/glyph_atlas.h"
#include "impeller/typographer/text_frame.h"

namespace impeller {

class LazyGlyphAtlas;
class Context;

class TextContents final : public Contents {
 public:
  TextContents();

  ~TextContents();

  void SetTextFrame(const TextFrame& frame);

  void SetGlyphAtlas(std::shared_ptr<LazyGlyphAtlas> atlas);

  void SetColor(Color color);

  Color GetColor() const;

  bool CanInheritOpacity(const Entity& entity) const override;

  void SetInheritedOpacity(Scalar opacity) override;

  void SetOffset(Vector2 offset);

  std::optional<Rect> GetTextFrameBounds() const;

  // |Contents|
  std::optional<Rect> GetCoverage(const Entity& entity) const override;

  // |Contents|
  bool Render(const ContentContext& renderer,
              const Entity& entity,
              RenderPass& pass) const override;

  // TODO(dnfield): remove this https://github.com/flutter/flutter/issues/111640
  bool RenderSdf(const ContentContext& renderer,
                 const Entity& entity,
                 RenderPass& pass) const;

 private:
  TextFrame frame_;
  Color color_;
  Scalar inherited_opacity_ = 1.0;
  mutable std::shared_ptr<LazyGlyphAtlas> lazy_atlas_;
  Vector2 offset_;

  std::shared_ptr<GlyphAtlas> ResolveAtlas(
      GlyphAtlas::Type type,
      std::shared_ptr<GlyphAtlasContext> atlas_context,
      std::shared_ptr<Context> context) const;

  FML_DISALLOW_COPY_AND_ASSIGN(TextContents);
};

}  // namespace impeller
