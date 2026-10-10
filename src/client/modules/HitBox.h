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

// Port of keystrokesmod/module/impl/combat/HitBox.java (targeting part).
// LMB edge runs an expanded-box raycast and overwrites objectMouseOver.
// The GL "show hitbox" pass is omitted (needs in-context GL); friends check
// needs the Relationships module (absent in Batch 2).
namespace raven {
namespace modules {

class HitBox final : public core::Module {
public:
  HitBox();
  void OnUpdate() override;

  // Public for Reach::getEntity (mirrors Java's static getExpand).
  core::SliderSetting multiplier{"Multiplier", 1.2, 1.0, 5.0, 0.05};
  core::BoolSetting playersOnly{"Players only", true};
  core::BoolSetting weaponOnly{"Weapon only", false};

  double Expand() const { return multiplier.value; }

private:
  bool prevLmb_ = false;
};

}  // namespace modules
}  // namespace raven
