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

// Port of keystrokesmod/module/impl/combat/Velocity.java.
// Java hooks LivingUpdateEvent; here we poll hurtTime == maxHurtTime on the
// player each logic tick and scale motion the same way.
namespace raven {
namespace modules {

class Velocity final : public core::Module {
public:
  Velocity();
  void OnUpdate() override;

  core::SliderSetting horizontal{"Horizontal", 90.0, 0.0, 100.0, 1.0};
  core::SliderSetting vertical{"Vertical", 100.0, 0.0, 100.0, 1.0};
  core::SliderSetting chance{"Chance", 100.0, 0.0, 100.0, 1.0};
  core::BoolSetting onlyTargeting{"Only while targeting", false};
  core::BoolSetting disableS{"Disable while holding S", false};
};

}  // namespace modules
}  // namespace raven
