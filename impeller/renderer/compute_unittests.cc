// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/time/time_point.h"
#include "flutter/testing/testing.h"
#include "gmock/gmock.h"
#include "impeller/base/strings.h"
#include "impeller/core/formats.h"
#include "impeller/fixtures/sample.comp.h"
#include "impeller/fixtures/stage1.comp.h"
#include "impeller/fixtures/stage2.comp.h"
#include "impeller/geometry/path.h"
#include "impeller/geometry/path_component.h"
#include "impeller/playground/compute_playground_test.h"
#include "impeller/renderer/command_buffer.h"
#include "impeller/renderer/compute_command.h"
#include "impeller/renderer/compute_pipeline_builder.h"
#include "impeller/renderer/pipeline_library.h"

namespace impeller {
namespace testing {
using ComputeTest = ComputePlaygroundTest;
INSTANTIATE_COMPUTE_SUITE(ComputeTest);

TEST_P(ComputeTest, CanCreateComputePass) {
  using CS = SampleComputeShader;
  auto context = GetContext();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->GetCapabilities()->SupportsCompute());

  using SamplePipelineBuilder = ComputePipelineBuilder<CS>;
  auto pipeline_desc =
      SamplePipelineBuilder::MakeDefaultPipelineDescriptor(*context);
  ASSERT_TRUE(pipeline_desc.has_value());
  auto compute_pipeline =
      context->GetPipelineLibrary()->GetPipeline(pipeline_desc).Get();
  ASSERT_TRUE(compute_pipeline);

  auto cmd_buffer = context->CreateCommandBuffer();
  auto pass = cmd_buffer->CreateComputePass();
  ASSERT_TRUE(pass && pass->IsValid());

  static constexpr size_t kCount = 5;

  pass->SetGridSize(ISize(kCount, 1));
  pass->SetThreadGroupSize(ISize(kCount, 1));

  ComputeCommand cmd;
  cmd.label = "Compute";
  cmd.pipeline = compute_pipeline;

  CS::Info info{.count = kCount};
  CS::Input0<kCount> input_0;
  CS::Input1<kCount> input_1;
  for (size_t i = 0; i < kCount; i++) {
    input_0.elements[i] = Vector4(2.0 + i, 3.0 + i, 4.0 + i, 5.0 * i);
    input_1.elements[i] = Vector4(6.0, 7.0, 8.0, 9.0);
  }

  input_0.fixed_array[1] = IPoint32(2, 2);
  input_1.fixed_array[0] = UintPoint32(3, 3);
  input_0.some_int = 5;
  input_1.some_struct = CS::SomeStruct{.vf = Point(3, 4), .i = 42};

  auto output_buffer = CreateHostVisibleDeviceBuffer<CS::Output<kCount>>(
      context, "Output Buffer");

  CS::BindInfo(cmd, pass->GetTransientsBuffer().EmplaceUniform(info));
  CS::BindInput0(cmd,
                 pass->GetTransientsBuffer().EmplaceStorageBuffer(input_0));
  CS::BindInput1(cmd,
                 pass->GetTransientsBuffer().EmplaceStorageBuffer(input_1));
  CS::BindOutput(cmd, output_buffer->AsBufferView());

  ASSERT_TRUE(pass->AddCommand(std::move(cmd)));
  ASSERT_TRUE(pass->EncodeCommands());

  fml::AutoResetWaitableEvent latch;
  ASSERT_TRUE(
      cmd_buffer->SubmitCommands([&latch, output_buffer, &input_0,
                                  &input_1](CommandBuffer::Status status) {
        EXPECT_EQ(status, CommandBuffer::Status::kCompleted);

        auto view = output_buffer->AsBufferView();
        EXPECT_EQ(view.range.length, sizeof(CS::Output<kCount>));

        CS::Output<kCount>* output =
            reinterpret_cast<CS::Output<kCount>*>(view.contents);
        EXPECT_TRUE(output);
        for (size_t i = 0; i < kCount; i++) {
          Vector4 vector = output->elements[i];
          Vector4 computed = input_0.elements[i] * input_1.elements[i];
          EXPECT_EQ(vector, Vector4(computed.x + 2 + input_1.some_struct.i,
                                    computed.y + 3 + input_1.some_struct.vf.x,
                                    computed.z + 5 + input_1.some_struct.vf.y,
                                    computed.w));
        }
        latch.Signal();
      }));

  latch.Wait();
}

TEST_P(ComputeTest, MultiStageInputAndOutput) {
  using CS1 = Stage1ComputeShader;
  using Stage1PipelineBuilder = ComputePipelineBuilder<CS1>;
  using CS2 = Stage2ComputeShader;
  using Stage2PipelineBuilder = ComputePipelineBuilder<CS2>;

  auto context = GetContext();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->GetCapabilities()->SupportsCompute());

  auto pipeline_desc_1 =
      Stage1PipelineBuilder::MakeDefaultPipelineDescriptor(*context);
  ASSERT_TRUE(pipeline_desc_1.has_value());
  auto compute_pipeline_1 =
      context->GetPipelineLibrary()->GetPipeline(pipeline_desc_1).Get();
  ASSERT_TRUE(compute_pipeline_1);

  auto pipeline_desc_2 =
      Stage2PipelineBuilder::MakeDefaultPipelineDescriptor(*context);
  ASSERT_TRUE(pipeline_desc_2.has_value());
  auto compute_pipeline_2 =
      context->GetPipelineLibrary()->GetPipeline(pipeline_desc_2).Get();
  ASSERT_TRUE(compute_pipeline_2);

  auto cmd_buffer = context->CreateCommandBuffer();
  auto pass = cmd_buffer->CreateComputePass();
  ASSERT_TRUE(pass && pass->IsValid());

  static constexpr size_t kCount1 = 5;
  static constexpr size_t kCount2 = kCount1 * 2;

  pass->SetGridSize(ISize(512, 1));
  pass->SetThreadGroupSize(ISize(512, 1));

  CS1::Input<kCount1> input_1;
  input_1.count = kCount1;
  for (size_t i = 0; i < kCount1; i++) {
    input_1.elements[i] = i;
  }

  CS2::Input<kCount2> input_2;
  input_2.count = kCount2;
  for (size_t i = 0; i < kCount2; i++) {
    input_2.elements[i] = i;
  }

  auto output_buffer_1 = CreateHostVisibleDeviceBuffer<CS1::Output<kCount2>>(
      context, "Output Buffer Stage 1");
  auto output_buffer_2 = CreateHostVisibleDeviceBuffer<CS2::Output<kCount2>>(
      context, "Output Buffer Stage 2");

  {
    ComputeCommand cmd;
    cmd.label = "Compute1";
    cmd.pipeline = compute_pipeline_1;

    CS1::BindInput(cmd,
                   pass->GetTransientsBuffer().EmplaceStorageBuffer(input_1));
    CS1::BindOutput(cmd, output_buffer_1->AsBufferView());

    ASSERT_TRUE(pass->AddCommand(std::move(cmd)));
  }

  {
    ComputeCommand cmd;
    cmd.label = "Compute2";
    cmd.pipeline = compute_pipeline_2;

    CS1::BindInput(cmd, output_buffer_1->AsBufferView());
    CS2::BindOutput(cmd, output_buffer_2->AsBufferView());
    ASSERT_TRUE(pass->AddCommand(std::move(cmd)));
  }

  ASSERT_TRUE(pass->EncodeCommands());

  fml::AutoResetWaitableEvent latch;
  ASSERT_TRUE(cmd_buffer->SubmitCommands([&latch, &output_buffer_1,
                                          &output_buffer_2](
                                             CommandBuffer::Status status) {
    EXPECT_EQ(status, CommandBuffer::Status::kCompleted);

    CS1::Output<kCount2>* output_1 = reinterpret_cast<CS1::Output<kCount2>*>(
        output_buffer_1->AsBufferView().contents);
    EXPECT_TRUE(output_1);
    EXPECT_EQ(output_1->count, 10u);
    EXPECT_THAT(output_1->elements,
                ::testing::ElementsAre(0, 0, 2, 3, 4, 6, 6, 9, 8, 12));

    CS2::Output<kCount2>* output_2 = reinterpret_cast<CS2::Output<kCount2>*>(
        output_buffer_2->AsBufferView().contents);
    EXPECT_TRUE(output_2);
    EXPECT_EQ(output_2->count, 10u);
    EXPECT_THAT(output_2->elements,
                ::testing::ElementsAre(0, 0, 4, 6, 8, 12, 12, 18, 16, 24));

    latch.Signal();
  }));

  latch.Wait();
}

TEST_P(ComputeTest, CanCorrectlyDownScaleLargeGridSize) {
  using CS = SampleComputeShader;
  auto context = GetContext();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->GetCapabilities()->SupportsCompute());

  using SamplePipelineBuilder = ComputePipelineBuilder<CS>;
  auto pipeline_desc =
      SamplePipelineBuilder::MakeDefaultPipelineDescriptor(*context);
  ASSERT_TRUE(pipeline_desc.has_value());
  auto compute_pipeline =
      context->GetPipelineLibrary()->GetPipeline(pipeline_desc).Get();
  ASSERT_TRUE(compute_pipeline);

  auto cmd_buffer = context->CreateCommandBuffer();
  auto pass = cmd_buffer->CreateComputePass();
  ASSERT_TRUE(pass && pass->IsValid());

  static constexpr size_t kCount = 5;

  // Intentionally making the grid size obscenely large. No GPU will tolerate
  // this.
  pass->SetGridSize(ISize(std::numeric_limits<int64_t>::max(), 1));
  pass->SetThreadGroupSize(ISize(std::numeric_limits<int64_t>::max(), 1));

  ComputeCommand cmd;
  cmd.label = "Compute";
  cmd.pipeline = compute_pipeline;

  CS::Info info{.count = kCount};
  CS::Input0<kCount> input_0;
  CS::Input1<kCount> input_1;
  for (size_t i = 0; i < kCount; i++) {
    input_0.elements[i] = Vector4(2.0 + i, 3.0 + i, 4.0 + i, 5.0 * i);
    input_1.elements[i] = Vector4(6.0, 7.0, 8.0, 9.0);
  }

  input_0.fixed_array[1] = IPoint32(2, 2);
  input_1.fixed_array[0] = UintPoint32(3, 3);
  input_0.some_int = 5;
  input_1.some_struct = CS::SomeStruct{.vf = Point(3, 4), .i = 42};

  auto output_buffer = CreateHostVisibleDeviceBuffer<CS::Output<kCount>>(
      context, "Output Buffer");

  CS::BindInfo(cmd, pass->GetTransientsBuffer().EmplaceUniform(info));
  CS::BindInput0(cmd,
                 pass->GetTransientsBuffer().EmplaceStorageBuffer(input_0));
  CS::BindInput1(cmd,
                 pass->GetTransientsBuffer().EmplaceStorageBuffer(input_1));
  CS::BindOutput(cmd, output_buffer->AsBufferView());

  ASSERT_TRUE(pass->AddCommand(std::move(cmd)));
  ASSERT_TRUE(pass->EncodeCommands());

  fml::AutoResetWaitableEvent latch;
  ASSERT_TRUE(
      cmd_buffer->SubmitCommands([&latch, output_buffer, &input_0,
                                  &input_1](CommandBuffer::Status status) {
        EXPECT_EQ(status, CommandBuffer::Status::kCompleted);

        auto view = output_buffer->AsBufferView();
        EXPECT_EQ(view.range.length, sizeof(CS::Output<kCount>));

        CS::Output<kCount>* output =
            reinterpret_cast<CS::Output<kCount>*>(view.contents);
        EXPECT_TRUE(output);
        for (size_t i = 0; i < kCount; i++) {
          Vector4 vector = output->elements[i];
          Vector4 computed = input_0.elements[i] * input_1.elements[i];
          EXPECT_EQ(vector, Vector4(computed.x + 2 + input_1.some_struct.i,
                                    computed.y + 3 + input_1.some_struct.vf.x,
                                    computed.z + 5 + input_1.some_struct.vf.y,
                                    computed.w));
        }
        latch.Signal();
      }));

  latch.Wait();
}

TEST_P(ComputeTest, ReturnsEarlyWhenAnyGridDimensionIsZero) {
  using CS = SampleComputeShader;
  auto context = GetContext();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->GetCapabilities()->SupportsCompute());

  using SamplePipelineBuilder = ComputePipelineBuilder<CS>;
  auto pipeline_desc =
      SamplePipelineBuilder::MakeDefaultPipelineDescriptor(*context);
  ASSERT_TRUE(pipeline_desc.has_value());
  auto compute_pipeline =
      context->GetPipelineLibrary()->GetPipeline(pipeline_desc).Get();
  ASSERT_TRUE(compute_pipeline);

  auto cmd_buffer = context->CreateCommandBuffer();
  auto pass = cmd_buffer->CreateComputePass();
  ASSERT_TRUE(pass && pass->IsValid());

  static constexpr size_t kCount = 5;

  // Intentionally making the grid size obscenely large. No GPU will tolerate
  // this.
  pass->SetGridSize(ISize(0, 1));
  pass->SetThreadGroupSize(ISize(0, 1));

  ComputeCommand cmd;
  cmd.label = "Compute";
  cmd.pipeline = compute_pipeline;

  CS::Info info{.count = kCount};
  CS::Input0<kCount> input_0;
  CS::Input1<kCount> input_1;
  for (size_t i = 0; i < kCount; i++) {
    input_0.elements[i] = Vector4(2.0 + i, 3.0 + i, 4.0 + i, 5.0 * i);
    input_1.elements[i] = Vector4(6.0, 7.0, 8.0, 9.0);
  }

  input_0.fixed_array[1] = IPoint32(2, 2);
  input_1.fixed_array[0] = UintPoint32(3, 3);
  input_0.some_int = 5;
  input_1.some_struct = CS::SomeStruct{.vf = Point(3, 4), .i = 42};

  auto output_buffer = CreateHostVisibleDeviceBuffer<CS::Output<kCount>>(
      context, "Output Buffer");

  CS::BindInfo(cmd, pass->GetTransientsBuffer().EmplaceUniform(info));
  CS::BindInput0(cmd,
                 pass->GetTransientsBuffer().EmplaceStorageBuffer(input_0));
  CS::BindInput1(cmd,
                 pass->GetTransientsBuffer().EmplaceStorageBuffer(input_1));
  CS::BindOutput(cmd, output_buffer->AsBufferView());

  ASSERT_TRUE(pass->AddCommand(std::move(cmd)));
  ASSERT_FALSE(pass->EncodeCommands());
}

}  // namespace testing
}  // namespace impeller
