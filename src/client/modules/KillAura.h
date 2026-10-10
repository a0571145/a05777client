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

// Batch-2 subset of keystrokesmod/module/impl/combat/KillAura.java (564 lines).
// Implemented: single-target select (distance/health/hurt/yaw sort), FOV gate,
// CPS swing+attack via PlayerControllerMP, Lock-view rotations with smoothing,
// mobs/invis/teammate(use-item/inventory/mining/mouse/weapon gates.
// Simplified vs Java: one target (no switch-delay/multi-target/hitMap),
// Silent behaves as Lock view (no pre-motion packet hook exists here),
// teammates via display-name color prefix, mobs = monster package only.
namespace raven {
namespace modules {

class KillAura final : public core::Module {
public:
  KillAura();
  void OnEnable() override;
  void OnDisable() override;
  void OnUpdate() override;

  core::SliderSetting targetCPS{"Target CPS", 10.0, 1.0, 20.0, 0.5};
  core::SliderSetting fov{"FOV", 360.0, 30.0, 360.0, 4.0};
  core::SliderSetting attackRange{"Range (attack)", 3.0, 3.0, 6.0, 0.05};
  core::SliderSetting swingRange{"Range (swing)", 4.5, 3.0, 8.0, 0.05};
  core::SliderSetting aimRange{"Range (aim)", 4.5, 3.0, 8.0, 0.05};
  core::ModeSetting rotationMode{"Rotation mode", {"Silent", "Lock view", "None"}, 0};
  core::ModeSetting sortMode{"Sort mode", {"Distance", "Health", "Hurt time", "Yaw"}, 0};
  core::SliderSetting speed{"Speed", 10.0, 1.0, 30.0, 1.0};
  core::BoolSetting targetInvis{"Target invis", true};
  core::BoolSetting attackMobs{"Attack mobs", false};
  core::BoolSetting throughWalls{"Hit through walls", false};
  core::BoolSetting disableInInventory{"Disable in inventory", true};
  core::BoolSetting disableWhileMining{"Disable while mining", false};
  core::BoolSetting ignoreTeammates{"Ignore teammates", true};
  core::BoolSetting notUsingItem{"Not using item", false};
  core::BoolSetting requireMouseDown{"Require mouse down", false};
  core::BoolSetting weaponOnly{"Weapon only", false};

private:
  long long nextClickMs_ = 0;
};

}  // namespace modules
}  // namespace raven
