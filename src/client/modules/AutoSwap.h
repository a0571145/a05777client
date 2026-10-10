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
#include <jni.h>
#include "core/Module.h"
#include "sdk/Mc.h"

// Port of keystrokesmod/module/impl/player/AutoSwap.java (subset).
// While placing blocks, the held stack runs out: swap to another hotbar
// slot holding the same block type. Differences from Java: placements are
// tracked by polling (held ItemBlock with size>0) instead of C08 sniffing
// (packet hook), and there is no block whitelist (needs a list setting).
namespace raven {
namespace modules {

class AutoSwap final : public core::Module {
 public:
  AutoSwap();
  void OnUpdate() override;
  void OnDisable() override;

 private:
  static long long NowMs();
  void ResetTrack();

  jclass trackedCls_ = nullptr;  // global ref to the placed block's Item class
  int trackedMeta_ = 0;
  bool haveTrack_ = false;
  int lastSwapSlot_ = -1;
  long long lastSwapMs_ = 0;
};

}  // namespace modules
}  // namespace raven
