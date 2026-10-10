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

// Port of keystrokesmod/module/impl/movement/BHop.java.
// Strafe + Ground/9/8/7-tick modes with potion-aware speed, liquid/sneak
// gates and air-damp. Omitted: damage-boost group (dead in this Raven
// version), rotate-yaw (consumed by absent RotationHelper), LongJump/
// KillAura cross-checks.
namespace raven {
namespace modules {

class BHop final : public core::Module {
public:
  BHop();
  void OnUpdate() override;
  void OnDisable() override;

  core::ModeSetting mode{"Mode", {"Strafe", "Ground", "9 tick", "8 tick", "7 tick"}, 0};
  // NOTE: Java default (2.0) sits above max (1.2); kept verbatim.
  core::SliderSetting speed{"Speed", 2.0, 0.8, 1.2, 0.01};
  core::BoolSetting liquidDisable{"Disable in liquid", true};
  core::BoolSetting sneakDisable{"Disable while sneaking", true};
  core::BoolSetting jumpMoving{"Only jump when moving", true};

private:
  bool hopping_ = false;
  bool lowhopAir_ = false;
  int airTicks_ = 0;
  bool setSlow_ = false;
};

}  // namespace modules
}  // namespace raven
