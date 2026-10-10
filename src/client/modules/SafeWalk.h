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

// Port of keystrokesmod/module/impl/player/SafeWalk.java.
// Sneak is forced through the sneak KeyBinding at edges (edge test via
// block scan); GUI-open resync needs the Forge event and is omitted.
namespace raven {
namespace modules {

class SafeWalk final : public core::Module {
public:
  SafeWalk();
  void OnUpdate() override;
  void OnDisable() override;

  core::SliderSetting sneakDelay{"Sneak delay", 0.0, 0.0, 20.0, 1.0};
  core::SliderSetting motion{"Motion", 1.0, 0.5, 1.2, 0.01};
  core::BoolSetting blocksOnly{"Blocks only", true};
  core::BoolSetting disableOnForward{"Disable on forward", false};
  core::BoolSetting pitchCheck{"Pitch check", false};
  core::BoolSetting sneak{"Sneak", false};

private:
  bool SettingsMet(void* envPtr, void* playerPtr);
  void SetSneak(void* envPtr, bool state);

  int unsneakTicks_ = 0;
  bool sneaking_ = false;
};

}  // namespace modules
}  // namespace raven
