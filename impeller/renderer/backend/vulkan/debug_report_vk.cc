// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/vulkan/debug_report_vk.h"

#include "impeller/renderer/backend/vulkan/capabilities_vk.h"

namespace impeller {

DebugReportVK::DebugReportVK(const CapabilitiesVK& caps,
                             const vk::Instance& instance) {
  if (!caps.AreValidationsEnabled()) {
    is_valid_ = true;
    return;
  }

  vk::DebugUtilsMessengerCreateInfoEXT messenger_info;
  messenger_info.messageSeverity =
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
  messenger_info.messageType =
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
  messenger_info.pUserData = this;
  messenger_info.pfnUserCallback = DebugUtilsMessengerCallback;

  auto messenger = instance.createDebugUtilsMessengerEXTUnique(messenger_info);

  if (messenger.result != vk::Result::eSuccess) {
    FML_LOG(ERROR) << "Could not create debug messenger: "
                   << vk::to_string(messenger.result);
    return;
  }

  messenger_ = std::move(messenger.value);
  is_valid_ = true;
}

DebugReportVK::~DebugReportVK() = default;

bool DebugReportVK::IsValid() const {
  return is_valid_;
}

static std::string JoinLabels(const VkDebugUtilsLabelEXT* labels,
                              size_t count) {
  std::stringstream stream;
  for (size_t i = 0u; i < count; i++) {
    stream << labels[i].pLabelName;
    if (i != count - 1u) {
      stream << ", ";
    }
  }
  return stream.str();
}

static std::string JoinVKDebugUtilsObjectNameInfoEXT(
    const VkDebugUtilsObjectNameInfoEXT* names,
    size_t count) {
  std::stringstream stream;
  for (size_t i = 0u; i < count; i++) {
    stream << vk::to_string(vk::ObjectType(names[i].objectType)) << " ["
           << names[i].objectHandle << "] [";
    if (names[i].pObjectName != nullptr) {
      stream << names[i].pObjectName;
    } else {
      stream << "UNNAMED";
    }
    stream << "]";
    if (i != count - 1u) {
      stream << ", ";
    }
  }
  return stream.str();
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportVK::DebugUtilsMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* debug_report) {
  auto result =
      reinterpret_cast<DebugReportVK*>(debug_report)
          ->OnDebugCallback(
              static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(
                  severity),                                         //
              static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(type),  //
              callback_data                                          //
          );
  switch (result) {
    case Result::kContinue:
      return VK_FALSE;
    case Result::kAbort:
      return VK_TRUE;
  }
  return VK_FALSE;
}

DebugReportVK::Result DebugReportVK::OnDebugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data) {
  // Issue in older versions of the SDK.
  // https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/3554
  if (strstr(data->pMessageIdName, "CoreValidation-Shader-OutputNotConsumed") !=
      nullptr) {
    return Result::kContinue;
  }

  // This is a real error but we can't fix it due to our headers being too
  // old. More more details see:
  // https://vulkan.lunarg.com/doc/view/1.3.224.1/mac/1.3-extensions/vkspec.html#VUID-VkImageViewCreateInfo-imageViewFormatSwizzle-04465
  // This validation error currently only trips on macOS due to the use of
  // texture swizzles.
  if (data->messageIdNumber == static_cast<int32_t>(0x82ae5050)) {
    return Result::kContinue;
  }

  std::vector<std::pair<std::string, std::string>> items;

  items.emplace_back("Severity", vk::to_string(severity));

  items.emplace_back("Type", vk::to_string(type));

  if (data->pMessageIdName) {
    items.emplace_back("ID Name", data->pMessageIdName);
  }

  items.emplace_back("ID Number", std::to_string(data->messageIdNumber));

  if (auto queues = JoinLabels(data->pQueueLabels, data->queueLabelCount);
      !queues.empty()) {
    items.emplace_back("Queue Breadcrumbs", std::move(queues));
  } else {
    items.emplace_back("Queue Breadcrumbs", "[NONE]");
  }

  if (auto cmd_bufs = JoinLabels(data->pCmdBufLabels, data->cmdBufLabelCount);
      !cmd_bufs.empty()) {
    items.emplace_back("CMD Buffer Breadcrumbs", std::move(cmd_bufs));
  } else {
    items.emplace_back("CMD Buffer Breadcrumbs", "[NONE]");
  }

  if (auto related =
          JoinVKDebugUtilsObjectNameInfoEXT(data->pObjects, data->objectCount);
      !related.empty()) {
    items.emplace_back("Related Objects", std::move(related));
  }

  if (data->pMessage) {
    items.emplace_back("Trigger", data->pMessage);
  }

  size_t padding = 0;

  for (const auto& item : items) {
    padding = std::max(padding, item.first.size());
  }

  padding += 1;

  std::stringstream stream;

  stream << std::endl;

  stream << "--- Vulkan Debug Report  ----------------------------------------";

  stream << std::endl;

  for (const auto& item : items) {
    stream << "| " << std::setw(static_cast<int>(padding)) << item.first
           << std::setw(0) << ": " << item.second << std::endl;
  }

  stream << "-----------------------------------------------------------------";

  if (type == vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance) {
    FML_LOG(INFO) << stream.str();
  } else {
    VALIDATION_LOG << stream.str();
  }

  return Result::kContinue;
}

}  // namespace impeller
