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

// Port of keystrokesmod/module/impl/combat/TPAura.java.
// On our own hurt edge the offset re-randomizes; each tick the first player
// in range is teleported onto (offset) and attacked via the controller.
// Friends/AntiBot gates need absent modules and are omitted.
namespace raven {
namespace modules {

class TPAura final : public core::Module {
public:
  TPAura();
  void OnEnable() override;
  void OnUpdate() override;

  core::SliderSetting range{"Range", 0.0, 0.0, 50.0, 1.0};
  core::BoolSetting weaponOnly{"Weapon only", false};

private:
  double ox_ = 0.0, oy_ = 0.0, oz_ = 0.0;
  void RandomizeOffset();
};

}  // namespace modules
}  // namespace raven
