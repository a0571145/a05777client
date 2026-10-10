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

// Port of a Fullbright (Raven has no gamma module; behavior: pin gammaSetting
// while enabled, restore the previous value on disable).
namespace raven {
namespace modules {

class Fullbright final : public core::Module {
public:
  Fullbright();
  void OnEnable() override;
  void OnUpdate() override;
  void OnDisable() override;

  core::SliderSetting brightness{"Brightness", 10.0, 1.0, 100.0, 1.0};

private:
  float saved_ = -1.f;
};

}  // namespace modules
}  // namespace raven
