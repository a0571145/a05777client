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

// Port of keystrokesmod/module/impl/movement/KeepSprint.java.
// Java is called from the attack Mixin on every landed hit; here we
// edge-detect the crosshair target's hurtTime hitting maxHurtTime and apply
// the same motion multipliers. The KillAura/Reach branches are simplified:
// without those modules, "Only reduce reach hits" measures eyes-distance.
namespace raven {
namespace modules {

class KeepSprint final : public core::Module {
public:
  KeepSprint();
  void OnUpdate() override;
  void OnDisable() override;

  core::SliderSetting slow{"Slow %", 40.0, 0.0, 40.0, 1.0};
  core::BoolSetting stopSprint{"Stop Sprint", true};
  core::BoolSetting disableJump{"Disable while jumping", false};
  core::BoolSetting reachHits{"Only reduce reach hits", false};

private:
  void* lastTarget_ = nullptr;  // weak global ref, owned here
  int lastHurt_ = -1;
  void ForgetTarget(void* envPtr);
};

}  // namespace modules
}  // namespace raven
