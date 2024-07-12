// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/windows/public/flutter_windows.h"

#include <io.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <vector>

#include "flutter/shell/platform/common/client_wrapper/include/flutter/plugin_registrar.h"
#include "flutter/shell/platform/common/incoming_message_dispatcher.h"
#include "flutter/shell/platform/common/path_utils.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "flutter/shell/platform/windows/dpi_utils.h"
#include "flutter/shell/platform/windows/flutter_project_bundle.h"
#include "flutter/shell/platform/windows/flutter_window.h"
#include "flutter/shell/platform/windows/flutter_windows_engine.h"
#include "flutter/shell/platform/windows/flutter_windows_view.h"
#include "flutter/shell/platform/windows/window_binding_handler.h"
#include "flutter/shell/platform/windows/window_state.h"

static_assert(FLUTTER_ENGINE_VERSION == 1, "");

// Returns the engine corresponding to the given opaque API handle.
static flutter::FlutterWindowsEngine* EngineFromHandle(
    FlutterDesktopEngineRef ref) {
  return reinterpret_cast<flutter::FlutterWindowsEngine*>(ref);
}

// Returns the opaque API handle for the given engine instance.
static FlutterDesktopEngineRef HandleForEngine(
    flutter::FlutterWindowsEngine* engine) {
  return reinterpret_cast<FlutterDesktopEngineRef>(engine);
}

// Returns the view corresponding to the given opaque API handle.
static flutter::FlutterWindowsView* ViewFromHandle(FlutterDesktopViewRef ref) {
  return reinterpret_cast<flutter::FlutterWindowsView*>(ref);
}

// Returns the opaque API handle for the given view instance.
static FlutterDesktopViewRef HandleForView(flutter::FlutterWindowsView* view) {
  return reinterpret_cast<FlutterDesktopViewRef>(view);
}

// Returns the texture registrar corresponding to the given opaque API handle.
static flutter::FlutterWindowsTextureRegistrar* TextureRegistrarFromHandle(
    FlutterDesktopTextureRegistrarRef ref) {
  return reinterpret_cast<flutter::FlutterWindowsTextureRegistrar*>(ref);
}

// Returns the opaque API handle for the given texture registrar instance.
static FlutterDesktopTextureRegistrarRef HandleForTextureRegistrar(
    flutter::FlutterWindowsTextureRegistrar* registrar) {
  return reinterpret_cast<FlutterDesktopTextureRegistrarRef>(registrar);
}

FlutterDesktopViewControllerRef FlutterDesktopViewControllerCreate(
    int width,
    int height,
    FlutterDesktopEngineRef engine) {
  std::unique_ptr<flutter::WindowBindingHandler> window_wrapper =
      std::make_unique<flutter::FlutterWindow>(width, height);

  auto state = std::make_unique<FlutterDesktopViewControllerState>();
  state->view =
      std::make_unique<flutter::FlutterWindowsView>(std::move(window_wrapper));
  // Take ownership of the engine, starting it if necessary.
  state->view->SetEngine(
      std::unique_ptr<flutter::FlutterWindowsEngine>(EngineFromHandle(engine)));
  state->view->CreateRenderSurface();
  if (!state->view->GetEngine()->running()) {
    if (!state->view->GetEngine()->Run()) {
      return nullptr;
    }
  }

  // Must happen after engine is running.
  state->view->SendInitialBounds();
  state->view->SendInitialAccessibilityFeatures();
  return state.release();
}

void FlutterDesktopViewControllerDestroy(
    FlutterDesktopViewControllerRef controller) {
  delete controller;
}

FlutterDesktopEngineRef FlutterDesktopViewControllerGetEngine(
    FlutterDesktopViewControllerRef controller) {
  return HandleForEngine(controller->view->GetEngine());
}

FlutterDesktopViewRef FlutterDesktopViewControllerGetView(
    FlutterDesktopViewControllerRef controller) {
  return HandleForView(controller->view.get());
}

void FlutterDesktopViewControllerForceRedraw(
    FlutterDesktopViewControllerRef controller) {
  controller->view->ForceRedraw();
}

bool FlutterDesktopViewControllerHandleTopLevelWindowProc(
    FlutterDesktopViewControllerRef controller,
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam,
    LRESULT* result) {
  std::optional<LRESULT> delegate_result =
      controller->view->GetEngine()
          ->window_proc_delegate_manager()
          ->OnTopLevelWindowProc(hwnd, message, wparam, lparam);
  if (delegate_result) {
    *result = *delegate_result;
  }
  return delegate_result.has_value();
}

FlutterDesktopEngineRef FlutterDesktopEngineCreate(
    const FlutterDesktopEngineProperties* engine_properties) {
  flutter::FlutterProjectBundle project(*engine_properties);
  auto engine = std::make_unique<flutter::FlutterWindowsEngine>(project);
  return HandleForEngine(engine.release());
}

bool FlutterDesktopEngineDestroy(FlutterDesktopEngineRef engine_ref) {
  flutter::FlutterWindowsEngine* engine = EngineFromHandle(engine_ref);
  bool result = true;
  if (engine->running()) {
    result = engine->Stop();
  }
  delete engine;
  return result;
}

bool FlutterDesktopEngineRun(FlutterDesktopEngineRef engine,
                             const char* entry_point) {
  std::string_view entry_point_view{""};
  if (entry_point != nullptr) {
    entry_point_view = entry_point;
  }

  return EngineFromHandle(engine)->Run(entry_point_view);
}

uint64_t FlutterDesktopEngineProcessMessages(FlutterDesktopEngineRef engine) {
  return std::chrono::nanoseconds::max().count();
}

void FlutterDesktopEngineReloadSystemFonts(FlutterDesktopEngineRef engine) {
  EngineFromHandle(engine)->ReloadSystemFonts();
}

FlutterDesktopPluginRegistrarRef FlutterDesktopEngineGetPluginRegistrar(
    FlutterDesktopEngineRef engine,
    const char* plugin_name) {
  // Currently, one registrar acts as the registrar for all plugins, so the
  // name is ignored. It is part of the API to reduce churn in the future when
  // aligning more closely with the Flutter registrar system.

  return EngineFromHandle(engine)->GetRegistrar();
}

FlutterDesktopMessengerRef FlutterDesktopEngineGetMessenger(
    FlutterDesktopEngineRef engine) {
  return EngineFromHandle(engine)->messenger();
}

FlutterDesktopTextureRegistrarRef FlutterDesktopEngineGetTextureRegistrar(
    FlutterDesktopEngineRef engine) {
  return HandleForTextureRegistrar(
      EngineFromHandle(engine)->texture_registrar());
}

void FlutterDesktopEngineSetNextFrameCallback(FlutterDesktopEngineRef engine,
                                              VoidCallback callback,
                                              void* user_data) {
  EngineFromHandle(engine)->SetNextFrameCallback(
      [callback, user_data]() { callback(user_data); });
}

HWND FlutterDesktopViewGetHWND(FlutterDesktopViewRef view) {
  return ViewFromHandle(view)->GetPlatformWindow();
}

IDXGIAdapter* FlutterDesktopViewGetGraphicsAdapter(FlutterDesktopViewRef view) {
  auto surface_manager = ViewFromHandle(view)->GetEngine()->surface_manager();
  if (surface_manager) {
    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    if (surface_manager->GetDevice(d3d_device.GetAddressOf()) &&
        SUCCEEDED(d3d_device.As(&dxgi_device))) {
      IDXGIAdapter* adapter;
      if (SUCCEEDED(dxgi_device->GetAdapter(&adapter))) {
        return adapter;
      }
    }
  }
  return nullptr;
}

FlutterDesktopViewRef FlutterDesktopPluginRegistrarGetView(
    FlutterDesktopPluginRegistrarRef registrar) {
  return HandleForView(registrar->engine->view());
}

void FlutterDesktopPluginRegistrarRegisterTopLevelWindowProcDelegate(
    FlutterDesktopPluginRegistrarRef registrar,
    FlutterDesktopWindowProcCallback delegate,
    void* user_data) {
  registrar->engine->window_proc_delegate_manager()
      ->RegisterTopLevelWindowProcDelegate(delegate, user_data);
}

void FlutterDesktopPluginRegistrarUnregisterTopLevelWindowProcDelegate(
    FlutterDesktopPluginRegistrarRef registrar,
    FlutterDesktopWindowProcCallback delegate) {
  registrar->engine->window_proc_delegate_manager()
      ->UnregisterTopLevelWindowProcDelegate(delegate);
}

UINT FlutterDesktopGetDpiForHWND(HWND hwnd) {
  return flutter::GetDpiForHWND(hwnd);
}

UINT FlutterDesktopGetDpiForMonitor(HMONITOR monitor) {
  return flutter::GetDpiForMonitor(monitor);
}

void FlutterDesktopResyncOutputStreams() {
  FILE* unused;
  if (freopen_s(&unused, "CONOUT$", "w", stdout)) {
    _dup2(_fileno(stdout), 1);
  }
  if (freopen_s(&unused, "CONOUT$", "w", stderr)) {
    _dup2(_fileno(stdout), 2);
  }
  std::ios::sync_with_stdio();
}

// Implementations of common/ API methods.

FlutterDesktopMessengerRef FlutterDesktopPluginRegistrarGetMessenger(
    FlutterDesktopPluginRegistrarRef registrar) {
  return registrar->engine->messenger();
}

void FlutterDesktopPluginRegistrarSetDestructionHandler(
    FlutterDesktopPluginRegistrarRef registrar,
    FlutterDesktopOnPluginRegistrarDestroyed callback) {
  registrar->engine->AddPluginRegistrarDestructionCallback(callback, registrar);
}

bool FlutterDesktopMessengerSendWithReply(FlutterDesktopMessengerRef messenger,
                                          const char* channel,
                                          const uint8_t* message,
                                          const size_t message_size,
                                          const FlutterDesktopBinaryReply reply,
                                          void* user_data) {
  FML_DCHECK(FlutterDesktopMessengerIsAvailable(messenger))
      << "Messenger must reference a running engine to send a message";

  return flutter::FlutterDesktopMessenger::FromRef(messenger)
      ->GetEngine()
      ->SendPlatformMessage(channel, message, message_size, reply, user_data);
}

bool FlutterDesktopMessengerSend(FlutterDesktopMessengerRef messenger,
                                 const char* channel,
                                 const uint8_t* message,
                                 const size_t message_size) {
  return FlutterDesktopMessengerSendWithReply(messenger, channel, message,
                                              message_size, nullptr, nullptr);
}

void FlutterDesktopMessengerSendResponse(
    FlutterDesktopMessengerRef messenger,
    const FlutterDesktopMessageResponseHandle* handle,
    const uint8_t* data,
    size_t data_length) {
  FML_DCHECK(FlutterDesktopMessengerIsAvailable(messenger))
      << "Messenger must reference a running engine to send a response";

  flutter::FlutterDesktopMessenger::FromRef(messenger)
      ->GetEngine()
      ->SendPlatformMessageResponse(handle, data, data_length);
}

void FlutterDesktopMessengerSetCallback(FlutterDesktopMessengerRef messenger,
                                        const char* channel,
                                        FlutterDesktopMessageCallback callback,
                                        void* user_data) {
  FML_DCHECK(FlutterDesktopMessengerIsAvailable(messenger))
      << "Messenger must reference a running engine to set a callback";

  flutter::FlutterDesktopMessenger::FromRef(messenger)
      ->GetEngine()
      ->message_dispatcher()
      ->SetMessageCallback(channel, callback, user_data);
}

FlutterDesktopMessengerRef FlutterDesktopMessengerAddRef(
    FlutterDesktopMessengerRef messenger) {
  return flutter::FlutterDesktopMessenger::FromRef(messenger)
      ->AddRef()
      ->ToRef();
}

void FlutterDesktopMessengerRelease(FlutterDesktopMessengerRef messenger) {
  flutter::FlutterDesktopMessenger::FromRef(messenger)->Release();
}

bool FlutterDesktopMessengerIsAvailable(FlutterDesktopMessengerRef messenger) {
  return flutter::FlutterDesktopMessenger::FromRef(messenger)->GetEngine() !=
         nullptr;
}

FlutterDesktopMessengerRef FlutterDesktopMessengerLock(
    FlutterDesktopMessengerRef messenger) {
  flutter::FlutterDesktopMessenger::FromRef(messenger)->GetMutex().lock();
  return messenger;
}

void FlutterDesktopMessengerUnlock(FlutterDesktopMessengerRef messenger) {
  flutter::FlutterDesktopMessenger::FromRef(messenger)->GetMutex().unlock();
}

FlutterDesktopTextureRegistrarRef FlutterDesktopRegistrarGetTextureRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  return HandleForTextureRegistrar(registrar->engine->texture_registrar());
}

int64_t FlutterDesktopTextureRegistrarRegisterExternalTexture(
    FlutterDesktopTextureRegistrarRef texture_registrar,
    const FlutterDesktopTextureInfo* texture_info) {
  return TextureRegistrarFromHandle(texture_registrar)
      ->RegisterTexture(texture_info);
}

void FlutterDesktopTextureRegistrarUnregisterExternalTexture(
    FlutterDesktopTextureRegistrarRef texture_registrar,
    int64_t texture_id,
    void (*callback)(void* user_data),
    void* user_data) {
  auto registrar = TextureRegistrarFromHandle(texture_registrar);
  if (callback) {
    registrar->UnregisterTexture(
        texture_id, [callback, user_data]() { callback(user_data); });
    return;
  }
  registrar->UnregisterTexture(texture_id);
}

bool FlutterDesktopTextureRegistrarMarkExternalTextureFrameAvailable(
    FlutterDesktopTextureRegistrarRef texture_registrar,
    int64_t texture_id) {
  return TextureRegistrarFromHandle(texture_registrar)
      ->MarkTextureFrameAvailable(texture_id);
}
