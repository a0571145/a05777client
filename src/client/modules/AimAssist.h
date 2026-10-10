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
#include "core/Module.h"

// Port of keystrokesmod/module/impl/combat/AimAssist.java (aim core).
// Target select + smoothed direct rotations each tick; Silent behaves as
// Normal (no rotation-event hook exists here). keepMoveDirection needs the
// movement hook and is omitted.
namespace raven {
namespace modules {

class AimAssist final : public core::Module {
public:
  AimAssist();
  void OnUpdate() override;

  core::ModeSetting mode{"Mode", {"Normal", "Silent"}, 0};
  core::SliderSetting speed{"Speed", 10.0, 1.0, 30.0, 1.0};
  core::SliderSetting multiH{"Multipoint horizontal", 0.0, 0.0, 100.0, 1.0};
  core::SliderSetting multiV{"Multipoint vertical", 0.0, 0.0, 100.0, 1.0};
  core::SliderSetting randomization{"Randomization", 50.0, 0.0, 100.0, 1.0};
  core::SliderSetting fov{"FOV", 90.0, 15.0, 360.0, 1.0};
  core::SliderSetting range{"Range", 4.5, 0.0, 5.0, 0.1};
  core::ModeSetting sortMode{"Sort", {"Health", "Angle", "Hurt time", "Distance"}, 1};
  core::BoolSetting aimInvis{"Aim invis", false};
  core::BoolSetting requireMouse{"Require left mouse", true};
  core::BoolSetting ignoreTeammates{"Ignore teammates", true};
  core::BoolSetting ignoreWalls{"Ignore behind walls", false};
  core::BoolSetting ignoreEntities{"Ignore behind entities", false};
  core::BoolSetting stopBreaking{"Stop when breaking", false};
  core::SliderSetting hoverDelay{"Hover delay", 100.0, 0.0, 500.0, 10.0};
  core::BoolSetting weaponOnly{"Weapon only", false};

private:
  // Sticky aim point: offsets re-rolled only on target change.
  void* lastTarget_ = nullptr;  // weak global ref
  double aimOX_ = 0.0, aimOY_ = 0.0, aimOZ_ = 0.0;
  double aimJY_ = 0.0, aimJP_ = 0.0;
  long long lastMs_ = 0;
  void ForgetTarget(void* envPtr);

public:
  void OnDisable() override;
};

}  // namespace modules
}  // namespace raven
