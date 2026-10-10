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

// Port of keystrokesmod/module/impl/combat/Reach.java.
// Java rewrites objectMouseOver on left-click; here an LMB edge (100 Hz poll)
// runs the shared raycast and overwrites objectMouseOver + pointedEntity.
// Uses HitBox multiplier when that module is present (mirrors Java statics).
namespace raven {
namespace modules {

class Reach final : public core::Module {
public:
  Reach();
  void OnUpdate() override;

  core::SliderSetting minReach{"Min", 3.1, 3.0, 6.0, 0.05};
  core::SliderSetting maxReach{"Max", 3.3, 3.0, 6.0, 0.05};
  core::BoolSetting weaponOnly{"Weapon only", false};
  core::BoolSetting movingOnly{"Moving only", false};
  core::BoolSetting sprintOnly{"Sprint only", false};
  core::BoolSetting throughBlocks{"Hit through blocks", false};

private:
  bool prevLmb_ = false;
};

}  // namespace modules
}  // namespace raven
