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

// Port of keystrokesmod/module/impl/combat/Reduce.java.
// Java runs on the attack hook; here landed hits are edge-detected on the
// crosshair target like KeepSprint. KeepSprint defers while Reduce is on
// ("Overrides KeepSprint").
namespace raven {
namespace modules {

class Reduce final : public core::Module {
public:
  Reduce();
  void OnUpdate() override;
  void OnDisable() override;

  core::SliderSetting reduction{"Attack reduction %", 60.0, 60.0, 100.0, 0.5};
  core::SliderSetting chance{"Chance", 100.0, 0.0, 100.0, 1.0};

private:
  void* lastTarget_ = nullptr;
  int lastHurt_ = -1;
  void ForgetTarget(void* envPtr);
};

}  // namespace modules
}  // namespace raven
