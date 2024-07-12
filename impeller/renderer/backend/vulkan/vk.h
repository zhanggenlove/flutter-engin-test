// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "flutter/fml/build_config.h"
#include "flutter/fml/logging.h"
#include "impeller/base/validation.h"

#define VK_NO_PROTOTYPES

#if FML_OS_IOS

// #ifndef VK_USE_PLATFORM_IOS_MVK
// #define VK_USE_PLATFORM_IOS_MVK
// #endif  // VK_USE_PLATFORM_IOS_MVK

#ifndef VK_USE_PLATFORM_METAL_EXT
#define VK_USE_PLATFORM_METAL_EXT
#endif  // VK_USE_PLATFORM_METAL_EXT

#elif FML_OS_MACOSX

// #ifndef VK_USE_PLATFORM_MACOS_MVK
// #define VK_USE_PLATFORM_MACOS_MVK
// #endif  // VK_USE_PLATFORM_MACOS_MVK

#ifndef VK_USE_PLATFORM_METAL_EXT
#define VK_USE_PLATFORM_METAL_EXT
#endif  // VK_USE_PLATFORM_METAL_EXT

#elif FML_OS_ANDROID

#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif  // VK_USE_PLATFORM_ANDROID_KHR

#elif FML_OS_LINUX

// Nothing for now.

#elif FML_OS_WIN

#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif  // VK_USE_PLATFORM_WIN32_KHR

#elif OS_FUCHSIA

#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif  // VK_USE_PLATFORM_ANDROID_KHR

#endif  // FML_OS

#if !defined(NDEBUG)
#define VULKAN_HPP_ASSERT FML_CHECK
#else
#define VULKAN_HPP_ASSERT(ignored) \
  {}
#endif

#define VULKAN_HPP_NAMESPACE impeller::vk
#define VULKAN_HPP_ASSERT_ON_RESULT(ignored) \
  { [[maybe_unused]] auto res = (ignored); }
#include "vulkan/vulkan.hpp"

static_assert(VK_HEADER_VERSION >= 215, "Vulkan headers must not be too old.");

#include "flutter/flutter_vma/flutter_vma.h"
