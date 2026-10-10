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

// Port of keystrokesmod/module/impl/combat/ClickAssist.java.
// Java injects extra Robot clicks on chance; here Win32 mouse_event synthesizes
// the bonus press, with a short self-echo guard (ignNL/ignNR equivalent) and
// an above-5-CPS gate measured from real edges.
namespace raven {
namespace modules {

class ClickAssist final : public core::Module {
public:
  ClickAssist();
  void OnEnable() override;
  void OnDisable() override;
  void OnUpdate() override;

  core::BoolSetting disableCreative{"Disable in creative", true};
  core::BoolSetting leftClick{"Left click", true};
  core::SliderSetting chanceLeft{"Chance left", 80.0, 0.0, 100.0, 1.0};
  core::BoolSetting weaponOnly{"Weapon only", true};
  core::BoolSetting onlyTargeting{"Only while targeting", false};
  core::BoolSetting rightClick{"Right click", false};
  core::SliderSetting chanceRight{"Chance right", 80.0, 0.0, 100.0, 1.0};
  core::BoolSetting blocksOnly{"Blocks only", true};
  core::BoolSetting aboveCps{"Above 5 cps", false};

private:
  bool prevL_ = false, prevR_ = false;
  long long ignoreUntilMs_ = 0;
  long long edges_[32] = {};
  int edgePos_ = 0;
  void RecordEdge(long long now);
  bool CpsAbove5(long long now);
};

}  // namespace modules
}  // namespace raven
