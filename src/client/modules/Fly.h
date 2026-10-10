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

// Port of keystrokesmod/module/impl/movement/Fly.java (Vanilla/Fast/Fast 2).
// Walk mode needs the collision-event hook and is omitted (documented).
namespace raven {
namespace modules {

class Fly final : public core::Module {
public:
  Fly();
  void OnEnable() override;
  void OnUpdate() override;
  void OnDisable() override;

  core::ModeSetting mode{"Mode", {"Vanilla", "Fast", "Fast 2"}, 0};
  core::SliderSetting hSpeed{"Horizontal speed", 2.0, 1.0, 9.0, 0.1};
  core::SliderSetting vSpeed{"Vertical speed", 2.0, 1.0, 9.0, 0.1};

private:
  bool canFly_ = false;
};

}  // namespace modules
}  // namespace raven
