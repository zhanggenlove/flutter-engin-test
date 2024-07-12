// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "texture_contents.h"

#include <memory>
#include <optional>
#include <utility>

#include "impeller/core/formats.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/entity.h"
#include "impeller/entity/texture_fill.frag.h"
#include "impeller/entity/texture_fill.vert.h"
#include "impeller/geometry/constants.h"
#include "impeller/geometry/path_builder.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/sampler_library.h"

namespace impeller {

TextureContents::TextureContents() = default;

TextureContents::~TextureContents() = default;

std::shared_ptr<TextureContents> TextureContents::MakeRect(Rect destination) {
  auto contents = std::make_shared<TextureContents>();
  contents->rect_ = destination;
  return contents;
}

void TextureContents::SetLabel(std::string label) {
  label_ = std::move(label);
}

void TextureContents::SetRect(Rect rect) {
  rect_ = rect;
}

void TextureContents::SetTexture(std::shared_ptr<Texture> texture) {
  texture_ = std::move(texture);
}

std::shared_ptr<Texture> TextureContents::GetTexture() const {
  return texture_;
}

void TextureContents::SetOpacity(Scalar opacity) {
  opacity_ = opacity;
}

void TextureContents::SetStencilEnabled(bool enabled) {
  stencil_enabled_ = enabled;
}

bool TextureContents::CanInheritOpacity(const Entity& entity) const {
  return true;
}

void TextureContents::SetInheritedOpacity(Scalar opacity) {
  inherited_opacity_ = opacity;
}

Scalar TextureContents::GetOpacity() const {
  return opacity_ * inherited_opacity_;
}

std::optional<Rect> TextureContents::GetCoverage(const Entity& entity) const {
  if (GetOpacity() == 0) {
    return std::nullopt;
  }
  return rect_.TransformBounds(entity.GetTransformation());
};

std::optional<Snapshot> TextureContents::RenderToSnapshot(
    const ContentContext& renderer,
    const Entity& entity,
    const std::optional<SamplerDescriptor>& sampler_descriptor,
    bool msaa_enabled) const {
  // Passthrough textures that have simple rectangle paths and complete source
  // rects.
  auto bounds = rect_;
  auto opacity = GetOpacity();
  if (source_rect_ == Rect::MakeSize(texture_->GetSize()) &&
      (opacity >= 1 - kEhCloseEnough || defer_applying_opacity_)) {
    auto scale = Vector2(bounds.size / Size(texture_->GetSize()));
    return Snapshot{
        .texture = texture_,
        .transform = entity.GetTransformation() *
                     Matrix::MakeTranslation(bounds.origin) *
                     Matrix::MakeScale(scale),
        .sampler_descriptor = sampler_descriptor.value_or(sampler_descriptor_),
        .opacity = opacity};
  }
  return Contents::RenderToSnapshot(
      renderer, entity, sampler_descriptor.value_or(sampler_descriptor_));
}

static TextureFillVertexShader::PerVertexData ComputeVertexData(
    const Point& vtx,
    const Rect& coverage_rect,
    const ISize& texture_size,
    const Rect& source_rect) {
  TextureFillVertexShader::PerVertexData data;
  data.position = vtx;
  auto coverage_coords = (vtx - coverage_rect.origin) / coverage_rect.size;
  data.texture_coords =
      (source_rect.origin + source_rect.size * coverage_coords) / texture_size;
  return data;
}

bool TextureContents::Render(const ContentContext& renderer,
                             const Entity& entity,
                             RenderPass& pass) const {
  using VS = TextureFillVertexShader;
  using FS = TextureFillFragmentShader;

  const auto coverage_rect = rect_;

  if (coverage_rect.size.IsEmpty() || source_rect_.IsEmpty() ||
      texture_ == nullptr || texture_->GetSize().IsEmpty()) {
    return true;
  }

  VertexBufferBuilder<VS::PerVertexData> vertex_builder;
  for (const auto vtx : rect_.GetPoints()) {
    vertex_builder.AppendVertex(ComputeVertexData(
        vtx, coverage_rect, texture_->GetSize(), source_rect_));
  }

  auto& host_buffer = pass.GetTransientsBuffer();

  VS::FrameInfo frame_info;
  frame_info.mvp = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                   entity.GetTransformation();
  frame_info.texture_sampler_y_coord_scale = texture_->GetYCoordScale();

  FS::FragInfo frag_info;
  frag_info.alpha = GetOpacity();

  Command cmd;
  cmd.label = "Texture Fill";
  if (!label_.empty()) {
    cmd.label += ": " + label_;
  }

  auto pipeline_options = OptionsFromPassAndEntity(pass, entity);
  if (!stencil_enabled_) {
    pipeline_options.stencil_compare = CompareFunction::kAlways;
  }
  pipeline_options.primitive_type = PrimitiveType::kTriangleStrip;

  cmd.pipeline = renderer.GetTexturePipeline(pipeline_options);
  cmd.stencil_reference = entity.GetStencilDepth();
  cmd.BindVertices(vertex_builder.CreateVertexBuffer(host_buffer));
  VS::BindFrameInfo(cmd, host_buffer.EmplaceUniform(frame_info));
  FS::BindFragInfo(cmd, host_buffer.EmplaceUniform(frag_info));
  FS::BindTextureSampler(cmd, texture_,
                         renderer.GetContext()->GetSamplerLibrary()->GetSampler(
                             sampler_descriptor_));
  pass.AddCommand(std::move(cmd));

  return true;
}

void TextureContents::SetSourceRect(const Rect& source_rect) {
  source_rect_ = source_rect;
}

const Rect& TextureContents::GetSourceRect() const {
  return source_rect_;
}

void TextureContents::SetSamplerDescriptor(SamplerDescriptor desc) {
  sampler_descriptor_ = std::move(desc);
}

const SamplerDescriptor& TextureContents::GetSamplerDescriptor() const {
  return sampler_descriptor_;
}

void TextureContents::SetDeferApplyingOpacity(bool defer_applying_opacity) {
  defer_applying_opacity_ = defer_applying_opacity;
}

}  // namespace impeller
