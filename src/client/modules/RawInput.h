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

// Port of keystrokesmod/module/impl/combat/RawInput.java (native equivalent).
// Java swaps the MouseHelper for JInput raw polling; without bytecode hooks
// we achieve the same user-visible effect by disabling Windows "Enhance
// pointer precision" (OS mouse acceleration) while enabled.
namespace raven {
namespace modules {

class RawInput final : public core::Module {
public:
  RawInput();
  void OnEnable() override;
  void OnDisable() override;

private:
  int mouseParams_[3] = {0, 0, 0};
  bool saved_ = false;
};

}  // namespace modules
}  // namespace raven
