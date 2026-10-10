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

// Port of keystrokesmod/module/impl/movement/Speed.java.
// Vanilla mode is a faithful port. Float mode ports the ground-sustain core
// (local tick counters + real potion levels); the ModuleUtils interplay
// (SafeWalk/BHop/LongJump gates) and snow/carpet special-cases are omitted.
namespace raven {
namespace modules {

class Speed final : public core::Module {
public:
  Speed();
  void OnUpdate() override;

  core::ModeSetting mode{"Mode", {"Vanilla", "Float"}, 0};
  core::SliderSetting multiplier{"Multiplier", 1.2, 1.0, 1.5, 0.01};
  core::BoolSetting onlyForward{"Only forward", false};
  core::BoolSetting onlyStrafe{"Only strafe", false};

private:
  int groundTicks_ = 0;
  int stillTicks_ = 0;
  bool requireJump_ = false;
  static constexpr double kLevels[5] = {0.2, 0.22, 0.28, 0.29, 0.3};
};

}  // namespace modules
}  // namespace raven
