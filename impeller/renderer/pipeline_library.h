// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <optional>

#include "compute_pipeline_descriptor.h"
#include "flutter/fml/macros.h"
#include "impeller/renderer/pipeline.h"
#include "impeller/renderer/pipeline_descriptor.h"

namespace impeller {

class Context;

using PipelineMap = std::unordered_map<PipelineDescriptor,
                                       PipelineFuture<PipelineDescriptor>,
                                       ComparableHash<PipelineDescriptor>,
                                       ComparableEqual<PipelineDescriptor>>;

using ComputePipelineMap =
    std::unordered_map<ComputePipelineDescriptor,
                       PipelineFuture<ComputePipelineDescriptor>,
                       ComparableHash<ComputePipelineDescriptor>,
                       ComparableEqual<ComputePipelineDescriptor>>;

class PipelineLibrary : public std::enable_shared_from_this<PipelineLibrary> {
 public:
  virtual ~PipelineLibrary();

  PipelineFuture<PipelineDescriptor> GetPipeline(
      std::optional<PipelineDescriptor> descriptor);

  PipelineFuture<ComputePipelineDescriptor> GetPipeline(
      std::optional<ComputePipelineDescriptor> descriptor);

  virtual bool IsValid() const = 0;

  virtual PipelineFuture<PipelineDescriptor> GetPipeline(
      PipelineDescriptor descriptor) = 0;

  virtual PipelineFuture<ComputePipelineDescriptor> GetPipeline(
      ComputePipelineDescriptor descriptor) = 0;

  virtual void RemovePipelinesWithEntryPoint(
      std::shared_ptr<const ShaderFunction> function) = 0;

 protected:
  PipelineLibrary();

 private:
  FML_DISALLOW_COPY_AND_ASSIGN(PipelineLibrary);
};

}  // namespace impeller
