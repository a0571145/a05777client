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
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Click-through layered overlay for ESP boxes (GDI, magenta colorkey).
// No GL hook required: the worker thread pushes screen-space boxes collected
// via JNI, the overlay thread draws them at ~30 Hz above the MC window.
namespace raven {
namespace render {

struct EspBox {
  float x = 0.f, y = 0.f, w = 0.f, h = 0.f;  // screen px, y-down
  float hp = 1.f;                            // 0..1 (clamped by producer)
  uint32_t color = 0x00FF00;                 // 0xRRGGBB
  bool drawBar = true;
  int lineW = 1;  // box outline width in px (PlayerESP "Line width")
};

struct EspCamera {
  double ex = 0.0, ey = 0.0, ez = 0.0;
  double yaw = 0.0, pitch = 0.0, fov = 90.0;
  int w = 0, h = 0;
};

struct EspSnapshot {
  EspCamera cam;
  std::vector<EspBox> boxes;
  std::vector<std::pair<std::string, uint32_t>> lines;
};

// HUD layout and palette, pushed by Hud and cached for the overlay draw thread.
struct HudStyle {
  uint32_t color = 0xFFFFFF;
  bool background = false;
  bool shadow = true;
  int outline = 0;  // 0=None, 1=Side bar, 2=Full border
  bool alignRight = true;
  bool enabled = false;
  int x = 8;
  int y = 70;
  float scale = 1.0f;  // HUD "Scale" slider (0.5..2): font + row metrics
};

// Diagnosis for "overlay invisible" triage (exposed via /api/debug/esp).
struct OverlayDiag {
  bool running = false;
  bool hasWindow = false;
  bool windowValid = false;
  bool minimized = false;
  bool fullscreen = false;  // borderless/exclusive-sized: layered may not cover
  int x = 0, y = 0, w = 0, h = 0;
  unsigned dpi = 0;          // window DPI (96 = 100%)
  unsigned long lastUwlErr = 0;
  bool ulwFailed = false;
  bool surfFailed = false;
  unsigned long long beats = 0;
  // Watchdog: where the draw thread is (see Loop stages), updated every frame.
  // 0=never entered, 1=pumping, 2=window bad/minimized, 3=tiny area,
  // 4=rect ok, 5=drawn, 6=presented. iters counts completed presentations.
  unsigned long long iters = 0;
  int stage = 0;
  bool flash = false;  // red test pattern active (POST /api/debug/overlay/flash)
  bool paintMode = false;  // presenting via WM_PAINT BitBlt (ULW fallback)
};

class OverlayWindow {
public:
  static OverlayWindow& Instance();

  // mcWindow: the LWJGL game window to follow. False when null/unusable.
  bool Start(HWND mcWindow);
  void Stop();
  // Unconditional teardown (DLL unload path): zeroes users, joins thread.
  void Shutdown();
  bool IsRunning() const { return running_.load(); }

  // Snapshot from the game-logic thread (copied under lock).
  void SetBoxes(std::vector<EspBox> boxes);
  void Clear();
  // Camera for /api/debug/esp diagnosis.
  void SetCamera(const EspCamera& cam);
  EspSnapshot GetSnapshot();
  OverlayDiag GetDiag();
  // Heal a stale game-window handle while running (game recreates its window).
  void Retarget(HWND mcWindow);
  HWND CurrentWindow();
  // WM_PAINT entry (called by Proc on the Loop thread). Public only because
  // the window proc is a free function; do not call from elsewhere.
  void OnPaint(HWND h);
  // Draw a fullscreen red test pattern for ms (0 = off). Tells "window path
  // broken" apart from "text rendering broken".
  void FlashTest(int ms);
  // HUD text lines (ArrayList), drawn at HudStyle position. Color per line 0xRRGGBB.
  void SetLines(std::vector<std::pair<std::string, uint32_t>> lines);
  void SetHudStyle(const HudStyle& st);

private:
  OverlayWindow() = default;
  void Loop();
  void TeardownLocked();  // startMutex_ held
  bool EnsureSurface(int w, int h);
  void DrawFrame(int w, int h);
  // Window + font MUST be created/destroyed on the Loop thread (its owner):
  // cross-thread SetWindowPos/DestroyWindow block on synchronous window
  // messages that only the owner thread can dispatch (stage 4 hang, 2026-09-30).
  bool CreateOwnedWindow();
  void DestroyOwnedWindow();

  std::atomic<bool> running_{false};
  HWND mcWindow_ = nullptr;
  HWND overlay_ = nullptr;
  ATOM klass_ = 0;
  std::thread thread_;
  std::mutex mutex_;        // boxes_/lines_/cam_
  std::mutex startMutex_;   // Start/Stop serialization
  int users_ = 0;           // shared by HUD + ESP; teardown at zero
  std::vector<EspBox> boxes_;
  std::vector<std::pair<std::string, uint32_t>> lines_;
  HudStyle hudStyle_;
  EspCamera cam_;
  HFONT font_ = nullptr;
  bool fontOwned_ = false;
  float fontScale_ = 1.0f;  // Loop-thread only: rebuild font_ when HudStyle.scale moves
  // Diagnostics (written by Loop thread, read by REST thread under mutex_).
  OverlayDiag diag_;
  // Test-pattern deadline (Loop thread only, steady clock).
  unsigned long long flashUntil_ = 0;
  // Presentation state (Loop thread only).
  bool paintMode_ = false;
  int ulwFailStreak_ = 0;

  HDC memDc_ = nullptr;
  HBITMAP dib_ = nullptr;
  HGDIOBJ oldBmp_ = nullptr;
  void* bits_ = nullptr;
  int dibW_ = 0, dibH_ = 0;
};

}  // namespace render
}  // namespace raven
