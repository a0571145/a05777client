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

// Port of keystrokesmod/module/impl/player/AutoTool.java (subset).
// While the left button mines a block, picks the hotbar slot with the best
// Item.getStrVsBlock strength and holds it, switching back afterwards.
// Spoof rendering, slot-scroll cancelling and block/held-item lists need
// mixin events or list settings and are omitted.
namespace raven {
namespace modules {

class AutoTool final : public core::Module {
 public:
  AutoTool();
  void OnUpdate() override;
  void OnDisable() override;

  core::BoolSetting requireMouse{"Require left mouse", true};
  core::SliderSetting activation{"Activation time", 0.0, 0.0, 1000.0, 25.0};
  core::SliderSetting hoverDelay{"Hover delay", 0.0, 0.0, 1000.0, 25.0};
  core::BoolSetting onlyCrouch{"Only while crouching", false};
  core::BoolSetting switchBack{"Switch back when done", true};

 private:
  static long long NowMs();
  void ResetSlot(JNIEnv* env, sdk::Mc::Frame& f);
  void ResetTimers() {
    leftStartMs_ = 0;
    hoverStartMs_ = 0;
  }

  int previous_ = -1;
  long long leftStartMs_ = 0;
  long long hoverStartMs_ = 0;
};

}  // namespace modules
}  // namespace raven
