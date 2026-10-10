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

// Port of keystrokesmod/module/impl/movement/Sprint.java (key-force part).
// The Java omnidirectional allowances (backwards/sideways/using-item) are
// enforced by MixinEntityPlayerSP; without that hook this port forces the
// sprint key while the rules above hold. Backwards/sideways sprint needs the
// movement hook (Batch 2).
namespace raven {
namespace modules {

class Sprint final : public core::Module {
public:
  Sprint();
  void OnUpdate() override;
  void OnDisable() override;

  core::BoolSetting allowInInventory{"In inventory", false};
};

}  // namespace modules
}  // namespace raven
