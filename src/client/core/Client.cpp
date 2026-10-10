/*
 * Copyright (C) 2026 a05777
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "core/Client.h"
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include "hooks/Hooks.h"
#include "modules/AutoBlock.h"
#include "render/Overlay.h"
#include "render/OverlayWindow.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"
#include "util/Logger.h"
#include "web/RestServer.h"

namespace raven {
namespace core {

Client& Client::Instance() {
  static Client inst;
  return inst;
}

void Client::Start(HMODULE self) {
  if (running_.exchange(true)) return;
  self_ = self;
  // Log next to the DLL: the host's working dir is often not writable
  // (e.g. launcher sandbox), which would silently drop all logging.
  wchar_t dllPath[MAX_PATH] = {};
  GetModuleFileNameW(self, dllPath, MAX_PATH);
  std::wstring logPath = dllPath;
  size_t sep = logPath.find_last_of(L"\\/");
  logPath = (sep == std::wstring::npos) ? L"a05777Client.log"
                                        : logPath.substr(0, sep) + L"\\a05777Client.log";
  util::Logger::Init(logPath.c_str());
  util::Logger::Info("a05777Client starting (MC 1.8.9, injected, C++)");
  modules_.RegisterDefaults();
  sdk::Jvm::Instance().Attach();
  hooks::Hooks::Instance().Install(hooks::FindMinecraftWindow());
  // Web panel replaces any in-game menu by design.
  web::RestServer::Instance().Start(&modules_, 8080);
  worker_ = std::thread(&Client::Loop, this);
}

void Client::Stop() {
  if (!running_.exchange(false)) return;
  web::RestServer::Instance().Stop();
  // Stop posting native frame messages before restoring the game's WndProc.
  hooks::Hooks::Instance().Uninstall();
  if (worker_.joinable()) worker_.join();
  // Tear down the overlay and modules only after an in-flight game-window
  // frame has left the DLL.
  std::lock_guard<std::recursive_mutex> frameLock(frameMutex_);
  render::OverlayWindow::Instance().Shutdown();
  // Module-owned JNI/native state must be released while the DLL is still
  // mapped; AutoBlock may still hold vanilla key/animation state here.
  modules_.DisableAll();
  sdk::Jvm::Instance().Detach();
  util::Logger::Info("a05777Client stopped");
  util::Logger::Shutdown();
}

void Client::OnFrame() {
  if (!running_.load()) return;
  std::lock_guard<std::recursive_mutex> frameLock(frameMutex_);
  if (!running_.load()) return;
  render::Overlay::Instance().BeginFrame();
  modules_.TickAll();
  modules_.RenderAll();
  PollBinds();
  render::Overlay::Instance().EndFrame();
}

bool Client::OnNativeMouseButton(bool rightButton, bool down) {
  if (!running_.load()) return false;
  std::lock_guard<std::recursive_mutex> frameLock(frameMutex_);
  if (!running_.load()) return false;
  std::lock_guard<std::recursive_mutex> lock(modules_.Mutex());
  auto* autoblock = dynamic_cast<modules::AutoBlock*>(
      modules_.Find("Auto Block"));
  if (!autoblock) return false;
  if (rightButton) return autoblock->HandleRightMouseButton(down);
  if (down) autoblock->HandleManualAttackInput();
  return false;
}

void Client::PollBinds() {
  auto mods = modules_.Snapshot();
  std::vector<Module*> edged;
  for (auto& m : mods) {
    int vk = m->Bind();
    if (vk <= 0 || vk > 254) continue;
    bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    auto it = bindPrev_.find(vk);
    bool was = (it != bindPrev_.end()) ? it->second : false;
    bindPrev_[vk] = down;
    if (down && !was) edged.push_back(m.get());
  }
  if (edged.empty()) return;
  // Gate 1: the game window must hold the foreground (typing in a browser
  // or clicking the web panel must never toggle modules).
  HWND mcw = hooks::Hooks::Instance().Window();
  if (!mcw || GetForegroundWindow() != mcw) return;
  // Gate 2: no GUI screen open (typing in chat must never toggle modules).
  // Needs JNI: without a JVM (smoke/self-test) there is nothing to gate,
  // and no key edge can be real anyway, so stay inert.
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  if (!sdk::Mc::Instance().Ensure(env)) return;
  sdk::Mc::Frame f;
  if (!sdk::Mc::Instance().BeginFrame(env, f)) return;
  bool screenOpen = f.screen != nullptr;
  sdk::Mc::Instance().ReleaseFrame(f);
  if (screenOpen) return;
  std::lock_guard<std::recursive_mutex> lock(modules_.Mutex());
  for (auto* m : edged) m->Toggle();
}

void Client::Loop() {
  // 100 Hz logic tick: modules poll JNI state here. (MC itself ticks at
  // 20 Hz; the finer granularity gives CPS schedulers ~10 ms resolution.)
  using namespace std::chrono_literals;
  int ticks = 0;
  while (running_.load()) {
    auto& hooks = hooks::Hooks::Instance();
    if (hooks.FrameDispatchReady()) {
      // JNI-backed module work runs on the game's window thread. If posting
      // fails, keep the old worker fallback alive for headless/custom clients.
      if (!hooks.PostFrame()) OnFrame();
    } else {
      OnFrame();
    }
    // ~1 Hz: heal a stale game-window handle (game recreates its window,
    // or injection raced window creation). Cheap: RefreshWindow is a no-op
    // while the cached handle is valid.
    if (++ticks % 100 == 0) {
      HWND fresh = hooks::Hooks::Instance().RefreshWindow();
      if (fresh) render::OverlayWindow::Instance().Retarget(fresh);
    }
    std::this_thread::sleep_for(10ms);
  }
}

}  // namespace core
}  // namespace raven
