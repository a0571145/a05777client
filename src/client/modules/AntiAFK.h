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
#include <random>
#include "core/Module.h"
#include "sdk/Mc.h"

// Port of keystrokesmod/module/impl/player/AntiAFK.java (subset).
// Movement modes + spin + jump + hotbar swap via JNI key/motion writes.
// Random clicks need the clickMouse accessor (mixin-only) and are omitted.
// Held/block lists need list settings (not in Setting.h) and are omitted.
namespace raven {
namespace modules {

class AntiAFK final : public core::Module {
 public:
  AntiAFK();
  void OnUpdate() override;
  void OnDisable() override;
  void OnEnable() override;

  core::ModeSetting afk{"AFK", {"None", "Wander", "Lateral shuffle", "Forward", "Backward"}, 0};
  core::BoolSetting jump{"Jump", false};
  core::BoolSetting jumpCollided{"Jump only when collided", false};
  core::BoolSetting swapItem{"Swap item", false};
  core::ModeSetting spin{"Spin", {"None", "Random", "Right", "Left"}, 0};
  core::BoolSetting randDelta{"Randomize delta", true};
  core::BoolSetting randPitch{"Randomize pitch", true};
  core::SliderSetting minDelay{"Minimum delay ticks", 10.0, 4.0, 160.0, 2.0};
  core::SliderSetting maxDelay{"Maximum delay ticks", 80.0, 4.0, 160.0, 2.0};

 private:
  void JavaTick(JNIEnv* env, sdk::Mc::Frame& f);
  void ReleaseKeys(JNIEnv* env, sdk::Mc::Frame& f);
  void SetMoveKey(JNIEnv* env, sdk::Mc::Frame& f, jfieldID keyField, bool pressed);
  int RollDelayTicks();
  double SpinDelta(bool right);
  double PitchDelta();

  int ticks_ = 0;
  bool random_ = false;
  bool droveKeys_ = false;
  int tickAcc_ = 0;  // 10ms OnUpdate calls -> one ~50ms Java tick
  std::mt19937 rng_{std::random_device{}()};
};

}  // namespace modules
}  // namespace raven
