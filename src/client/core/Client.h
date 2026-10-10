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

#pragma once
#include <windows.h>
#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include "core/ModuleManager.h"

// C++ port of keystrokesmod/Raven.java init/onTick.
// DllMain spawns Client::Run on a worker thread (never do work under loader lock).
namespace raven {
namespace core {

class Client {
public:
  static Client& Instance();

  void Start(HMODULE self);
  void Stop();

  // Called from the game window thread through the native WM_APP dispatch;
  // the worker thread calls this only when no dispatch window is available.
  void OnFrame();
  // Runs before the game's original WndProc handles mouse-button messages.
  // Returns true when AutoBlock owns and consumes the right-button event.
  bool OnNativeMouseButton(bool rightButton, bool down);

  ModuleManager& Modules() { return modules_; }
  bool Running() const { return running_.load(); }

 private:
  Client() = default;
  void Loop();
  // Hotkey polling: edge-detect each module's bind (Win32 VK, 0 = none)
  // and toggle on rising edge. Cheap path is JNI-free (GetAsyncKeyState
  // only); the foreground + open-screen gates run only when an edge fired.
  void PollBinds();

  std::atomic<bool> running_{false};
  HMODULE self_ = nullptr;
  std::thread worker_;
  ModuleManager modules_;
  std::map<int, bool> bindPrev_;  // VK -> was-down (worker thread only)
  std::recursive_mutex frameMutex_;  // serializes frames and pre-input callbacks
};

}  // namespace core
}  // namespace raven
