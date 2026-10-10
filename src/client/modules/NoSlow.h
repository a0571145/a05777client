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

// Port of keystrokesmod/module/impl/movement/NoSlow.java.
// Java hooks movement-input + pre-update events via Mixin; here we poll:
//  - input rescale replaces the MixinMovementInputFromOptions hook
//    (moveF/Strafe are restored from x0.2 vanilla slow to the Slow % factor)
//  - Beta-mode C09/C07 packets are sent via JNI like onPreUpdate
// Dead Java code (setJump, never armed) is intentionally omitted.
namespace raven {
namespace modules {

class NoSlow final : public core::Module {
public:
  NoSlow();
  void OnUpdate() override;
  void OnDisable() override;

  core::ModeSetting mode{"Mode", {"Vanilla", "Beta"}, 0};
  core::SliderSetting slowed{"Slow %", 80.0, 0.0, 80.0, 1.0};
  core::BoolSetting disableBow{"Disable bow", false};
  core::BoolSetting disablePotions{"Disable potions", false};
  core::BoolSetting swordOnly{"Sword only", false};
  core::BoolSetting vanillaSword{"Vanilla sword", false};

  // Mirrors Java's public noSlowing flag (set while actively un-slowing).
  bool noSlowing = false;

private:
  // getSlowed() port: motion factor while using an item (0.2 == vanilla).
  float SlowedFactor(void* envPtr, void* playerPtr);
};

}  // namespace modules
}  // namespace raven
