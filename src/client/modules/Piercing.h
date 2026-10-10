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

// Port of keystrokesmod/module/impl/combat/Piercing.java (override part).
// Every tick the crosshair is re-raycast and objectMouseOver is replaced when
// the module's conditions hold. Hurt/health sort collapses to closest-hit
// (single-raycast limitation, documented).
namespace raven {
namespace modules {

class Piercing final : public core::Module {
public:
  Piercing();
  void OnUpdate() override;

  core::BoolSetting ignoreBlocks{"Ignore blocks", false};
  core::BoolSetting ignoreNonPlayer{"Ignore non-players", true};
  core::BoolSetting ignoreTeammates{"Ignore teammates", true};
  core::BoolSetting weaponOnly{"Weapon only", false};
  core::BoolSetting insideOnly{"Inside hitbox only", false};
};

}  // namespace modules
}  // namespace raven
