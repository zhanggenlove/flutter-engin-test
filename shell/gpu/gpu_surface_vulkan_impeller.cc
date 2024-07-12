// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/gpu/gpu_surface_vulkan_impeller.h"

#include "flutter/fml/make_copyable.h"
#include "flutter/impeller/display_list/display_list_dispatcher.h"
#include "flutter/impeller/renderer/renderer.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"

namespace flutter {

GPUSurfaceVulkanImpeller::GPUSurfaceVulkanImpeller(
    std::shared_ptr<impeller::Context> context)
    : weak_factory_(this) {
  if (!context || !context->IsValid()) {
    return;
  }

  auto renderer = std::make_shared<impeller::Renderer>(context);
  if (!renderer->IsValid()) {
    return;
  }

  auto aiks_context = std::make_shared<impeller::AiksContext>(context);
  if (!aiks_context->IsValid()) {
    return;
  }

  impeller_context_ = std::move(context);
  impeller_renderer_ = std::move(renderer);
  aiks_context_ = std::move(aiks_context);
  is_valid_ = true;
}

// |Surface|
GPUSurfaceVulkanImpeller::~GPUSurfaceVulkanImpeller() = default;

// |Surface|
bool GPUSurfaceVulkanImpeller::IsValid() {
  return is_valid_;
}

// |Surface|
std::unique_ptr<SurfaceFrame> GPUSurfaceVulkanImpeller::AcquireFrame(
    const SkISize& size) {
  if (!IsValid()) {
    FML_LOG(ERROR) << "Vulkan surface was invalid.";
    return nullptr;
  }

  if (size.isEmpty()) {
    FML_LOG(ERROR) << "Vulkan surface was asked for an empty frame.";
    return nullptr;
  }

  auto& context_vk = impeller::ContextVK::Cast(*impeller_context_);
  std::unique_ptr<impeller::Surface> surface = context_vk.AcquireNextSurface();

  SurfaceFrame::SubmitCallback submit_callback =
      fml::MakeCopyable([renderer = impeller_renderer_,  //
                         aiks_context = aiks_context_,   //
                         surface = std::move(surface)    //
  ](SurfaceFrame& surface_frame, DlCanvas* canvas) mutable -> bool {
        if (!aiks_context) {
          return false;
        }

        auto display_list = surface_frame.BuildDisplayList();
        if (!display_list) {
          FML_LOG(ERROR) << "Could not build display list for surface frame.";
          return false;
        }

        impeller::DisplayListDispatcher impeller_dispatcher;
        display_list->Dispatch(impeller_dispatcher);
        auto picture = impeller_dispatcher.EndRecordingAsPicture();

        return renderer->Render(
            std::move(surface),
            fml::MakeCopyable(
                [aiks_context, picture = std::move(picture)](
                    impeller::RenderTarget& render_target) -> bool {
                  return aiks_context->Render(picture, render_target);
                }));
      });

  return std::make_unique<SurfaceFrame>(
      nullptr,                          // surface
      SurfaceFrame::FramebufferInfo{},  // framebuffer info
      submit_callback,                  // submit callback
      size,                             // frame size
      nullptr,                          // context result
      true                              // display list fallback
  );
}

// |Surface|
SkMatrix GPUSurfaceVulkanImpeller::GetRootTransformation() const {
  // This backend does not currently support root surface transformations. Just
  // return identity.
  return {};
}

// |Surface|
GrDirectContext* GPUSurfaceVulkanImpeller::GetContext() {
  // Impeller != Skia.
  return nullptr;
}

// |Surface|
std::unique_ptr<GLContextResult>
GPUSurfaceVulkanImpeller::MakeRenderContextCurrent() {
  // This backend has no such concept.
  return std::make_unique<GLContextDefaultResult>(true);
}

// |Surface|
bool GPUSurfaceVulkanImpeller::EnableRasterCache() const {
  return false;
}

// |Surface|
std::shared_ptr<impeller::AiksContext>
GPUSurfaceVulkanImpeller::GetAiksContext() const {
  return aiks_context_;
}

}  // namespace flutter
