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
#include <mutex>
#include <string>

// Hook layer: resolves the OpenGL entry point and, when a game window is
// available, installs a small WndProc dispatch point for game-thread frames.
// The OpenGL pointer is still only a capability probe; frame execution uses
// the native WM_APP dispatch and has a worker fallback for headless clients.
namespace raven {
namespace hooks {

class Hooks {
public:
  static Hooks& Instance();
  bool Install(HWND mcWindowOrNull);
  void Uninstall();

  bool WglResolved() const { return wglSwapBuffers_ != nullptr; }
  HWND Window() const { return window_; }
  void SetWindow(HWND w) { window_ = w; }
  // Posts a native frame message to the game window thread. The handler calls
  // Client::OnFrame from that thread so JNI-backed modules observe vanilla's
  // own input/render state. Returns false when the subclass is unavailable.
  bool PostFrame();
  bool FrameDispatchReady() const;
  // Re-run window discovery when the cached handle is null/invalid
  // (game started after inject, or window recreated). Returns fresh handle.
  HWND RefreshWindow();
  // Full re-validation: switch to a strictly better-scoring window when one
  // appears (e.g. launcher matched first, game opened later). Throttled.
  HWND RevalidateWindow();
  // Short ASCII description of the tracked window for /api/debug/esp.
  std::string WindowDesc();

 private:
  Hooks() = default;
  static LRESULT CALLBACK WndProcThunk(HWND hWnd, UINT msg, WPARAM wParam,
                                       LPARAM lParam);
  bool AttachFrameDispatch(HWND w);
  void DetachFrameDispatch();
  void NoteWindow(HWND w);
  void* wglSwapBuffers_ = nullptr;
  HWND window_ = nullptr;
  bool installed_ = false;
  HWND dispatchWindow_ = nullptr;
  WNDPROC originalWndProc_ = nullptr;
  std::atomic<bool> framePending_{false};
  mutable std::recursive_mutex dispatchMutex_;
  std::mutex descMutex_;
  std::string windowDesc_;
};

HWND FindMinecraftWindow();
int ScoreWindow(HWND w);
HWND PickBestWindow();

}  // namespace hooks
}  // namespace raven
