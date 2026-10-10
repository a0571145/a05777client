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

#include "modules/Hud.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "core/Client.h"
#include "hooks/Hooks.h"
#include "render/OverlayWindow.h"

namespace raven {
namespace modules {

Hud::Hud() : core::Module("HUD", core::Category::Render, 0) {
  RegisterBool(&alphabeticalSort);
  RegisterSlider(&scale);
  RegisterColor(&hudColor);
  RegisterBool(&drawBackground);
  RegisterBool(&textShadow);
  RegisterMode(&outline);
  RegisterBool(&alignRight);
  RegisterSlider(&posX);
  RegisterSlider(&posY);
  RegisterBool(&lowercase);
}

void Hud::OnEnable() {
  render::OverlayWindow::Instance().Start(hooks::Hooks::Instance().Window());
}

void Hud::OnUpdate() {
  // Overlay may not have existed at enable time (game window late).
  // Refresh the cached game window: it is null when injected before the game
  // window appears, which used to park the overlay at 0x0 forever.
  if (!render::OverlayWindow::Instance().IsRunning()) {
    HWND fresh = hooks::Hooks::Instance().RefreshWindow();
    if (fresh) render::OverlayWindow::Instance().Start(fresh);
  }
}

void Hud::OnRender() {
  auto mods = core::Client::Instance().Modules().Snapshot();
  std::vector<std::string> names;
  for (const auto& m : mods) {
    if (m->IsEnabled() && m->Name() != "HUD") names.push_back(m->Name());
  }
  if (alphabeticalSort.value) {
    std::sort(names.begin(), names.end());
  } else {
    std::sort(names.begin(), names.end(),
              [](const std::string& a, const std::string& b) {
                return a.size() == b.size() ? a < b : a.size() > b.size();
              });
  }
  if (names.size() > 24) names.resize(24);
  if (lowercase.value) {
    for (auto& n : names)
      std::transform(n.begin(), n.end(), n.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  }
  uint32_t col = (static_cast<uint32_t>(hudColor.r & 255) << 16) |
                 (static_cast<uint32_t>(hudColor.g & 255) << 8) |
                 static_cast<uint32_t>(hudColor.b & 255);
  std::vector<std::pair<std::string, uint32_t>> lines;
  for (const auto& n : names) lines.emplace_back(n, col);
  render::HudStyle st;
  st.color = col;
  st.background = drawBackground.value;
  st.shadow = textShadow.value;
  st.outline = outline.index;
  st.alignRight = alignRight.value;
  st.enabled = true;
  st.x = static_cast<int>(posX.value);
  st.y = static_cast<int>(posY.value);
  st.scale = static_cast<float>(scale.value);
  render::OverlayWindow::Instance().SetHudStyle(st);
  render::OverlayWindow::Instance().SetLines(std::move(lines));
}

void Hud::OnDisable() {
  render::HudStyle st;
  render::OverlayWindow::Instance().SetHudStyle(st);
  render::OverlayWindow::Instance().SetLines({});
  render::OverlayWindow::Instance().Stop();
}

}  // namespace modules
}  // namespace raven
