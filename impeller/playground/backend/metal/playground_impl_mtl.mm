// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/playground/backend/metal/playground_impl_mtl.h"

#define GLFW_INCLUDE_NONE
#import "third_party/glfw/include/GLFW/glfw3.h"

#define GLFW_EXPOSE_NATIVE_COCOA
#import "third_party/glfw/include/GLFW/glfw3native.h"

#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

#include "flutter/fml/mapping.h"
#include "impeller/entity/mtl/entity_shaders.h"
#include "impeller/entity/mtl/framebuffer_blend_shaders.h"
#include "impeller/entity/mtl/modern_shaders.h"
#include "impeller/fixtures/mtl/fixtures_shaders.h"
#include "impeller/fixtures/mtl/subgroup_fixtures_shaders.h"
#include "impeller/playground/imgui/mtl/imgui_shaders.h"
#include "impeller/renderer/backend/metal/context_mtl.h"
#include "impeller/renderer/backend/metal/formats_mtl.h"
#include "impeller/renderer/backend/metal/surface_mtl.h"
#include "impeller/renderer/backend/metal/texture_mtl.h"
#include "impeller/renderer/mtl/compute_shaders.h"
#include "impeller/scene/shaders/mtl/scene_shaders.h"

namespace impeller {

struct PlaygroundImplMTL::Data {
  CAMetalLayer* metal_layer = nil;
};

static std::vector<std::shared_ptr<fml::Mapping>>
ShaderLibraryMappingsForPlayground() {
  return {std::make_shared<fml::NonOwnedMapping>(
              impeller_entity_shaders_data, impeller_entity_shaders_length),
          std::make_shared<fml::NonOwnedMapping>(
              impeller_modern_shaders_data, impeller_modern_shaders_length),
          std::make_shared<fml::NonOwnedMapping>(
              impeller_framebuffer_blend_shaders_data,
              impeller_framebuffer_blend_shaders_length),
          std::make_shared<fml::NonOwnedMapping>(
              impeller_fixtures_shaders_data, impeller_fixtures_shaders_length),
          std::make_shared<fml::NonOwnedMapping>(
              impeller_subgroup_fixtures_shaders_data,
              impeller_subgroup_fixtures_shaders_length),
          std::make_shared<fml::NonOwnedMapping>(impeller_imgui_shaders_data,
                                                 impeller_imgui_shaders_length),
          std::make_shared<fml::NonOwnedMapping>(impeller_scene_shaders_data,
                                                 impeller_scene_shaders_length),
          std::make_shared<fml::NonOwnedMapping>(
              impeller_compute_shaders_data, impeller_compute_shaders_length)

  };
}

void PlaygroundImplMTL::DestroyWindowHandle(WindowHandle handle) {
  if (!handle) {
    return;
  }
  ::glfwDestroyWindow(reinterpret_cast<GLFWwindow*>(handle));
}

PlaygroundImplMTL::PlaygroundImplMTL(PlaygroundSwitches switches)
    : PlaygroundImpl(switches),
      handle_(nullptr, &DestroyWindowHandle),
      data_(std::make_unique<Data>()) {
  ::glfwDefaultWindowHints();
  ::glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  ::glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  auto window = ::glfwCreateWindow(1, 1, "Test", nullptr, nullptr);
  if (!window) {
    return;
  }
  auto context = ContextMTL::Create(ShaderLibraryMappingsForPlayground(),
                                    "Playground Library");
  if (!context) {
    return;
  }
  NSWindow* cocoa_window = ::glfwGetCocoaWindow(window);
  if (cocoa_window == nil) {
    return;
  }
  data_->metal_layer = [CAMetalLayer layer];
  data_->metal_layer.device = ContextMTL::Cast(*context).GetMTLDevice();
  data_->metal_layer.pixelFormat =
      ToMTLPixelFormat(context->GetCapabilities()->GetDefaultColorFormat());
  data_->metal_layer.framebufferOnly = NO;
  cocoa_window.contentView.layer = data_->metal_layer;
  cocoa_window.contentView.wantsLayer = YES;

  handle_.reset(window);
  context_ = std::move(context);
}

PlaygroundImplMTL::~PlaygroundImplMTL() = default;

std::shared_ptr<Context> PlaygroundImplMTL::GetContext() const {
  return context_;
}

// |PlaygroundImpl|
PlaygroundImpl::WindowHandle PlaygroundImplMTL::GetWindowHandle() const {
  return handle_.get();
}

// |PlaygroundImpl|
std::unique_ptr<Surface> PlaygroundImplMTL::AcquireSurfaceFrame(
    std::shared_ptr<Context> context) {
  if (!data_->metal_layer) {
    return nullptr;
  }

  const auto layer_size = data_->metal_layer.bounds.size;
  const auto scale = GetContentScale();
  data_->metal_layer.drawableSize =
      CGSizeMake(layer_size.width * scale.x, layer_size.height * scale.y);

  return SurfaceMTL::WrapCurrentMetalLayerDrawable(context, data_->metal_layer);
}

}  // namespace impeller
