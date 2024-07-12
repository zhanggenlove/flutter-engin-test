// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_surface_vulkan_impeller.h"

#include <memory>
#include <utility>

#include "flutter/fml/concurrent_message_loop.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/memory/ref_ptr.h"
#include "flutter/fml/paths.h"
#include "flutter/impeller/renderer/backend/vulkan/context_vk.h"
#include "flutter/shell/gpu/gpu_surface_vulkan_impeller.h"
#include "flutter/vulkan/vulkan_native_surface_android.h"
#include "impeller/entity/vk/entity_shaders_vk.h"
#include "impeller/entity/vk/modern_shaders_vk.h"
#include "impeller/scene/shaders/vk/scene_shaders_vk.h"

namespace flutter {

static std::shared_ptr<impeller::Context> CreateImpellerContext(
    const fml::RefPtr<vulkan::VulkanProcTable>& proc_table,
    const std::shared_ptr<fml::ConcurrentMessageLoop>& concurrent_loop,
    bool enable_vulkan_validation) {
  std::vector<std::shared_ptr<fml::Mapping>> shader_mappings = {
      std::make_shared<fml::NonOwnedMapping>(impeller_entity_shaders_vk_data,
                                             impeller_entity_shaders_vk_length),
      std::make_shared<fml::NonOwnedMapping>(impeller_scene_shaders_vk_data,
                                             impeller_scene_shaders_vk_length),
      std::make_shared<fml::NonOwnedMapping>(impeller_modern_shaders_vk_data,
                                             impeller_modern_shaders_vk_length),
  };

  PFN_vkGetInstanceProcAddr instance_proc_addr =
      proc_table->NativeGetInstanceProcAddr();

  impeller::ContextVK::Settings settings;
  settings.proc_address_callback = instance_proc_addr;
  settings.shader_libraries_data = std::move(shader_mappings);
  settings.cache_directory = fml::paths::GetCachesDirectory();
  settings.worker_task_runner = concurrent_loop->GetTaskRunner();
  settings.enable_validation = enable_vulkan_validation;
  return impeller::ContextVK::Create(std::move(settings));
}

AndroidSurfaceVulkanImpeller::AndroidSurfaceVulkanImpeller(
    const std::shared_ptr<AndroidContext>& android_context,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    bool enable_vulkan_validation)
    : AndroidSurface(android_context),
      proc_table_(fml::MakeRefCounted<vulkan::VulkanProcTable>()),
      workers_(fml::ConcurrentMessageLoop::Create()) {
  impeller_context_ =
      CreateImpellerContext(proc_table_, workers_, enable_vulkan_validation);
  is_valid_ =
      proc_table_->HasAcquiredMandatoryProcAddresses() && impeller_context_;
}

AndroidSurfaceVulkanImpeller::~AndroidSurfaceVulkanImpeller() = default;

bool AndroidSurfaceVulkanImpeller::IsValid() const {
  return is_valid_;
}

void AndroidSurfaceVulkanImpeller::TeardownOnScreenContext() {
  // Nothing to do.
}

std::unique_ptr<Surface> AndroidSurfaceVulkanImpeller::CreateGPUSurface(
    GrDirectContext* gr_context) {
  if (!IsValid()) {
    return nullptr;
  }

  if (!native_window_ || !native_window_->IsValid()) {
    return nullptr;
  }

  std::unique_ptr<GPUSurfaceVulkanImpeller> gpu_surface =
      std::make_unique<GPUSurfaceVulkanImpeller>(impeller_context_);

  if (!gpu_surface->IsValid()) {
    return nullptr;
  }

  return gpu_surface;
}

bool AndroidSurfaceVulkanImpeller::OnScreenSurfaceResize(const SkISize& size) {
  return true;
}

bool AndroidSurfaceVulkanImpeller::ResourceContextMakeCurrent() {
  return true;
}

bool AndroidSurfaceVulkanImpeller::ResourceContextClearCurrent() {
  return true;
}

bool AndroidSurfaceVulkanImpeller::SetNativeWindow(
    fml::RefPtr<AndroidNativeWindow> window) {
  native_window_ = std::move(window);
  bool success = native_window_ && native_window_->IsValid();
  if (success) {
    auto& context_vk = impeller::ContextVK::Cast(*impeller_context_);
    auto surface = context_vk.CreateAndroidSurface(native_window_->handle());

    if (!surface) {
      FML_LOG(ERROR) << "Could not create a vulkan surface.";
      return false;
    }

    return context_vk.SetWindowSurface(std::move(surface));
  }

  native_window_ = nullptr;
  return false;
}

std::shared_ptr<impeller::Context>
AndroidSurfaceVulkanImpeller::GetImpellerContext() {
  return impeller_context_;
}

}  // namespace flutter
