// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/vertex_descriptor.h"

namespace impeller {

VertexDescriptor::VertexDescriptor() = default;

VertexDescriptor::~VertexDescriptor() = default;

bool VertexDescriptor::SetStageInputs(
    const ShaderStageIOSlot* const stage_inputs[],
    size_t count) {
  inputs_.reserve(inputs_.size() + count);
  for (size_t i = 0; i < count; i++) {
    inputs_.emplace_back(*stage_inputs[i]);
  }
  return true;
}

bool VertexDescriptor::RegisterDescriptorSetLayouts(
    const DescriptorSetLayout desc_set_layout[],
    size_t count) {
  desc_set_layouts_.reserve(desc_set_layouts_.size() + count);
  for (size_t i = 0; i < count; i++) {
    desc_set_layouts_.emplace_back(desc_set_layout[i]);
  }
  return true;
}

// |Comparable<VertexDescriptor>|
size_t VertexDescriptor::GetHash() const {
  auto seed = fml::HashCombine();
  for (const auto& input : inputs_) {
    fml::HashCombineSeed(seed, input.GetHash());
  }
  return seed;
}

// |Comparable<VertexDescriptor>|
bool VertexDescriptor::IsEqual(const VertexDescriptor& other) const {
  return inputs_ == other.inputs_;
}

const std::vector<ShaderStageIOSlot>& VertexDescriptor::GetStageInputs() const {
  return inputs_;
}

const std::vector<DescriptorSetLayout>&
VertexDescriptor::GetDescriptorSetLayouts() const {
  return desc_set_layouts_;
}

}  // namespace impeller
