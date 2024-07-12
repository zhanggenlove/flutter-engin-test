// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <condition_variable>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>

#include "flutter/fml/closure.h"
#include "flutter/fml/macros.h"
#include "impeller/base/thread.h"
#include "impeller/renderer/backend/vulkan/shared_object_vk.h"
#include "impeller/renderer/backend/vulkan/vk.h"

namespace impeller {

class ContextVK;

class FenceWaiterVK {
 public:
  ~FenceWaiterVK();

  bool IsValid() const;

  void Terminate();

  bool AddFence(vk::UniqueFence fence, const fml::closure& callback);

 private:
  friend class ContextVK;

  const vk::Device device_;
  std::unique_ptr<std::thread> waiter_thread_;
  std::mutex wait_set_mutex_;
  std::condition_variable wait_set_cv_;
  std::unordered_map<SharedHandleVK<vk::Fence>, fml::closure> wait_set_;
  bool terminate_ = false;
  bool is_valid_ = false;

  explicit FenceWaiterVK(vk::Device device);

  void Main();

  std::optional<std::vector<vk::Fence>> TrimAndCreateWaitSetLocked();

  FML_DISALLOW_COPY_AND_ASSIGN(FenceWaiterVK);
};

}  // namespace impeller
