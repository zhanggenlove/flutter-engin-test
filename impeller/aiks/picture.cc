// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/aiks/picture.h"

#include <memory>
#include <optional>

#include "impeller/base/validation.h"
#include "impeller/entity/entity.h"
#include "impeller/renderer/render_target.h"
#include "impeller/renderer/snapshot.h"

namespace impeller {

std::optional<Snapshot> Picture::Snapshot(AiksContext& context) {
  auto coverage = pass->GetElementsCoverage(std::nullopt);
  if (!coverage.has_value() || coverage->IsEmpty()) {
    return std::nullopt;
  }

  const auto translate = Matrix::MakeTranslation(-coverage.value().origin);
  auto texture =
      RenderToTexture(context, ISize(coverage.value().size), translate);
  return impeller::Snapshot{
      .texture = std::move(texture),
      .transform = Matrix::MakeTranslation(coverage.value().origin)};
}

std::shared_ptr<Image> Picture::ToImage(AiksContext& context,
                                        ISize size) const {
  if (size.IsEmpty()) {
    return nullptr;
  }
  auto texture = RenderToTexture(context, size);
  return texture ? std::make_shared<Image>(texture) : nullptr;
}

std::shared_ptr<Texture> Picture::RenderToTexture(
    AiksContext& context,
    ISize size,
    std::optional<const Matrix> translate) const {
  FML_DCHECK(!size.IsEmpty());

  pass->IterateAllEntities([&translate](auto& entity) -> bool {
    auto matrix = translate.has_value()
                      ? translate.value() * entity.GetTransformation()
                      : entity.GetTransformation();
    entity.SetTransformation(matrix);
    return true;
  });

  // This texture isn't host visible, but we might want to add host visible
  // features to Image someday.
  auto impeller_context = context.GetContext();
  RenderTarget target;
  if (impeller_context->GetCapabilities()->SupportsOffscreenMSAA()) {
    target = RenderTarget::CreateOffscreenMSAA(*impeller_context, size);
  } else {
    target = RenderTarget::CreateOffscreen(*impeller_context, size);
  }
  if (!target.IsValid()) {
    VALIDATION_LOG << "Could not create valid RenderTarget.";
    return nullptr;
  }

  if (!context.Render(*this, target)) {
    VALIDATION_LOG << "Could not render Picture to Texture.";
    return nullptr;
  }

  auto texture = target.GetRenderTargetTexture();
  if (!texture) {
    VALIDATION_LOG << "RenderTarget has no target texture.";
    return nullptr;
  }

  return texture;
}

}  // namespace impeller
