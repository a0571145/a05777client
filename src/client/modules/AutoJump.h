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

// Port of keystrokesmod/module/impl/player/AutoJump.java (lite).
// Java simulates one player tick and forces jump input when the sim leaves
// the ground; here the 100Hz poll approximates it: grounded + moving +
// horizontally collided -> jump(). No settings in Java, none here.
namespace raven {
namespace modules {

class AutoJump final : public core::Module {
 public:
  AutoJump();
  void OnUpdate() override;
};

}  // namespace modules
}  // namespace raven
