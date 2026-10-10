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
#include <chrono>
#include <vector>
#include "core/Module.h"

// Batch-1 subset of keystrokesmod/module/impl/render/PlayerESP.java:
// 2D box + health bar on the click-through GDI overlay, fed by JNI snapshots.
// Deferred to later batches: outline/shaded/ring/skeleton passes, team color,
// red-on-damage, nametags (all need in-context GL work).
namespace raven {
namespace modules {

class PlayerESP final : public core::Module {
public:
  PlayerESP();
  void OnEnable() override;
  void OnUpdate() override;
  void OnDisable() override;

  core::BoolSetting box{"Box", true};
  core::BoolSetting healthBar{"Health bar", true};
  core::ColorSetting color{"Color", 0, 255, 0};
  core::BoolSetting rainbow{"Rainbow", false};
  core::BoolSetting renderSelf{"Render self", false};
  core::BoolSetting showInvis{"Show invis", true};
  core::SliderSetting maxDistance{"Max distance", 128.0, 32.0, 256.0, 8.0};
  core::SliderSetting lineWidth{"Line width", 1.0, 1.0, 3.0, 1.0};

private:
  std::chrono::steady_clock::time_point lastCollect_{};
  bool hadWorld_ = false;  // hysteresis: only clear boxes on world loss
};

}  // namespace modules
}  // namespace raven
