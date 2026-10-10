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

#include "render/OverlayWindow.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include "util/Logger.h"

namespace raven {
namespace render {

namespace {
constexpr COLORREF kKey = RGB(255, 0, 255);
constexpr wchar_t kClass[] = L"a05777Overlay";

// Make the process per-monitor DPI aware so GetClientRect/ClientToScreen
// (physical px) matches UpdateLayeredWindow. Dynamic load: works back to Win7.
void TryEnableDpiAwareness() {
  static bool done = false;
  if (done) return;
  done = true;
  HMODULE u32 = GetModuleHandleW(L"user32.dll");
  if (!u32) return;
  // Win10 1703+: SetProcessDpiAwarenessContext(PMv2 = HANDLE(-4)).
  using CtxFn = BOOL(WINAPI*)(HANDLE);
  auto ctx = reinterpret_cast<CtxFn>(GetProcAddress(u32, "SetProcessDpiAwarenessContext"));
  if (ctx) {
    if (ctx(reinterpret_cast<HANDLE>(-4))) {
      util::Logger::Info("OverlayWindow: DPI awareness PMv2 enabled");
    } else {
      util::Logger::Warn("OverlayWindow: SetProcessDpiAwarenessContext failed");
    }
    return;
  }
  using AwareFn = BOOL(WINAPI*)();
  auto aware = reinterpret_cast<AwareFn>(GetProcAddress(u32, "SetProcessDPIAware"));
  if (aware && aware()) {
    util::Logger::Info("OverlayWindow: DPI awareness (system) enabled");
  }
}

unsigned QueryWindowDpi(HWND w) {
  if (HMODULE u32 = GetModuleHandleW(L"user32.dll")) {
    using DpiFn = UINT(WINAPI*)(HWND);
    if (auto fn = reinterpret_cast<DpiFn>(GetProcAddress(u32, "GetDpiForWindow"))) {
      UINT d = fn(w);
      if (d >= 48 && d <= 480) return d;
    }
  }
  return 96;
}

// Borderless/exclusive-sized window covers the whole monitor: GDI layered
// overlay cannot reliably sit above exclusive fullscreen (by design).
bool IsFullscreenSized(HWND w, int clientW, int clientH) {
  if (!w) return false;
  HMONITOR mon = MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST);
  if (!mon) return false;
  MONITORINFO mi{};
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfoW(mon, &mi)) return false;
  int mw = mi.rcMonitor.right - mi.rcMonitor.left;
  int mh = mi.rcMonitor.bottom - mi.rcMonitor.top;
  // Client area ~= monitor size (allow 4px for borders/shadow).
  return clientW >= mw - 4 && clientH >= mh - 4;
}

LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_PAINT) {
    auto* self = reinterpret_cast<OverlayWindow*>(
        GetWindowLongPtrW(h, GWLP_USERDATA));
    if (self) {
      self->OnPaint(h);
      return 0;
    }
  }
  return DefWindowProcW(h, m, w, l);
}
}  // namespace

OverlayWindow& OverlayWindow::Instance() {
  static OverlayWindow inst;
  return inst;
}

bool OverlayWindow::Start(HWND mcWindow) {
  std::lock_guard<std::mutex> lock(startMutex_);
  TryEnableDpiAwareness();
  // Silent on missing window: HUD/ESP retry this every tick until the game
  // window exists; logging each attempt would flood the log.
  if (!mcWindow || !IsWindow(mcWindow)) return false;
  mcWindow_ = mcWindow;
  users_++;
  if (running_.load()) return true;  // already up, just counted

  if (!klass_) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = Proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClass;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
    klass_ = RegisterClassExW(&wc);
    if (!klass_) {
      DWORD err = GetLastError();
      if (err == ERROR_CLASS_ALREADY_EXISTS) {
        // Re-inject into a process where our class survived eject
        // (never unregistered on purpose: the owner HMODULE is the host exe).
        klass_ = 1;
        util::Logger::Warn("OverlayWindow: class already registered, reusing");
      } else {
        char msg[96];
        std::snprintf(msg, sizeof(msg), "OverlayWindow: RegisterClassEx failed: %lu",
                      static_cast<unsigned long>(err));
        util::Logger::Error(msg);
        users_--;  // Start failed: roll back the count taken above
        return false;
      }
    }
  }

  overlay_ = nullptr;  // created on the Loop thread (see CreateOwnedWindow)
  font_ = nullptr;
  fontOwned_ = false;

  running_ = true;
  thread_ = std::thread(&OverlayWindow::Loop, this);
  util::Logger::Info("OverlayWindow: ESP overlay running");
  return true;
}

bool OverlayWindow::CreateOwnedWindow() {
  // Runs on the Loop thread: the window is owned by the thread that pumps it.
  if (overlay_) return true;
  overlay_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW |
          WS_EX_NOACTIVATE,
      kClass, L"", WS_POPUP | WS_VISIBLE, 0, 0, 100, 100, nullptr, nullptr,
      GetModuleHandleW(nullptr), nullptr);
  if (!overlay_) {
    DWORD err = GetLastError();
    char msg[96];
    std::snprintf(msg, sizeof(msg), "OverlayWindow: CreateWindowEx failed: %lu",
                  static_cast<unsigned long>(err));
    util::Logger::Error(msg);
    return false;
  }
  // No SetLayeredWindowAttributes here: the ULW path needs none (it would only
  // add a mode-switch variable), paint mode sets it on fallback (see Loop).
  SetWindowLongPtrW(overlay_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  font_ = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                      DEFAULT_PITCH, L"Arial");
  fontOwned_ = (font_ != nullptr);
  if (!font_) {
    font_ = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    util::Logger::Warn("OverlayWindow: Consolas missing, fallback font used");
  }
  fontScale_ = 1.0f;  // DrawFrame rebuilds the font when the HUD scale changes.
  return true;
}

void OverlayWindow::OnPaint(HWND h) {
  // Owner (Loop) thread only. Paint-mode presentation: copy the DIB-backed
  // frame to the window; magenta is keyed out by LWA_COLORKEY attributes.
  PAINTSTRUCT ps{};
  HDC dc = BeginPaint(h, &ps);
  if (dc && paintMode_ && memDc_ && dibW_ > 0 && dibH_ > 0) {
    BitBlt(dc, 0, 0, dibW_, dibH_, memDc_, 0, 0, SRCCOPY);
  }
  EndPaint(h, &ps);
}

void OverlayWindow::DestroyOwnedWindow() {
  // Runs on the Loop thread (owner). Same-thread DestroyWindow cannot block.
  if (memDc_) {
    if (dib_) {
      if (oldBmp_) SelectObject(memDc_, oldBmp_);
      DeleteObject(dib_);
      dib_ = nullptr;
      oldBmp_ = nullptr;
    }
    DeleteDC(memDc_);
    memDc_ = nullptr;
  }
  dibW_ = dibH_ = 0;
  bits_ = nullptr;
  if (overlay_) {
    DestroyWindow(overlay_);
    overlay_ = nullptr;
  }
  if (font_) {
    if (fontOwned_) DeleteObject(font_);
    font_ = nullptr;
    fontOwned_ = false;
  }
}

void OverlayWindow::Stop() {
  std::lock_guard<std::mutex> lock(startMutex_);
  if (users_ > 0) users_--;
  if (users_ > 0 || !running_.load()) return;
  TeardownLocked();
}

void OverlayWindow::Shutdown() {
  std::lock_guard<std::mutex> lock(startMutex_);
  users_ = 0;
  if (running_.load()) TeardownLocked();
}

void OverlayWindow::TeardownLocked() {
  running_ = false;
  if (thread_.joinable()) thread_.join();
  // The Loop thread destroys its own window/font/surface on exit (same-thread
  // owner). Anything left here means the thread never got going: free the
  // thread-agnostic parts only, never cross-thread DestroyWindow.
  if (overlay_) overlay_ = nullptr;
  if (memDc_) {
    if (dib_) {
      if (oldBmp_) SelectObject(memDc_, oldBmp_);
      DeleteObject(dib_);
      dib_ = nullptr;
      oldBmp_ = nullptr;
    }
    DeleteDC(memDc_);
    memDc_ = nullptr;
  }
  dibW_ = dibH_ = 0;
  bits_ = nullptr;
  font_ = nullptr;
  fontOwned_ = false;
  paintMode_ = false;
  ulwFailStreak_ = 0;
  Clear();
  util::Logger::Info("OverlayWindow: stopped");
}

void OverlayWindow::SetBoxes(std::vector<EspBox> boxes) {
  std::lock_guard<std::mutex> lock(mutex_);
  boxes_ = std::move(boxes);
}

void OverlayWindow::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  boxes_.clear();
  lines_.clear();
}

void OverlayWindow::SetCamera(const EspCamera& cam) {
  std::lock_guard<std::mutex> lock(mutex_);
  cam_ = cam;
}

EspSnapshot OverlayWindow::GetSnapshot() {
  std::lock_guard<std::mutex> lock(mutex_);
  EspSnapshot s;
  s.cam = cam_;
  s.boxes = boxes_;
  s.lines = lines_;
  return s;
}

void OverlayWindow::SetLines(std::vector<std::pair<std::string, uint32_t>> lines) {
  std::lock_guard<std::mutex> lock(mutex_);
  lines_ = std::move(lines);
}

void OverlayWindow::SetHudStyle(const HudStyle& st) {
  std::lock_guard<std::mutex> lock(mutex_);
  hudStyle_ = st;
}

OverlayDiag OverlayWindow::GetDiag() {
  OverlayDiag d;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    d = diag_;
  }
  d.running = running_.load();
  return d;
}

void OverlayWindow::Retarget(HWND mcWindow) {
  if (!mcWindow || !IsWindow(mcWindow)) return;
  std::lock_guard<std::mutex> lock(startMutex_);
  if (mcWindow != mcWindow_) {
    mcWindow_ = mcWindow;
    util::Logger::Info("OverlayWindow: retargeted to fresh game window");
  }
}

HWND OverlayWindow::CurrentWindow() {
  std::lock_guard<std::mutex> lock(startMutex_);
  return mcWindow_;
}

void OverlayWindow::FlashTest(int ms) {
  if (ms <= 0) {
    flashUntil_ = 0;  // Loop-thread only in practice; benign race
    std::lock_guard<std::mutex> lock(mutex_);
    diag_.flash = false;
    return;
  }
  flashUntil_ = GetTickCount64() + static_cast<unsigned long long>(ms);
  std::lock_guard<std::mutex> lock(mutex_);
  diag_.flash = true;
}

bool OverlayWindow::EnsureSurface(int w, int h) {
  if (memDc_ && dib_ && w == dibW_ && h == dibH_) return true;
  if (memDc_) {
    if (dib_) {
      if (oldBmp_) SelectObject(memDc_, oldBmp_);
      DeleteObject(dib_);
      dib_ = nullptr;
      oldBmp_ = nullptr;
    }
    DeleteDC(memDc_);
    memDc_ = nullptr;
  }
  dibW_ = w;
  dibH_ = h;
  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;  // top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  HDC screen = GetDC(nullptr);
  memDc_ = CreateCompatibleDC(screen);
  dib_ = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits_, nullptr, 0);
  ReleaseDC(nullptr, screen);
  if (!memDc_ || !dib_) return false;
  oldBmp_ = SelectObject(memDc_, dib_);
  return true;
}

void OverlayWindow::DrawFrame(int w, int h) {
  auto* px = static_cast<uint32_t*>(bits_);
  bool flash = GetTickCount64() < flashUntil_;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    diag_.flash = flash;
  }
  if (flash) {
    // Solid red test pattern (NOT the magenta key): if the user sees this,
    // the window path works and the problem is content/text rendering.
    // DIB uint32 0x00RRGGBB in memory order B,G,R,x: red = 0x00FF0000.
    std::fill(px, px + static_cast<size_t>(w) * static_cast<size_t>(h), 0x00FF0000u);
    SetBkMode(memDc_, TRANSPARENT);
    if (font_) SelectObject(memDc_, font_);
    const wchar_t* t = L"OVERLAY TEST - CAN YOU SEE THIS?";
    SetTextColor(memDc_, RGB(255, 255, 255));
    TextOutW(memDc_, 20, h / 2 - 10, t, 32);
    return;
  }
  // Magenta key == fully transparent.
  std::fill(px, px + static_cast<size_t>(w) * static_cast<size_t>(h), 0x00FF00FFu);

  std::vector<EspBox> boxes;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    boxes = boxes_;
  }

  if (!boxes.empty()) {
  HGDIOBJ oldPen = SelectObject(memDc_, GetStockObject(DC_PEN));
  HGDIOBJ oldBrush = SelectObject(memDc_, GetStockObject(HOLLOW_BRUSH));
  HPEN widePen = nullptr;
  int curW = 1;
  uint32_t curC = 0;
  for (const EspBox& b : boxes) {
    if (b.w <= 1.f || b.h <= 1.f) continue;
    int x0 = static_cast<int>(b.x), y0 = static_cast<int>(b.y);
    int x1 = static_cast<int>(b.x + b.w), y1 = static_cast<int>(b.y + b.h);
    if (x1 < 0 || y1 < 0 || x0 > w || y0 > h) continue;
    uint32_t c = b.color;
    int wantW = std::clamp(b.lineW, 1, 3);
    if (wantW != curW || (wantW > 1 && c != curC)) {
      if (widePen) {
        SelectObject(memDc_, GetStockObject(DC_PEN));
        DeleteObject(widePen);
        widePen = nullptr;
      }
      curW = 1;
      if (wantW > 1) {
        widePen = CreatePen(PS_SOLID, wantW,
                            RGB((c >> 16) & 255, (c >> 8) & 255, c & 255));
        if (widePen) {
          SelectObject(memDc_, widePen);
          curW = wantW;
          curC = c;
        }
      }
      if (curW == 1) {
        SetDCPenColor(memDc_, RGB((c >> 16) & 255, (c >> 8) & 255, c & 255));
      }
    } else if (curW == 1) {
      SetDCPenColor(memDc_, RGB((c >> 16) & 255, (c >> 8) & 255, c & 255));
    }
    Rectangle(memDc_, x0, y0, x1, y1);

    if (b.drawBar) {
      float hp = std::clamp(b.hp, 0.f, 1.f);
      int bx = x0 - 5;
      // Background + 1px black frame.
      HBRUSH bg = CreateSolidBrush(RGB(20, 20, 20));
      RECT rBg{bx - 1, y0 - 1, bx + 4, y1 + 1};
      FillRect(memDc_, &rBg, bg);
      DeleteObject(bg);
      int fgH = static_cast<int>((y1 - y0) * hp);
      if (fgH > 0) {
        int rr = static_cast<int>((1.f - hp) * 255.f);
        int gg = static_cast<int>(hp * 255.f);
        HBRUSH fg = CreateSolidBrush(RGB(rr, gg, 40));
        RECT rFg{bx, y1 - fgH, bx + 3, y1};
        FillRect(memDc_, &rFg, fg);
        DeleteObject(fg);
      }
    }
  }
  SelectObject(memDc_, oldPen);
  SelectObject(memDc_, oldBrush);
  if (widePen) DeleteObject(widePen);
  }

  // Classic, compact array list. Keep the game view clear by default.
  std::vector<std::pair<std::string, uint32_t>> lines;
  HudStyle st;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    lines = lines_;
    st = hudStyle_;
  }
  if (!st.enabled) return;

  SetBkMode(memDc_, TRANSPARENT);
  float wantScale = std::clamp(st.scale <= 0.f ? 1.f : st.scale, 0.5f, 2.0f);
  if (wantScale != fontScale_) {
    if (font_) {
      if (fontOwned_) DeleteObject(font_);
      font_ = nullptr;
      fontOwned_ = false;
    }
    int fontH = std::max(10, static_cast<int>(std::round(15 * wantScale)));
    font_ = CreateFontW(fontH, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Arial");
    fontOwned_ = (font_ != nullptr);
    if (!font_) font_ = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    fontScale_ = wantScale;
  }
  if (font_) SelectObject(memDc_, font_);

  struct Row {
    wchar_t text[256]{};
    int n = 0;
    int width = 0;
    uint32_t color = 0xFFFFFF;
  };
  Row rows[24];
  int rowCount = 0;
  for (const auto& [name, color] : lines) {
    if (rowCount >= 24 || name.empty() || name.size() > 200) continue;
    Row& row = rows[rowCount];
    int count = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, row.text, 256);
    if (count <= 1) continue;
    row.n = count - 1;
    row.color = color;
    SIZE size{};
    if (GetTextExtentPoint32W(memDc_, row.text, row.n, &size)) {
      row.width = size.cx;
    }
    ++rowCount;
  }
  const int rowHeight = std::max(12, static_cast<int>(std::round(18 * wantScale)));
  const int rowGap = std::max(1, static_cast<int>(std::round(wantScale)));
  const int padX = std::max(2, static_cast<int>(std::round(4 * wantScale)));
  const int barWidth = st.outline == 1
                           ? std::max(1, static_cast<int>(std::round(2 * wantScale)))
                           : 0;
  const int outlinePad = st.outline == 2 ? 1 : 0;
  const int listHeight = rowCount > 0
                             ? rowCount * rowHeight + (rowCount - 1) * rowGap
                             : 0;
  const int listY = std::clamp(st.y, 0, std::max(0, h - listHeight));
  const COLORREF hudColor =
      RGB((st.color >> 16) & 255, (st.color >> 8) & 255, st.color & 255);
  HBRUSH background = st.background ? CreateSolidBrush(RGB(0, 0, 0)) : nullptr;
  HPEN outlinePen = st.outline == 2 ? CreatePen(PS_SOLID, 1, hudColor) : nullptr;
  TEXTMETRICW metrics{};
  GetTextMetricsW(memDc_, &metrics);

  for (int i = 0; i < rowCount; ++i) {
    const Row& row = rows[i];
    const int rowWidth = row.width + padX * 2 + barWidth + outlinePad * 2;
    int rowX = st.alignRight ? w - st.x - rowWidth : st.x;
    rowX = std::clamp(rowX, 0, std::max(0, w - rowWidth));
    const int rowY = listY + i * (rowHeight + rowGap);
    const int textLeft = rowX + outlinePad;
    const int textRight = rowX + rowWidth - outlinePad;

    if (background) {
      RECT rect{rowX, rowY, rowX + rowWidth, rowY + rowHeight};
      FillRect(memDc_, &rect, background);
    }
    if (outlinePen) {
      HGDIOBJ oldPen = SelectObject(memDc_, outlinePen);
      HGDIOBJ oldBrush = SelectObject(memDc_, GetStockObject(HOLLOW_BRUSH));
      Rectangle(memDc_, rowX, rowY, rowX + rowWidth, rowY + rowHeight);
      SelectObject(memDc_, oldPen);
      SelectObject(memDc_, oldBrush);
    }
    if (barWidth > 0) {
      const int barX = st.alignRight ? textRight - barWidth : textLeft;
      HBRUSH bar = CreateSolidBrush(hudColor);
      if (bar) {
        RECT rect{barX, rowY + 2, barX + barWidth, rowY + rowHeight - 2};
        FillRect(memDc_, &rect, bar);
        DeleteObject(bar);
      }
    }

    const int textX = st.alignRight
                          ? textRight - padX - barWidth - row.width
                          : textLeft + padX + barWidth;
    const int textY =
        rowY + std::max(0, (rowHeight - static_cast<int>(metrics.tmHeight)) / 2);
    const COLORREF textColor =
        RGB((row.color >> 16) & 255, (row.color >> 8) & 255, row.color & 255);
    if (st.shadow) {
      SetTextColor(memDc_, RGB(0, 0, 0));
      TextOutW(memDc_, textX + 1, textY + 1, row.text, row.n);
    }
    SetTextColor(memDc_, textColor);
    TextOutW(memDc_, textX, textY, row.text, row.n);
  }

  constexpr wchar_t watermark[] = L"a05777 Client v1.0.0";
  constexpr int watermarkLength =
      static_cast<int>(sizeof(watermark) / sizeof(watermark[0]) - 1);
  SetTextColor(memDc_, RGB(0, 0, 0));
  TextOutW(memDc_, 9, 9, watermark, watermarkLength);
  SetTextColor(memDc_, RGB(255, 255, 255));
  TextOutW(memDc_, 8, 8, watermark, watermarkLength);

  if (outlinePen) DeleteObject(outlinePen);
  if (background) DeleteObject(background);
}

void OverlayWindow::Loop() {
  using namespace std::chrono_literals;
  util::Logger::Info("OverlayWindow: draw thread entered");
  // The window is owned by THIS thread (created below): same-thread
  // SetWindowPos/UpdateLayeredWindow/DestroyWindow never block.
  paintMode_ = false;
  ulwFailStreak_ = 0;
  if (!CreateOwnedWindow()) {
    running_ = false;
    return;
  }
  bool visible = false;
  unsigned long long lastSkipLog = 0;
  bool loggedSurfFail = false;
  auto skipLog = [&](const char* why, int w, int h) {
    unsigned long long now = GetTickCount64();
    if (now - lastSkipLog > 5000) {
      lastSkipLog = now;
      char msg[224];
      std::snprintf(msg, sizeof(msg),
                    "OverlayWindow: skip (%s) mc=%p %dx%d", why,
                    static_cast<void*>(mcWindow_), w, h);
      util::Logger::Warn(msg);
    }
  };
  while (running_.load()) {
    // Pump messages so the window never ghosts. This thread owns overlay_,
    // so cross-thread SendMessage calls against it complete here.
    MSG msg{};
    while (PeekMessageW(&msg, overlay_, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      diag_.stage = 1;
    }
    HWND game = nullptr;
    {
      std::lock_guard<std::mutex> lock(startMutex_);
      game = mcWindow_;
    }

    if (!game || !IsWindow(game) || IsIconic(game)) {
      if (visible) {
        ShowWindow(overlay_, SW_HIDE);
        visible = false;
      }
      {
        std::lock_guard<std::mutex> lock(mutex_);
        diag_.hasWindow = game != nullptr;
        diag_.windowValid = (game && IsWindow(game)) ? true : false;
        diag_.minimized = (game && IsWindow(game) && IsIconic(game));
        diag_.stage = 2;
      }
      skipLog(!game || !IsWindow(game) ? "bad window" : "minimized", 0, 0);
      std::this_thread::sleep_for(500ms);
      continue;
    }

    RECT cr{};
    GetClientRect(game, &cr);
    POINT tl{cr.left, cr.top}, br{cr.right, cr.bottom};
    ClientToScreen(game, &tl);
    ClientToScreen(game, &br);
    int w = br.x - tl.x, h = br.y - tl.y;
    unsigned dpi = QueryWindowDpi(game);
    bool fullscreen = IsFullscreenSized(game, w, h);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      diag_.hasWindow = game != nullptr;
      diag_.windowValid = true;
      diag_.minimized = false;
      diag_.x = tl.x;
      diag_.y = tl.y;
      diag_.w = w;
      diag_.h = h;
      diag_.dpi = dpi;
      diag_.fullscreen = fullscreen;
    }
    if (w < 50 || h < 50) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        diag_.stage = 3;
      }
      skipLog("tiny client area", w, h);
      std::this_thread::sleep_for(200ms);
      continue;
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      diag_.stage = 4;
    }
    SetWindowPos(overlay_, HWND_TOPMOST, tl.x, tl.y, w, h,
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    visible = true;
    static bool loggedGeom = false;
    static bool loggedFail = false;

    if (paintMode_) {
      // Fallback presentation (ULW err 87, see §9.1): classic colorkey BitBlt.
      // UpdateWindow issues WM_PAINT synchronously on this (owner) thread.
      if (EnsureSurface(w, h)) DrawFrame(w, h);
      InvalidateRect(overlay_, nullptr, FALSE);
      UpdateWindow(overlay_);
      {
        std::lock_guard<std::mutex> lock(mutex_);
        diag_.stage = 6;
        diag_.iters++;
        diag_.paintMode = true;
      }
      if (!loggedGeom) {
        loggedGeom = true;
        char msg[224];
        std::snprintf(msg, sizeof(msg),
                      "OverlayWindow: frame %dx%d at (%d,%d) dpi=%u via PAINT mode",
                      w, h, tl.x, tl.y, dpi);
        util::Logger::Info(msg);
      }
    } else if (EnsureSurface(w, h)) {
      DrawFrame(w, h);
      {
        std::lock_guard<std::mutex> lock(mutex_);
        diag_.stage = 5;
      }
      HDC screen = GetDC(nullptr);      POINT dst{tl.x, tl.y}, src{0, 0};
      SIZE size{w, h};
      // Non-NULL blend + no prior SetLayeredWindowAttributes: err-87
      // hardening (AlphaFormat=0 keeps colorkey semantics).
      BLENDFUNCTION bf{};
      bf.BlendOp = AC_SRC_OVER;
      bf.SourceConstantAlpha = 255;
      bf.AlphaFormat = 0;
      BOOL ulwOk = UpdateLayeredWindow(overlay_, screen, &dst, &size, memDc_, &src, kKey,
                                       &bf, ULW_COLORKEY);
      DWORD ulwErr = ulwOk ? 0 : GetLastError();
      ReleaseDC(nullptr, screen);
      {
        std::lock_guard<std::mutex> lock(mutex_);
        diag_.stage = 6;
        if (ulwOk) diag_.iters++;
      }
      if (!loggedGeom && ulwOk) {
        loggedGeom = true;
        char msg[224];
        std::snprintf(msg, sizeof(msg),
                      "OverlayWindow: frame %dx%d at (%d,%d) dpi=%u%s", w, h, tl.x,
                      tl.y, dpi, fullscreen ? " FULLSCREEN-SIZED" : "");
        util::Logger::Info(msg);
        if (fullscreen) {
          util::Logger::Warn(
              "OverlayWindow: game covers full monitor; exclusive fullscreen "
              "cannot be overlaid - use windowed/borderless");
        }
        if (dpi != 96) {
          char dm[128];
          std::snprintf(dm, sizeof(dm),
                        "OverlayWindow: DPI=%u (scaling %g%%), aware mode on", dpi,
                        dpi / 96.0 * 100.0);
          util::Logger::Info(dm);
        }
      }
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ulwOk) {
          diag_.ulwFailed = true;
          diag_.lastUwlErr = ulwErr;
        } else {
          ulwFailStreak_ = 0;
        }
      }
      if (!ulwOk) {
        if (++ulwFailStreak_ == 1) {
          LONG ex = GetWindowLongW(overlay_, GWL_EXSTYLE);
          char msg[320];
          std::snprintf(
              msg, sizeof(msg),
              "OverlayWindow: ULW first fail err=%lu exstyle=0x%lx hwnd=%p "
              "memDc=%p dib=%p bits=%p %dx%d dst=(%d,%d)",
              static_cast<unsigned long>(ulwErr),
              static_cast<unsigned long>(ex), static_cast<void*>(overlay_),
              static_cast<void*>(memDc_), static_cast<void*>(dib_), bits_, w, h,
              tl.x, tl.y);
          util::Logger::Error(msg);
        }
        if (!loggedFail) {
          loggedFail = true;
          char msg[128];
          std::snprintf(msg, sizeof(msg),
                        "OverlayWindow: UpdateLayeredWindow failed: %lu",
                        static_cast<unsigned long>(ulwErr));
          util::Logger::Error(msg);
        }
        if (ulwFailStreak_ >= 5 && !paintMode_) {
          paintMode_ = true;
          SetLayeredWindowAttributes(overlay_, kKey, 0, LWA_COLORKEY);
          util::Logger::Warn(
              "OverlayWindow: ULW failed 5x, switching to PAINT present mode");
          std::lock_guard<std::mutex> lock(mutex_);
          diag_.paintMode = true;
        }
      }
    } else if (!loggedSurfFail) {
      loggedSurfFail = true;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        diag_.surfFailed = true;
      }
      util::Logger::Error("OverlayWindow: EnsureSurface failed (DIB creation)");
    }
    // Heartbeat: proves the draw thread is alive and what it holds.
    {
      unsigned long long now = GetTickCount64();
      static unsigned long long lastBeat = 0;
      if (now - lastBeat > 10000) {
        lastBeat = now;
        size_t nBoxes = 0, nLines = 0;
        unsigned long long iters = 0;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          nBoxes = boxes_.size();
          nLines = lines_.size();
          diag_.beats++;
          iters = diag_.iters;
        }
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "OverlayWindow: beat visible=%d boxes=%zu lines=%zu %dx%d "
                      "dpi=%u%s %s iters=%llu",
                      visible ? 1 : 0, nBoxes, nLines, w, h, dpi,
                      fullscreen ? " FULL" : "", paintMode_ ? "PAINT" : "ULW",
                      iters);
        util::Logger::Info(msg);
      }
    }
    std::this_thread::sleep_for(33ms);
  }
  DestroyOwnedWindow();  // same thread as creation: cannot block
  util::Logger::Info("OverlayWindow: draw thread exited");
}

}  // namespace render
}  // namespace raven
