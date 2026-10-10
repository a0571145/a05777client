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
#include "core/Module.h"
#include "core/Setting.h"

// Port of keystrokesmod/module/impl/render/HUD.java (ArrayList overlay).
// Draws a compact enabled-module array list through the native overlay.
namespace raven {
namespace modules {

class Hud final : public core::Module {
public:
  Hud();
  void OnEnable() override;
  void OnUpdate() override;
  void OnRender() override;
  void OnDisable() override;

  core::BoolSetting alphabeticalSort{"Alphabetical sort", false};
  core::SliderSetting scale{"Scale", 1.0, 0.5, 2.0, 0.05};
  core::ColorSetting hudColor{"Color", 255, 255, 255};
  core::BoolSetting drawBackground{"Draw background", false};
  core::BoolSetting textShadow{"Text shadow", true};
  core::ModeSetting outline{"Outline", {"None", "Side", "Full"}, 0};
  core::BoolSetting alignRight{"Align right", true};
  core::SliderSetting posX{"Pos X", 8.0, 0.0, 1920.0, 1.0};
  core::SliderSetting posY{"Pos Y", 70.0, 0.0, 1080.0, 1.0};
  core::BoolSetting lowercase{"Lowercase", false};
};

}  // namespace modules
}  // namespace raven
