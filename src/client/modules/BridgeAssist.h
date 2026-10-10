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

// Port of keystrokesmod/module/impl/player/BridgeAssist.java (subset).
// Edge-lock sneak while placing: physical SHIFT opens the door, the module
// assists only while the place key (RMB) is held. Off the place key the
// module goes hands-off without touching the sneak key, so native SHIFT
// just crouches. The edge distance is the foot overhang (0 = safely inside,
// larger = hanging off); sneak presses past Edge offset. Differences from
// Java (all documented): the distance is read from the live bounding box
// instead of a simulated tick; manual state is the physical Win32 SHIFT key
// (the in-game pressed flag echoes our own presses and self-locks); Pre
// place aim needs rotation events and is omitted; C08 place tracking is
// omitted (placing = physical RMB); chat-GUI nuance is omitted (any open
// screen pauses).
namespace raven {
namespace modules {

class BridgeAssist final : public core::Module {
 public:
  BridgeAssist();
  void OnUpdate() override;
  void OnDisable() override;
  void OnEnable() override;

  core::SliderSetting edgeOffset{"Edge offset", 0.0, 0.0, 0.3, 0.01};
  core::SliderSetting unsneakDelay{"Unsneak delay", 50.0, 50.0, 300.0, 5.0};
  core::SliderSetting sneakOnJump{"Sneak on jump", 0.0, 0.0, 500.0, 5.0};
  core::BoolSetting edgeLock{"Edge lock", true};
  core::BoolSetting holdingBlocks{"Holding blocks", false};
  core::BoolSetting lookingDown{"Looking down", false};
  core::BoolSetting notMovingForward{"Not moving forward", false};

 private:
  static long long NowMs();
  void SetSneak(JNIEnv* env, sdk::Mc::Frame& f, bool pressed);
  static bool PhysicalShift();
  void Press(JNIEnv* env, sdk::Mc::Frame& f);
  void Release(JNIEnv* env, sdk::Mc::Frame& f);
  // NaN when no ground under the feet slab.
  double EdgeOffset(JNIEnv* env, sdk::Mc::Frame& f);

  bool sneaking_ = false;
  long long unsneakAtMs_ = -1;  // delayed release deadline, -1 = none
  long long jumpHoldUntilMs_ = 0;
  int tickAcc_ = 0;  // 10ms OnUpdate calls -> one ~50ms Java tick
};

}  // namespace modules
}  // namespace raven
