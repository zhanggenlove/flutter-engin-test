// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/vulkan/capabilities_vk.h"

#include <algorithm>

#include "impeller/base/validation.h"
#include "impeller/renderer/backend/vulkan/vk.h"

namespace impeller {

static constexpr const char* kInstanceLayer = "ImpellerInstance";

CapabilitiesVK::CapabilitiesVK(bool enable_validations)
    : enable_validations_(enable_validations) {
  if (enable_validations_) {
    FML_LOG(INFO) << "Vulkan validations are enabled.";
  }
  auto extensions = vk::enumerateInstanceExtensionProperties();
  auto layers = vk::enumerateInstanceLayerProperties();

  if (extensions.result != vk::Result::eSuccess ||
      layers.result != vk::Result::eSuccess) {
    return;
  }

  for (const auto& ext : extensions.value) {
    exts_[kInstanceLayer].insert(ext.extensionName);
  }

  for (const auto& layer : layers.value) {
    const std::string layer_name = layer.layerName;
    auto layer_exts = vk::enumerateInstanceExtensionProperties(layer_name);
    if (layer_exts.result != vk::Result::eSuccess) {
      return;
    }
    for (const auto& layer_ext : layer_exts.value) {
      exts_[layer_name].insert(layer_ext.extensionName);
    }
  }

  is_valid_ = true;
}

CapabilitiesVK::~CapabilitiesVK() = default;

bool CapabilitiesVK::IsValid() const {
  return is_valid_;
}

bool CapabilitiesVK::AreValidationsEnabled() const {
  return enable_validations_;
}

std::optional<std::vector<std::string>> CapabilitiesVK::GetRequiredLayers()
    const {
  std::vector<std::string> required;

  if (enable_validations_) {
    if (!HasLayer("VK_LAYER_KHRONOS_validation")) {
      VALIDATION_LOG
          << "Requested validations but the validation layer was not found.";
      return std::nullopt;
    }
    required.push_back("VK_LAYER_KHRONOS_validation");
  }

  return required;
}

std::optional<std::vector<std::string>>
CapabilitiesVK::GetRequiredInstanceExtensions() const {
  std::vector<std::string> required;

  if (!HasExtension("VK_KHR_surface")) {
    // Swapchain support is required and this is a dependency of
    // VK_KHR_swapchain.
    VALIDATION_LOG << "Could not find the surface extension.";
    return std::nullopt;
  }
  required.push_back("VK_KHR_surface");

  auto has_wsi = false;
  if (HasExtension("VK_MVK_macos_surface")) {
    required.push_back("VK_MVK_macos_surface");
    has_wsi = true;
  }

  if (HasExtension("VK_EXT_metal_surface")) {
    required.push_back("VK_EXT_metal_surface");
    has_wsi = true;
  }

  if (HasExtension("VK_KHR_portability_enumeration")) {
    required.push_back("VK_KHR_portability_enumeration");
    has_wsi = true;
  }

  if (HasExtension("VK_KHR_win32_surface")) {
    required.push_back("VK_KHR_win32_surface");
    has_wsi = true;
  }

  if (HasExtension("VK_KHR_android_surface")) {
    required.push_back("VK_KHR_android_surface");
    has_wsi = true;
  }

  if (HasExtension("VK_KHR_xcb_surface")) {
    required.push_back("VK_KHR_xcb_surface");
    has_wsi = true;
  }

  if (HasExtension("VK_KHR_xlib_surface")) {
    required.push_back("VK_KHR_xlib_surface");
    has_wsi = true;
  }

  if (HasExtension("VK_KHR_wayland_surface")) {
    required.push_back("VK_KHR_wayland_surface");
    has_wsi = true;
  }

  if (!has_wsi) {
    // Don't really care which WSI extension there is as long there is at least
    // one.
    VALIDATION_LOG << "Could not find a WSI extension.";
    return std::nullopt;
  }

  if (enable_validations_) {
    if (!HasExtension("VK_EXT_debug_utils")) {
      VALIDATION_LOG << "Requested validations but could not find the "
                        "VK_EXT_debug_utils extension.";
      return std::nullopt;
    }
    required.push_back("VK_EXT_debug_utils");

    if (!HasExtension("VK_EXT_validation_features")) {
      VALIDATION_LOG << "Requested validations but could not find the "
                        "VK_EXT_validation_features extension.";
      return std::nullopt;
    }
    required.push_back("VK_EXT_validation_features");
  }

  return required;
}

std::optional<std::vector<std::string>>
CapabilitiesVK::GetRequiredDeviceExtensions(
    const vk::PhysicalDevice& physical_device) const {
  auto device_extensions = physical_device.enumerateDeviceExtensionProperties();
  if (device_extensions.result != vk::Result::eSuccess) {
    return std::nullopt;
  }

  std::set<std::string> exts;
  for (const auto& device_extension : device_extensions.value) {
    exts.insert(device_extension.extensionName);
  }

  std::vector<std::string> required;

  if (exts.find("VK_KHR_swapchain") == exts.end()) {
    VALIDATION_LOG << "Device does not support the swapchain extension.";
    return std::nullopt;
  }
  required.push_back("VK_KHR_swapchain");

  // Required for non-conformant implementations like MoltenVK.
  if (exts.find("VK_KHR_portability_subset") != exts.end()) {
    required.push_back("VK_KHR_portability_subset");
  }
  return required;
}

static bool HasSuitableColorFormat(const vk::PhysicalDevice& device,
                                   vk::Format format) {
  const auto props = device.getFormatProperties(format);
  // This needs to be more comprehensive.
  return !!(props.optimalTilingFeatures &
            vk::FormatFeatureFlagBits::eColorAttachment);
}

static bool HasSuitableDepthStencilFormat(const vk::PhysicalDevice& device,
                                          vk::Format format) {
  const auto props = device.getFormatProperties(format);
  return !!(props.optimalTilingFeatures &
            vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

static bool PhysicalDeviceSupportsRequiredFormats(
    const vk::PhysicalDevice& device) {
  const auto has_color_format =
      HasSuitableColorFormat(device, vk::Format::eB8G8R8A8Unorm);
  const auto has_depth_stencil_format =
      HasSuitableDepthStencilFormat(device, vk::Format::eS8Uint) ||
      HasSuitableDepthStencilFormat(device, vk::Format::eD24UnormS8Uint);
  return has_color_format && has_depth_stencil_format;
}

static bool HasRequiredProperties(const vk::PhysicalDevice& physical_device) {
  auto properties = physical_device.getProperties();
  if (!(properties.limits.framebufferColorSampleCounts &
        (vk::SampleCountFlagBits::e1 | vk::SampleCountFlagBits::e4))) {
    return false;
  }
  return true;
}

static bool HasRequiredQueues(const vk::PhysicalDevice& physical_device) {
  auto queue_flags = vk::QueueFlags{};
  for (const auto& queue : physical_device.getQueueFamilyProperties()) {
    if (queue.queueCount == 0) {
      continue;
    }
    queue_flags |= queue.queueFlags;
  }
  return static_cast<VkQueueFlags>(queue_flags &
                                   (vk::QueueFlagBits::eGraphics |
                                    vk::QueueFlagBits::eCompute |
                                    vk::QueueFlagBits::eTransfer));
}

std::optional<vk::PhysicalDeviceFeatures>
CapabilitiesVK::GetRequiredDeviceFeatures(
    const vk::PhysicalDevice& device) const {
  if (!PhysicalDeviceSupportsRequiredFormats(device)) {
    VALIDATION_LOG << "Device doesn't support the required formats.";
    return std::nullopt;
  }

  if (!HasRequiredProperties(device)) {
    VALIDATION_LOG << "Device doesn't support the required properties.";
    return std::nullopt;
  }

  if (!HasRequiredQueues(device)) {
    VALIDATION_LOG << "Device doesn't support the required queues.";
    return std::nullopt;
  }

  if (!GetRequiredDeviceExtensions(device).has_value()) {
    VALIDATION_LOG << "Device doesn't support the required queues.";
    return std::nullopt;
  }

  const auto device_features = device.getFeatures();

  vk::PhysicalDeviceFeatures required;

  // We require this for enabling wireframes in the playground. But its not
  // necessarily a big deal if we don't have this feature.
  required.fillModeNonSolid = device_features.fillModeNonSolid;

  return required;
}

bool CapabilitiesVK::HasLayer(const std::string& layer) const {
  for (const auto& [found_layer, exts] : exts_) {
    if (found_layer == layer) {
      return true;
    }
  }
  return false;
}

bool CapabilitiesVK::HasExtension(const std::string& ext) const {
  for (const auto& [layer, exts] : exts_) {
    if (exts.find(ext) != exts.end()) {
      return true;
    }
  }
  return false;
}

bool CapabilitiesVK::SetDevice(const vk::PhysicalDevice& device) {
  if (HasSuitableColorFormat(device, vk::Format::eB8G8R8A8Unorm)) {
    color_format_ = PixelFormat::kB8G8R8A8UNormInt;
  } else {
    return false;
  }

  if (HasSuitableDepthStencilFormat(device, vk::Format::eS8Uint)) {
    depth_stencil_format_ = PixelFormat::kS8UInt;
  } else if (HasSuitableDepthStencilFormat(device,
                                           vk::Format::eD24UnormS8Uint)) {
    depth_stencil_format_ = PixelFormat::kD32FloatS8UInt;
  } else {
    return false;
  }

  device_properties_ = device.getProperties();

  return true;
}

// |Capabilities|
bool CapabilitiesVK::HasThreadingRestrictions() const {
  return false;
}

// |Capabilities|
bool CapabilitiesVK::SupportsOffscreenMSAA() const {
  return true;
}

// |Capabilities|
bool CapabilitiesVK::SupportsSSBO() const {
  return true;
}

// |Capabilities|
bool CapabilitiesVK::SupportsTextureToTextureBlits() const {
  return true;
}

// |Capabilities|
bool CapabilitiesVK::SupportsFramebufferFetch() const {
  return false;
}

// |Capabilities|
bool CapabilitiesVK::SupportsCompute() const {
  return false;
}

// |Capabilities|
bool CapabilitiesVK::SupportsComputeSubgroups() const {
  return false;
}

// |Capabilities|
bool CapabilitiesVK::SupportsReadFromResolve() const {
  return false;
}

bool CapabilitiesVK::SupportsDecalTileMode() const {
  return true;
}

// |Capabilities|
PixelFormat CapabilitiesVK::GetDefaultColorFormat() const {
  return color_format_;
}

// |Capabilities|
PixelFormat CapabilitiesVK::GetDefaultStencilFormat() const {
  return depth_stencil_format_;
}

const vk::PhysicalDeviceProperties&
CapabilitiesVK::GetPhysicalDeviceProperties() const {
  return device_properties_;
}

}  // namespace impeller
