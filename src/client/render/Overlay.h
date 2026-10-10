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
#include <atomic>
#include <cstdint>

// Overlay scaffold. Phase 2 replaces Begin/End/Text with a real ImGui
// (opengl2 + win32 backends) ClickGUI + ESP/HUD passes.
// Scaffold only counts frames so OnRender wiring can be smoke-tested.
namespace raven {
namespace render {

class Overlay {
public:
  static Overlay& Instance();
  void BeginFrame();
  void Text(int x, int y, const char* text);
  void EndFrame();
  uint64_t Frames() const { return frames_.load(std::memory_order_relaxed); }

private:
  Overlay() = default;
  std::atomic<uint64_t> frames_{0};
};

}  // namespace render
}  // namespace raven
