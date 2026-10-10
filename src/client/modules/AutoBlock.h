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

#include <atomic>
#include <jni.h>

#include "core/Module.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

class AutoBlock final : public core::Module {
 private:
  enum class State : unsigned char { Idle, Blocking, AttackPause, LagPause };

 public:
  AutoBlock();
  void OnEnable() override;
  void OnUpdate() override;
  void OnDisable() override;

  bool IsActive() const {
    const State state = state_.load(std::memory_order_relaxed);
    return state == State::Blocking || state == State::LagPause;
  }
  bool IsBlocking() const {
    return state_.load(std::memory_order_relaxed) == State::Blocking;
  }
  bool CanAttackWhileUsingItem() const {
    const State state = state_.load(std::memory_order_relaxed);
    return state == State::Blocking || state == State::AttackPause;
  }

  // Called by attack modules immediately before sending an attack. It releases
  // AutoBlock's use state and queues RELEASE_USE_ITEM ahead of the attack.
  void PrepareAttack(JNIEnv* env, sdk::Mc::Frame& frame);
  // Native window-input bridge: runs before Minecraft handles a physical
  // click, so an active block can be released before the attack is processed.
  void HandleManualAttackInput();
  bool HandleRightMouseButton(bool down);
  // Releases a lag pause early when KillAura sends an attack packet.
  void NotifyAttack(bool lmbIntent = true);

  core::SliderSetting range{"Range", 4.0, 2.0, 6.0, 0.1};
  core::SliderSetting maxHurtTimeMs{"Maximum Hurt Time", 200.0, 50.0, 500.0, 50.0};
  core::SliderSetting maxHoldMs{"Maximum Hold Time", 150.0, 50.0, 500.0, 50.0};
  core::SliderSetting lagChance{"Lag Chance", 100.0, 0.0, 100.0, 5.0};
  core::SliderSetting lagMaxDuration{"Lag Max Duration", 200.0, 50.0, 500.0, 50.0};
  core::BoolSetting preventDelayAttacks{"Prevent delaying attacks", true};
  core::BoolSetting blockAgainImmediately{"Block again immediately", true};
  // Retained for existing configs; vanilla's use-key path controls animation.
  core::BoolSetting forceBlockAnimation{"Force block animation", true};
  core::BoolSetting requireLmb{"Require Left mouse", true};
  core::BoolSetting requireRmb{"Require right mouse", false};
  core::BoolSetting onlyWhenDamaged{"Damaged", false};
  core::BoolSetting ignoreTeammates{"Ignore teammates", true};

 private:
  static long long NowMs();
  static long long MsToTickDuration(double ms);
  void StartBlocking(JNIEnv* env, sdk::Mc::Frame& frame, long long now);
  void StopBlocking(JNIEnv* env, sdk::Mc::Frame& frame, bool sendReleasePacket);
  void ResetState(JNIEnv* env, sdk::Mc::Frame& frame);
  void StartLag(long long now, long long blockStartedAt);
  void ReleaseLag();
  bool CheckConditions(bool lmbDown, bool rmbDown) const;
  bool ShouldPredictiveBlock(int hurtTicks) const;
  bool ShouldStartLag();
  bool KillAuraCountsAsLmb(bool hasTarget) const;
  jobject FindTarget(JNIEnv* env, sdk::Mc::Frame& frame);

  std::atomic<State> state_{State::Idle};
  bool ownsUseKey_ = false;
  bool manualBlock_ = false;
  bool capturedRmb_ = false;
  bool prevLmbDown_ = false;
  bool pendingBlockAfterAttack_ = false;
  bool attackInterruptedLag_ = false;
  jint useKeyCode_ = 0;
  long long blockStartedMs_ = 0;
  long long stateUntilMs_ = 0;
  long long suppressBlockUntilMs_ = 0;
  int lastSelfHurtTime_ = 0;
};

}  // namespace modules
}  // namespace raven
