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

// Port of keystrokesmod/module/impl/combat/AutoClicker.java.
// Java drives clicks from PrePlayerInteract/RenderTick events; here the 100 Hz
// worker tick runs the same CPS scheduler and synthesizes attack presses via
// KeyBinding.onTick + setKeyBindState (the LWJGL Mouse.setButton hack has no
// JNI equivalent and is intentionally omitted). The container/inventory path
// uses windowClick like the Java inventory handler.
namespace raven {
namespace modules {

class AutoClicker final : public core::Module {
public:
  AutoClicker();
  void OnEnable() override;
  void OnDisable() override;
  void OnUpdate() override;

  core::SliderSetting targetCPS{"Target CPS", 10.0, 1.0, 20.0, 0.5};
  core::BoolSetting exhaust{"Simulate exhaust", true};
  core::BoolSetting notUsingItem{"Not using item", false};
  core::BoolSetting breakBlocks{"Break blocks", false};
  core::BoolSetting weaponOnly{"Weapon only", false};
  core::BoolSetting disableCreative{"Disable in creative", false};
  core::BoolSetting inventory{"Inventory", false};
  core::SliderSetting startDelay{"Start delay", 100.0, 0.0, 250.0, 10.0};

private:
  long long NextDelay();
  long long nextClickMs_ = 0;
  long long invNextMs_ = 0;
  bool holdingBreak_ = false;
};

}  // namespace modules
}  // namespace raven
