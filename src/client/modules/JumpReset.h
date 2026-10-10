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

// Port of keystrokesmod/module/impl/combat/JumpReset.java.
// Hurt edge (maxHurtTime increase) + gates, then taps the jump key until the
// jump input clears. Potion gate needs tier-2; precise look-direction FOV is
// approximated by the crosshair-aim gate.
namespace raven {
namespace modules {

class JumpReset final : public core::Module {
public:
  JumpReset();
  void OnUpdate() override;
  void OnDisable() override;

  core::SliderSetting chance{"Chance", 80.0, 0.0, 100.0, 1.0};
  core::BoolSetting requireMouse{"Require mouse down", false};
  core::BoolSetting requireForward{"Require moving forward", true};
  core::BoolSetting requireAim{"Require aim", true};

private:
  bool setJump_ = false;
  bool ignoreNext_ = false;
  int lastHurt_ = 0;
  float lastFall_ = 0.f;
};

}  // namespace modules
}  // namespace raven
