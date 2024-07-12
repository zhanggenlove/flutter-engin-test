// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string>

#include "flutter/fml/build_config.h"
#include "flutter/fml/logging.h"

#if FML_OS_ANDROID
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR 1
#endif  // VK_USE_PLATFORM_ANDROID_KHR
#endif  // FML_OS_ANDROID

#if OS_FUCHSIA
#ifndef VK_USE_PLATFORM_MAGMA_KHR
#define VK_USE_PLATFORM_MAGMA_KHR 1
#endif  // VK_USE_PLATFORM_MAGMA_KHR
#ifndef VK_USE_PLATFORM_FUCHSIA
#define VK_USE_PLATFORM_FUCHSIA 1
#endif  // VK_USE_PLATFORM_FUCHSIA
#endif  // OS_FUCHSIA

#include <vulkan/vulkan.h>

#define VK_CALL_LOG_ERROR(expression)                     \
  ({                                                      \
    __typeof__(expression) _rc = (expression);            \
    if (_rc != VK_SUCCESS) {                              \
      FML_LOG(INFO) << "Vulkan call '" << #expression     \
                    << "' failed with error "             \
                    << vulkan::VulkanResultToString(_rc); \
    }                                                     \
    _rc;                                                  \
  })

namespace vulkan {

std::string VulkanResultToString(VkResult result);

}  // namespace vulkan
