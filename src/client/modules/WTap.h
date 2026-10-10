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

// Port of keystrokesmod/module/impl/combat/WTap.java.
// Java reacts to AttackEntityEvent; here our own landed hits are edge-detected
// on the crosshair target (hurtTime hitting max). On fire, Sprint is released
// for Delay-until ms (see Sprint cooperation), then resumes after cooldown.
namespace raven {
namespace modules {

class WTap final : public core::Module {
public:
  WTap();
  void OnEnable() override;
  void OnUpdate() override;
  void OnDisable() override;

  // True while a reset is holding sprint off (consumed by Sprint).
  bool IsResetting() const { return resetting_; }

  core::SliderSetting chance{"Chance", 100.0, 0.0, 100.0, 1.0};
  core::SliderSetting delayBetween{"Delay between reset", 300.0, 0.0, 1000.0, 10.0};
  core::SliderSetting delayUntil{"Delay until reset", 150.0, 0.0, 1000.0, 10.0};
  core::BoolSetting playersOnly{"Players only", true};

private:
  long long pendingAtMs_ = 0;
  long long lastResetMs_ = 0;
  bool waitingRestart_ = false;
  bool wasSprinting_ = false;
  bool resetting_ = false;
  void* lastTarget_ = nullptr;
  int lastHurt_ = -1;
  void ForgetTarget(void* envPtr);
};

}  // namespace modules
}  // namespace raven
