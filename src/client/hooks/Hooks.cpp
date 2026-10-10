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

#include "hooks/Hooks.h"
#include <cstdio>
#include <cwctype>
#include <string>
#include "core/Client.h"
#include "util/Logger.h"

namespace raven {
namespace hooks {

namespace {
struct Pick {
  HWND best = nullptr;
  int bestScore = 0;
};

constexpr UINT kFrameMessage = WM_APP + 0x577;

std::wstring LowerW(std::wstring s) {
  for (auto& c : s) c = static_cast<wchar_t>(::towlower(c));
  return s;
}

bool IsCurrentProcessWindow(HWND w) {
  DWORD pid = 0;
  GetWindowThreadProcessId(w, &pid);
  return pid == GetCurrentProcessId();
}

int FallbackWindowScore(HWND w) {
  if (!w || !IsWindowVisible(w) || !IsCurrentProcessWindow(w)) return -1;
  wchar_t cls[64] = {};
  GetClassNameW(w, cls, 64);
  if (_wcsicmp(cls, L"ConsoleWindowClass") == 0 ||
      _wcsicmp(cls, L"a05777Overlay") == 0) {
    return -1;
  }
  RECT rc{};
  GetClientRect(w, &rc);
  int area = (rc.right - rc.left) * (rc.bottom - rc.top);
  if (area < 50 * 50) return -1;
  // A custom client can use an arbitrary title/class. Once the process check
  // has ruled out every other application, the largest visible top-level
  // window is the safest game-window fallback.
  return 1000 + area / 4096;
}

BOOL CALLBACK PickCb(HWND w, LPARAM lp) {
  auto* p = reinterpret_cast<Pick*>(lp);
  if (!IsCurrentProcessWindow(w)) return TRUE;
  int s = ScoreWindow(w);
  if (s == 0) s = FallbackWindowScore(w);
  if (s > p->bestScore) {
    p->bestScore = s;
    p->best = w;
  }
  return TRUE;
}

// ASCII-fy for logs/JSON (window titles can carry CJK).
std::string ToAscii(const std::wstring& ws) {
  std::string out;
  for (wchar_t c : ws) out.push_back(c < 128 && c >= 32 ? static_cast<char>(c) : '?');
  if (out.size() > 80) out.resize(80);
  return out;
}

std::string Describe(HWND w) {
  wchar_t title[256] = {};
  GetWindowTextW(w, title, 256);
  wchar_t cls[64] = {};
  GetClassNameW(w, cls, 64);
  RECT rc{};
  GetClientRect(w, &rc);
  char b[224];
  std::snprintf(b, sizeof(b), "'%s' [%s] %dx%d", ToAscii(title).c_str(),
                ToAscii(cls).c_str(), rc.right - rc.left, rc.bottom - rc.top);
  return b;
}
}  // namespace

Hooks& Hooks::Instance() {
  static Hooks inst;
  return inst;
}

LRESULT CALLBACK Hooks::WndProcThunk(HWND hWnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam) {
  Hooks& hooks = Instance();
  WNDPROC original = nullptr;
  bool isFrameMessage = false;
  bool isDispatchWindow = false;
  {
    std::lock_guard<std::recursive_mutex> lock(hooks.dispatchMutex_);
    isDispatchWindow = hWnd == hooks.dispatchWindow_;
    isFrameMessage = hWnd == hooks.dispatchWindow_ && msg == kFrameMessage;
    original = hooks.originalWndProc_;
    if (isFrameMessage) {
      hooks.framePending_.store(false, std::memory_order_release);
    }
  }
  if (isFrameMessage) {
    if (core::Client::Instance().Running()) core::Client::Instance().OnFrame();
    return 0;
  }

  if (isDispatchWindow && core::Client::Instance().Running()) {
    if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) {
      if (core::Client::Instance().OnNativeMouseButton(true, true)) {
        SetCapture(hWnd);
        return 0;
      }
    } else if (msg == WM_RBUTTONUP) {
      if (core::Client::Instance().OnNativeMouseButton(true, false)) {
        if (GetCapture() == hWnd) ReleaseCapture();
        return 0;
      }
    } else if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) {
      core::Client::Instance().OnNativeMouseButton(false, true);
    } else if (msg == WM_KILLFOCUS) {
      core::Client::Instance().OnNativeMouseButton(true, false);
      if (GetCapture() == hWnd) ReleaseCapture();
    } else if (msg == WM_CAPTURECHANGED) {
      core::Client::Instance().OnNativeMouseButton(true, false);
    }
  }

  if (original) return CallWindowProcW(original, hWnd, msg, wParam, lParam);
  return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool Hooks::AttachFrameDispatch(HWND w) {
  if (!w || !IsWindow(w)) return false;
  std::lock_guard<std::recursive_mutex> lock(dispatchMutex_);
  if (dispatchWindow_ == w && originalWndProc_ &&
      GetWindowLongPtrW(w, GWLP_WNDPROC) ==
          reinterpret_cast<LONG_PTR>(&Hooks::WndProcThunk)) {
    return true;
  }

  DetachFrameDispatch();
  SetLastError(0);
  LONG_PTR previous = SetWindowLongPtrW(
      w, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Hooks::WndProcThunk));
  if (!previous && GetLastError() != ERROR_SUCCESS) {
    util::Logger::Warn("Hooks: could not subclass MC window for native frame dispatch");
    return false;
  }

  dispatchWindow_ = w;
  originalWndProc_ = reinterpret_cast<WNDPROC>(previous);
  framePending_.store(false, std::memory_order_release);
  util::Logger::Info("Hooks: native frame dispatch attached to MC window thread");
  return originalWndProc_ != nullptr;
}

void Hooks::DetachFrameDispatch() {
  std::lock_guard<std::recursive_mutex> lock(dispatchMutex_);
  HWND w = dispatchWindow_;
  WNDPROC original = originalWndProc_;
  if (w && original && IsWindow(w)) {
    LONG_PTR current = GetWindowLongPtrW(w, GWLP_WNDPROC);
    if (current == reinterpret_cast<LONG_PTR>(&Hooks::WndProcThunk)) {
      SetWindowLongPtrW(w, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
    }
  }
  framePending_.store(false, std::memory_order_release);
  dispatchWindow_ = nullptr;
  originalWndProc_ = nullptr;
}

bool Hooks::FrameDispatchReady() const {
  std::lock_guard<std::recursive_mutex> lock(dispatchMutex_);
  return dispatchWindow_ && originalWndProc_ && IsWindow(dispatchWindow_) &&
         GetWindowLongPtrW(dispatchWindow_, GWLP_WNDPROC) ==
             reinterpret_cast<LONG_PTR>(&Hooks::WndProcThunk);
}

bool Hooks::PostFrame() {
  std::lock_guard<std::recursive_mutex> lock(dispatchMutex_);
  HWND w = dispatchWindow_;
  if (!w || !originalWndProc_ || !IsWindow(w) ||
      GetWindowLongPtrW(w, GWLP_WNDPROC) !=
          reinterpret_cast<LONG_PTR>(&Hooks::WndProcThunk)) {
    return false;
  }
  if (framePending_.exchange(true, std::memory_order_acq_rel)) return true;
  if (!PostMessageW(w, kFrameMessage, 0, 0)) {
    framePending_.store(false, std::memory_order_release);
    return false;
  }
  return true;
}

int ScoreWindow(HWND w) {
  if (!w || !IsWindowVisible(w)) return -1;
  wchar_t title[256] = {};
  GetWindowTextW(w, title, 256);
  wchar_t cls[64] = {};
  GetClassNameW(w, cls, 64);
  std::wstring t = title;
  if (t.empty()) return -1;
  std::wstring tl = LowerW(t);
  // Launchers/tools match "Minecraft" by title but are never the game.
  if (tl.find(L"launcher") != std::wstring::npos) return -1;
  if (tl.find(L"mojang") != std::wstring::npos) return -1;
  bool lwjgl = _wcsicmp(cls, L"LWJGL") == 0;
  bool exact = (t == L"Minecraft 1.8.9");
  bool hasMc = t.find(L"Minecraft") != std::wstring::npos ||
               t.find(L"minecraft") != std::wstring::npos;
  bool fps = t.find(L"FPSMaster") != std::wstring::npos;
  if (!lwjgl && !exact && !hasMc && !fps) return 0;
  RECT rc{};
  GetClientRect(w, &rc);
  int area = (rc.right - rc.left) * (rc.bottom - rc.top);
  if (area < 50 * 50) return -1;  // hidden helper windows (see §6.9)
  // Class/title dominate; area only breaks ties (a big launcher must NOT
  // beat a smaller game window).
  int s = area / 4096;
  if (hasMc) s += 50000;
  if (fps) s += 200000;
  if (lwjgl) s += 500000;
  if (exact) s += 1000000;
  return s;
}

HWND PickBestWindow() {
  // Prefer the exact vanilla title without a full enum round-trip.
  HWND exact = FindWindowW(nullptr, L"Minecraft 1.8.9");
  if (exact && IsCurrentProcessWindow(exact) && ScoreWindow(exact) > 0) return exact;
  Pick pick;
  EnumWindows(PickCb, reinterpret_cast<LPARAM>(&pick));
  return pick.best;
}

void Hooks::NoteWindow(HWND w) {
  window_ = w;
  std::string d = w ? Describe(w) : std::string("(none)");
  {
    std::lock_guard<std::mutex> lock(descMutex_);
    windowDesc_ = d;
  }
}

HWND FindMinecraftWindow() {
  // Scored pick (see ScoreWindow): class/title dominate, area only breaks
  // ties, launchers excluded. Logs the winner for overlay triage.
  HWND best = PickBestWindow();
  if (best) {
    char msg[300];
    std::snprintf(msg, sizeof(msg), "Hooks: MC window %s",
                  Describe(best).c_str());
    util::Logger::Info(msg);
  }
  return best;
}

bool Hooks::Install(HWND mcWindowOrNull) {
  if (installed_) return true;
  NoteWindow(mcWindowOrNull);
  if (window_ && !AttachFrameDispatch(window_)) {
    util::Logger::Warn("Hooks::Install: native frame dispatch could not attach");
  }

  HMODULE gl = GetModuleHandleW(L"opengl32.dll");
  if (!gl) gl = LoadLibraryW(L"opengl32.dll");
  wglSwapBuffers_ = gl ? reinterpret_cast<void*>(GetProcAddress(gl, "wglSwapBuffers")) : nullptr;

  if (wglSwapBuffers_) {
    util::Logger::Info("Hooks::Install: wglSwapBuffers resolved (probe only; WndProc dispatch owns frames)");
  } else {
    util::Logger::Warn("Hooks::Install: wglSwapBuffers NOT resolved; frame hook deferred");
  }
  if (window_) {
    char msg[320];
    std::snprintf(msg, sizeof(msg), "Hooks::Install: MC window %s",
                  WindowDesc().c_str());
    util::Logger::Info(msg);
  } else {
    util::Logger::Warn("Hooks::Install: MC window not found; running headless tick mode");
  }
  installed_ = true;
  return true;
}

HWND Hooks::RefreshWindow() {
  // Thin wrapper kept for callers that only need "any window, fast".
  return RevalidateWindow();
}

HWND Hooks::RevalidateWindow() {
  // Throttle EnumWindows: Hud calls us every tick while headless.
  unsigned long long now = GetTickCount64();
  static unsigned long long lastScan = 0;
  if (window_ && IsWindow(window_) && now - lastScan < 500) return window_;
  lastScan = now;
  HWND best = PickBestWindow();
  if (!window_ || !IsWindow(window_)) {
    if (best) {
      DetachFrameDispatch();
      NoteWindow(best);
      AttachFrameDispatch(best);
      char msg[320];
      std::snprintf(msg, sizeof(msg), "Hooks: window acquired %s",
                    WindowDesc().c_str());
      util::Logger::Info(msg);
    } else {
      DetachFrameDispatch();
      NoteWindow(nullptr);
    }
    return window_;
  }
  if (!FrameDispatchReady()) AttachFrameDispatch(window_);
  if (best && best != window_ && ScoreWindow(best) > ScoreWindow(window_)) {
    char msg[480];
    std::snprintf(msg, sizeof(msg), "Hooks: window switched %s -> %s",
                  Describe(window_).c_str(), Describe(best).c_str());
    DetachFrameDispatch();
    NoteWindow(best);
    AttachFrameDispatch(best);
    util::Logger::Info(msg);
  }
  return window_;
}

std::string Hooks::WindowDesc() {
  std::lock_guard<std::mutex> lock(descMutex_);
  return windowDesc_;
}

void Hooks::Uninstall() {
  DetachFrameDispatch();
  installed_ = false;
  wglSwapBuffers_ = nullptr;
  NoteWindow(nullptr);
}

}  // namespace hooks
}  // namespace raven
