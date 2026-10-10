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

// Port of keystrokesmod/module/impl/player/NoFall.java.
// Spoof/NoGround have no packet hook here, so both are served by direct
// C03 sends (grounded / ungrounded) while falling -- same wire effect.
// Packet A/B mirror Java (timer + C03(true)). Adventure-mode and replay
// gates need WorldInfo/scoreboard and are omitted.
namespace raven {
namespace modules {

class NoFall final : public core::Module {
public:
  NoFall();
  void OnUpdate() override;
  void OnDisable() override;

  core::ModeSetting mode{"Mode", {"Spoof", "NoGround", "Packet A", "Packet B"}, 2};
  core::SliderSetting minFall{"Minimum fall distance", 3.0, 0.0, 10.0, 0.1};
  core::BoolSetting ignoreVoid{"Ignore void", true};

private:
  void SendGround(JNIEnv* env, bool onGround);
  void SetTimer(JNIEnv* env, float speed);

  double initialY_ = 0.0;
  double dynamic_ = 3.0;
  bool falling_ = false;
};

}  // namespace modules
}  // namespace raven
