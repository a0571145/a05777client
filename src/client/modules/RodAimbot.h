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

// Port of keystrokesmod/module/impl/combat/RodAimbot.java.
// Java cancels the vanilla right-click and re-fires manually; without an
// event hook we cannot cancel, so this port is aim-only: on RMB edge with a
// rod it tracks the target with predicted rotations while held.
namespace raven {
namespace modules {

class RodAimbot final : public core::Module {
public:
  RodAimbot();
  void OnUpdate() override;
  void OnDisable() override;

  core::SliderSetting fov{"FOV", 180.0, 30.0, 360.0, 4.0};
  core::SliderSetting predicted{"Predicted ticks", 5.0, 0.0, 20.0, 1.0};
  core::SliderSetting distance{"Distance", 6.0, 3.0, 30.0, 0.5};
  core::BoolSetting aimInvis{"Aim invis", false};
  core::BoolSetting ignoreTeammates{"Ignore teammates", false};

private:
  bool rotating_ = false;
  void* target_ = nullptr;  // weak global ref
  long long stopAtMs_ = 0;
  bool prevRmb_ = false;
  void ForgetTarget(void* envPtr);
};

}  // namespace modules
}  // namespace raven
