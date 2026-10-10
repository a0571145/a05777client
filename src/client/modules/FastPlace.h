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

// Port of keystrokesmod/module/impl/player/FastPlace.java (lite).
// Sets Minecraft.rightClickDelayTimer while the use input is held and the
// gates pass. The C08 air-place cancel needs a packet hook and is omitted;
// block/held-item blacklists need list settings (not in Setting.h) and are
// omitted too.
namespace raven {
namespace modules {

class FastPlace final : public core::Module {
 public:
  FastPlace();
  void OnUpdate() override;
  void OnDisable() override;

  core::SliderSetting tickDelay{"Tick delay", 1.0, 0.0, 3.0, 1.0};
  core::SliderSetting activation{"Activation time", 0.0, 0.0, 100.0, 5.0};
  core::BoolSetting blocksOnly{"Blocks only", true};
  core::BoolSetting pitchCheck{"Pitch check", false};

 private:
  static long long NowMs();
  long long rightStartMs_ = 0;
};

}  // namespace modules
}  // namespace raven
