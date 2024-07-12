// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "flutter/fml/macros.h"
#include "impeller/renderer/backend/vulkan/vk.h"

namespace impeller {

class CapabilitiesVK;

class DebugReportVK {
 public:
  DebugReportVK(const CapabilitiesVK& caps, const vk::Instance& instance);

  ~DebugReportVK();

  bool IsValid() const;

 private:
  vk::UniqueDebugUtilsMessengerEXT messenger_;
  bool is_valid_ = false;

  enum class Result {
    kContinue,
    kAbort,
  };

  Result OnDebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                         vk::DebugUtilsMessageTypeFlagsEXT type,
                         const VkDebugUtilsMessengerCallbackDataEXT* data);

  static VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT severity,
      VkDebugUtilsMessageTypeFlagsEXT type,
      const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
      void* user_data);

  FML_DISALLOW_COPY_AND_ASSIGN(DebugReportVK);
};

}  // namespace impeller
