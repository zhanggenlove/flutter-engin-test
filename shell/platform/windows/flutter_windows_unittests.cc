// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/windows/public/flutter_windows.h"

#include <dxgi.h>
#include <wrl/client.h>
#include <thread>

#include "flutter/fml/synchronization/count_down_latch.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/shell/platform/windows/testing/windows_test.h"
#include "flutter/shell/platform/windows/testing/windows_test_config_builder.h"
#include "flutter/shell/platform/windows/testing/windows_test_context.h"
#include "gtest/gtest.h"
#include "third_party/tonic/converter/dart_converter.h"

namespace flutter {
namespace testing {

// Verify that we can fetch a texture registrar.
// Prevent regression: https://github.com/flutter/flutter/issues/86617
TEST(WindowsNoFixtureTest, GetTextureRegistrar) {
  FlutterDesktopEngineProperties properties = {};
  properties.assets_path = L"";
  properties.icu_data_path = L"icudtl.dat";
  auto engine = FlutterDesktopEngineCreate(&properties);
  ASSERT_NE(engine, nullptr);
  auto texture_registrar = FlutterDesktopEngineGetTextureRegistrar(engine);
  EXPECT_NE(texture_registrar, nullptr);
  FlutterDesktopEngineDestroy(engine);
}

// Verify we can successfully launch main().
TEST_F(WindowsTest, LaunchMain) {
  auto& context = GetContext();
  WindowsConfigBuilder builder(context);
  ViewControllerPtr controller{builder.Run()};
  ASSERT_NE(controller, nullptr);
}

// Verify there is no unexpected output from launching main.
TEST_F(WindowsTest, LaunchMainHasNoOutput) {
  // Replace stdout & stderr stream buffers with our own.
  std::stringstream cout_buffer;
  std::stringstream cerr_buffer;
  std::streambuf* old_cout_buffer = std::cout.rdbuf();
  std::streambuf* old_cerr_buffer = std::cerr.rdbuf();
  std::cout.rdbuf(cout_buffer.rdbuf());
  std::cerr.rdbuf(cerr_buffer.rdbuf());

  auto& context = GetContext();
  WindowsConfigBuilder builder(context);
  ViewControllerPtr controller{builder.Run()};
  ASSERT_NE(controller, nullptr);

  // Restore original stdout & stderr stream buffer.
  std::cout.rdbuf(old_cout_buffer);
  std::cerr.rdbuf(old_cerr_buffer);

  // Verify stdout & stderr have no output.
  std::string cout = cout_buffer.str();
  std::string cerr = cerr_buffer.str();
  EXPECT_TRUE(cout.empty());
  EXPECT_TRUE(cerr.empty());
}

// Verify we can successfully launch a custom entry point.
TEST_F(WindowsTest, LaunchCustomEntrypoint) {
  auto& context = GetContext();
  WindowsConfigBuilder builder(context);
  builder.SetDartEntrypoint("customEntrypoint");
  ViewControllerPtr controller{builder.Run()};
  ASSERT_NE(controller, nullptr);
}

// Verify that engine launches with the custom entrypoint specified in the
// FlutterDesktopEngineRun parameter when no entrypoint is specified in
// FlutterDesktopEngineProperties.dart_entrypoint.
//
// TODO(cbracken): https://github.com/flutter/flutter/issues/109285
TEST_F(WindowsTest, LaunchCustomEntrypointInEngineRunInvocation) {
  auto& context = GetContext();
  WindowsConfigBuilder builder(context);
  EnginePtr engine{builder.InitializeEngine()};
  ASSERT_NE(engine, nullptr);

  ASSERT_TRUE(FlutterDesktopEngineRun(engine.get(), "customEntrypoint"));
}

// Verify that the engine can launch in headless mode.
TEST_F(WindowsTest, LaunchHeadlessEngine) {
  auto& context = GetContext();
  WindowsConfigBuilder builder(context);
  EnginePtr engine{builder.InitializeEngine()};
  ASSERT_NE(engine, nullptr);

  ASSERT_TRUE(FlutterDesktopEngineRun(engine.get(), nullptr));
}

// Verify that engine fails to launch when a conflicting entrypoint in
// FlutterDesktopEngineProperties.dart_entrypoint and the
// FlutterDesktopEngineRun parameter.
//
// TODO(cbracken): https://github.com/flutter/flutter/issues/109285
TEST_F(WindowsTest, LaunchConflictingCustomEntrypoints) {
  auto& context = GetContext();
  WindowsConfigBuilder builder(context);
  builder.SetDartEntrypoint("customEntrypoint");
  EnginePtr engine{builder.InitializeEngine()};
  ASSERT_NE(engine, nullptr);

  ASSERT_FALSE(FlutterDesktopEngineRun(engine.get(), "conflictingEntrypoint"));
}

// Verify that native functions can be registered and resolved.
TEST_F(WindowsTest, VerifyNativeFunction) {
  auto& context = GetContext();
  WindowsConfigBuilder builder(context);
  builder.SetDartEntrypoint("verifyNativeFunction");

  fml::AutoResetWaitableEvent latch;
  auto native_entry =
      CREATE_NATIVE_ENTRY([&](Dart_NativeArguments args) { latch.Signal(); });
  context.AddNativeFunction("Signal", native_entry);

  ViewControllerPtr controller{builder.Run()};
  ASSERT_NE(controller, nullptr);

  // Wait until signal has been called.
  latch.Wait();
}

// Verify that native functions that pass parameters can be registered and
// resolved.
TEST_F(WindowsTest, VerifyNativeFunctionWithParameters) {
  auto& context = GetContext();
  WindowsConfigBuilder builder(context);
  builder.SetDartEntrypoint("verifyNativeFunctionWithParameters");

  bool bool_value = false;
  fml::AutoResetWaitableEvent latch;
  auto native_entry = CREATE_NATIVE_ENTRY([&](Dart_NativeArguments args) {
    auto handle = Dart_GetNativeBooleanArgument(args, 0, &bool_value);
    ASSERT_FALSE(Dart_IsError(handle));
    latch.Signal();
  });
  context.AddNativeFunction("SignalBoolValue", native_entry);

  ViewControllerPtr controller{builder.Run()};
  ASSERT_NE(controller, nullptr);

  // Wait until signalBoolValue has been called.
  latch.Wait();
  EXPECT_TRUE(bool_value);
}

// Verify that Platform.executable returns the executable name.
TEST_F(WindowsTest, PlatformExecutable) {
  auto& context = GetContext();
  WindowsConfigBuilder builder(context);
  builder.SetDartEntrypoint("readPlatformExecutable");

  std::string executable_name;
  fml::AutoResetWaitableEvent latch;
  auto native_entry = CREATE_NATIVE_ENTRY([&](Dart_NativeArguments args) {
    auto handle = Dart_GetNativeArgument(args, 0);
    ASSERT_FALSE(Dart_IsError(handle));
    executable_name = tonic::DartConverter<std::string>::FromDart(handle);
    latch.Signal();
  });
  context.AddNativeFunction("SignalStringValue", native_entry);

  ViewControllerPtr controller{builder.Run()};
  ASSERT_NE(controller, nullptr);

  // Wait until signalStringValue has been called.
  latch.Wait();
  EXPECT_EQ(executable_name, "flutter_windows_unittests.exe");
}

// Verify that native functions that return values can be registered and
// resolved.
TEST_F(WindowsTest, VerifyNativeFunctionWithReturn) {
  auto& context = GetContext();
  WindowsConfigBuilder builder(context);
  builder.SetDartEntrypoint("verifyNativeFunctionWithReturn");

  bool bool_value_to_return = true;
  fml::CountDownLatch latch(2);
  auto bool_return_entry = CREATE_NATIVE_ENTRY([&](Dart_NativeArguments args) {
    Dart_SetBooleanReturnValue(args, bool_value_to_return);
    latch.CountDown();
  });
  context.AddNativeFunction("SignalBoolReturn", bool_return_entry);

  bool bool_value_passed = false;
  auto bool_pass_entry = CREATE_NATIVE_ENTRY([&](Dart_NativeArguments args) {
    auto handle = Dart_GetNativeBooleanArgument(args, 0, &bool_value_passed);
    ASSERT_FALSE(Dart_IsError(handle));
    latch.CountDown();
  });
  context.AddNativeFunction("SignalBoolValue", bool_pass_entry);

  ViewControllerPtr controller{builder.Run()};
  ASSERT_NE(controller, nullptr);

  // Wait until signalBoolReturn and signalBoolValue have been called.
  latch.Wait();
  EXPECT_TRUE(bool_value_passed);
}

// Verify the next frame callback is executed.
TEST_F(WindowsTest, NextFrameCallback) {
  struct Captures {
    fml::AutoResetWaitableEvent frame_scheduled_latch;
    fml::AutoResetWaitableEvent frame_drawn_latch;
    std::thread::id thread_id;
  };
  Captures captures;

  CreateNewThread("test_platform_thread")->PostTask([&]() {
    captures.thread_id = std::this_thread::get_id();

    auto& context = GetContext();
    WindowsConfigBuilder builder(context);
    builder.SetDartEntrypoint("drawHelloWorld");

    auto native_entry = CREATE_NATIVE_ENTRY([&](Dart_NativeArguments args) {
      ASSERT_FALSE(captures.frame_drawn_latch.IsSignaledForTest());
      captures.frame_scheduled_latch.Signal();
    });
    context.AddNativeFunction("NotifyFirstFrameScheduled", native_entry);

    ViewControllerPtr controller{builder.Run()};
    ASSERT_NE(controller, nullptr);

    auto engine = FlutterDesktopViewControllerGetEngine(controller.get());

    FlutterDesktopEngineSetNextFrameCallback(
        engine,
        [](void* user_data) {
          auto captures = static_cast<Captures*>(user_data);

          ASSERT_TRUE(captures->frame_scheduled_latch.IsSignaledForTest());

          // Callback should execute on platform thread.
          ASSERT_EQ(std::this_thread::get_id(), captures->thread_id);

          // Signal the test passed and end the Windows message loop.
          captures->frame_drawn_latch.Signal();
          ::PostQuitMessage(0);
        },
        &captures);

    // Pump messages for the Windows platform task runner.
    ::MSG msg;
    while (::GetMessage(&msg, nullptr, 0, 0)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  });

  captures.frame_drawn_latch.Wait();
}

TEST_F(WindowsTest, GetGraphicsAdapter) {
  auto& context = GetContext();
  WindowsConfigBuilder builder(context);
  ViewControllerPtr controller{builder.Run()};
  ASSERT_NE(controller, nullptr);
  auto view = FlutterDesktopViewControllerGetView(controller.get());

  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_adapter = FlutterDesktopViewGetGraphicsAdapter(view);
  ASSERT_NE(dxgi_adapter, nullptr);
  DXGI_ADAPTER_DESC desc{};
  ASSERT_TRUE(SUCCEEDED(dxgi_adapter->GetDesc(&desc)));
}

}  // namespace testing
}  // namespace flutter
