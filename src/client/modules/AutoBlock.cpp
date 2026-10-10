/*
 * Copyright (C) 2026 a05777
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "modules/AutoBlock.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <random>
#include <string>

#include "core/Client.h"
#include "modules/CombatUtil.h"
#include "modules/KillAura.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

namespace {

thread_local std::mt19937 t_rng(std::random_device{}());

bool SetKeyState(JNIEnv* env, const sdk::McIds& id, jint code, bool down) {
  if (!env || !id.keyBinding || !id.kSetState || code == 0) return false;
  env->CallStaticVoidMethod(id.keyBinding, id.kSetState, code,
                            down ? JNI_TRUE : JNI_FALSE);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return false;
  }
  return true;
}

void SendUseRelease(JNIEnv* env, sdk::Mc::Frame& frame,
                    const sdk::McIds& id) {
  if (!env || !frame.player || !id.spSendQueue || !id.c07 || !id.c07Ctor ||
      !id.releaseAction || !id.originPos || !id.downFace || !id.nhSend) {
    return;
  }

  jobject queue = env->GetObjectField(frame.player, id.spSendQueue);
  if (!queue || env->ExceptionCheck()) {
    env->ExceptionClear();
    if (queue) env->DeleteLocalRef(queue);
    return;
  }

  jobject packet = env->NewObject(id.c07, id.c07Ctor, id.releaseAction,
                                  id.originPos, id.downFace);
  if (packet && !env->ExceptionCheck()) {
    env->CallVoidMethod(queue, id.nhSend, packet);
  }
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (packet) env->DeleteLocalRef(packet);
  env->DeleteLocalRef(queue);
}

bool HoldingSword(JNIEnv* env, jobject player) {
  if (!env || !player) return false;
  auto& id = sdk::Mc::Instance().Ids();
  if (!id.lGetHeld || !id.stGetItem || !id.sword) return false;

  jobject held = env->CallObjectMethod(player, id.lGetHeld);
  if (!held || env->ExceptionCheck()) {
    env->ExceptionClear();
    if (held) env->DeleteLocalRef(held);
    return false;
  }
  jobject item = env->CallObjectMethod(held, id.stGetItem);
  bool sword = item && !env->ExceptionCheck() && env->IsInstanceOf(item, id.sword);
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (item) env->DeleteLocalRef(item);
  env->DeleteLocalRef(held);
  return sword;
}

double EyeToEntityBoxDistanceSq(JNIEnv* env, jobject player, jobject target,
                                const sdk::McIds& id) {
  if (!env || !player || !target || !id.ePosX || !id.ePosY || !id.ePosZ) {
    return std::numeric_limits<double>::max();
  }

  constexpr double kEyeHeight = 1.62;
  const double ex = sdk::Mc::D(env, player, id.ePosX);
  const double ey = sdk::Mc::D(env, player, id.ePosY) + kEyeHeight;
  const double ez = sdk::Mc::D(env, player, id.ePosZ);
  const double tx = sdk::Mc::D(env, target, id.ePosX);
  const double ty = sdk::Mc::D(env, target, id.ePosY);
  const double tz = sdk::Mc::D(env, target, id.ePosZ);
  const double halfWidth = id.eWidth
                               ? std::max(0.01, static_cast<double>(
                                                    sdk::Mc::Fl(env, target,
                                                                id.eWidth)) *
                                                    0.5)
                               : 0.3;
  const double height = id.eHeight
                            ? std::max(0.01, static_cast<double>(
                                                 sdk::Mc::Fl(env, target,
                                                             id.eHeight)))
                            : 1.8;
  const double cx = std::clamp(ex, tx - halfWidth, tx + halfWidth);
  const double cy = std::clamp(ey, ty, ty + height);
  const double cz = std::clamp(ez, tz - halfWidth, tz + halfWidth);
  const double dx = ex - cx, dy = ey - cy, dz = ez - cz;
  return dx * dx + dy * dy + dz * dz;
}

}  // namespace

AutoBlock::AutoBlock() : core::Module("Auto Block", core::Category::Combat, 0) {
  RegisterSlider(&range);
  RegisterSlider(&maxHurtTimeMs);
  RegisterSlider(&maxHoldMs);
  RegisterSlider(&lagChance);
  RegisterSlider(&lagMaxDuration);
  RegisterBool(&preventDelayAttacks);
  RegisterBool(&blockAgainImmediately);
  RegisterBool(&forceBlockAnimation);
  RegisterBool(&requireLmb);
  RegisterBool(&requireRmb);
  RegisterBool(&onlyWhenDamaged);
  RegisterBool(&ignoreTeammates);
}

long long AutoBlock::NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

long long AutoBlock::MsToTickDuration(double ms) {
  if (ms <= 0.0) return 0;
  return static_cast<long long>(std::ceil(ms / 50.0)) * 50;
}

void AutoBlock::OnEnable() {
  state_.store(State::Idle, std::memory_order_release);
  ownsUseKey_ = false;
  manualBlock_ = false;
  prevLmbDown_ = false;
  pendingBlockAfterAttack_ = false;
  attackInterruptedLag_ = false;
  useKeyCode_ = 0;
  blockStartedMs_ = 0;
  stateUntilMs_ = 0;
  suppressBlockUntilMs_ = 0;
  lastSelfHurtTime_ = 0;
}

void AutoBlock::OnDisable() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  sdk::Mc::Frame frame;
  if (env) {
    sdk::Mc& mc = sdk::Mc::Instance();
    if (mc.Ensure(env) && mc.BeginFrame(env, frame)) {
      ResetState(env, frame);
      mc.ReleaseFrame(frame);
      return;
    }
    ResetState(env, frame);
  } else {
    state_.store(State::Idle, std::memory_order_release);
    ownsUseKey_ = false;
    manualBlock_ = false;
    pendingBlockAfterAttack_ = false;
    attackInterruptedLag_ = false;
    useKeyCode_ = 0;
    blockStartedMs_ = 0;
    stateUntilMs_ = 0;
    suppressBlockUntilMs_ = 0;
    lastSelfHurtTime_ = 0;
  }
}

void AutoBlock::StartBlocking(JNIEnv* env, sdk::Mc::Frame& frame,
                              long long now) {
  if (state_.load(std::memory_order_relaxed) == State::Blocking) return;

  auto& id = sdk::Mc::Instance().Ids();
  if (!env || !frame.useKey || !id.keyBinding || !id.kGetCode ||
      !id.kSetState || !id.kOnTick) {
    return;
  }

  const jint code = env->CallIntMethod(frame.useKey, id.kGetCode);
  if (env->ExceptionCheck() || code == 0) {
    env->ExceptionClear();
    return;
  }
  if (!SetKeyState(env, id, code, true)) return;

  if (id.kPressed) env->SetBooleanField(frame.useKey, id.kPressed, JNI_TRUE);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    SetKeyState(env, id, code, false);
    return;
  }
  env->CallStaticVoidMethod(id.keyBinding, id.kOnTick, code);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    SetKeyState(env, id, code, false);
    if (id.kPressed) env->SetBooleanField(frame.useKey, id.kPressed, JNI_FALSE);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return;
  }

  ownsUseKey_ = true;
  useKeyCode_ = code;
  blockStartedMs_ = now;
  stateUntilMs_ = 0;
  pendingBlockAfterAttack_ = false;
  state_.store(State::Blocking, std::memory_order_release);
}

void AutoBlock::StopBlocking(JNIEnv* env, sdk::Mc::Frame& frame,
                             bool sendReleasePacket) {
  auto& id = sdk::Mc::Instance().Ids();
  const bool wasBlocking =
      state_.load(std::memory_order_relaxed) == State::Blocking;

  if (sendReleasePacket && wasBlocking && ownsUseKey_) {
    SendUseRelease(env, frame, id);
    if (env && frame.player && id.pClearItemInUse) {
      env->CallVoidMethod(frame.player, id.pClearItemInUse);
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
  }
  if (env && ownsUseKey_ && useKeyCode_ != 0) {
    SetKeyState(env, id, useKeyCode_, false);
    if (frame.useKey && id.kPressed) {
      env->SetBooleanField(frame.useKey, id.kPressed, JNI_FALSE);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  ownsUseKey_ = false;
  useKeyCode_ = 0;
  blockStartedMs_ = 0;
  if (state_.load(std::memory_order_relaxed) != State::LagPause) {
    stateUntilMs_ = 0;
  }
  state_.store(State::Idle, std::memory_order_release);
}

void AutoBlock::ResetState(JNIEnv* env, sdk::Mc::Frame& frame) {
  ReleaseLag();
  StopBlocking(env, frame, true);
  manualBlock_ = false;
  prevLmbDown_ = false;
  pendingBlockAfterAttack_ = false;
  attackInterruptedLag_ = false;
  suppressBlockUntilMs_ = 0;
  lastSelfHurtTime_ = 0;
}

void AutoBlock::PrepareAttack(JNIEnv* env, sdk::Mc::Frame& frame) {
  if (!IsEnabled() || !env || !HoldingSword(env, frame.player)) return;

  const State oldState = state_.load(std::memory_order_relaxed);
  attackInterruptedLag_ =
      oldState == State::LagPause && preventDelayAttacks.value;
  const bool releaseServerUse = oldState == State::Blocking ||
                                combat::RmbDown();
  auto& mc = sdk::Mc::Instance();
  auto& id = mc.Ids();
  if (releaseServerUse) {
    mc.EnsureExtra(env);
    SendUseRelease(env, frame, id);
    if (frame.player && id.pClearItemInUse) {
      env->CallVoidMethod(frame.player, id.pClearItemInUse);
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
  }

  if (ownsUseKey_ && useKeyCode_ != 0) {
    SetKeyState(env, id, useKeyCode_, false);
    if (frame.useKey && id.kPressed) {
      env->SetBooleanField(frame.useKey, id.kPressed, JNI_FALSE);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  ownsUseKey_ = false;
  useKeyCode_ = 0;
  blockStartedMs_ = 0;
  pendingBlockAfterAttack_ = false;
  manualBlock_ = false;
  if (oldState == State::LagPause && !preventDelayAttacks.value) return;
  if (oldState == State::LagPause) ReleaseLag();
  const long long now = NowMs();
  stateUntilMs_ = now + (preventDelayAttacks.value ? 50 : 0);
  suppressBlockUntilMs_ = std::max(suppressBlockUntilMs_, stateUntilMs_);
  state_.store(State::AttackPause, std::memory_order_release);
}

void AutoBlock::NotifyAttack(bool lmbIntent) {
  if (!IsEnabled() || !attackInterruptedLag_) return;
  attackInterruptedLag_ = false;
  if (!lmbIntent || !blockAgainImmediately.value || !combat::RmbDown()) return;
  pendingBlockAfterAttack_ = true;
  stateUntilMs_ = NowMs();
  state_.store(State::AttackPause, std::memory_order_release);
}

void AutoBlock::HandleManualAttackInput() {
  if (!IsEnabled() || !combat::ForegroundIsGame()) return;
  if (!combat::RmbDown() &&
      state_.load(std::memory_order_relaxed) != State::Blocking) {
    return;
  }

  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) return;
  sdk::Mc::Frame frame;
  if (!mc.BeginFrame(env, frame)) return;
  const bool validContext = frame.player && frame.world && frame.hasFocus &&
                            !frame.screen && HoldingSword(env, frame.player);
  if (validContext) {
    PrepareAttack(env, frame);
    manualBlock_ = false;
    attackInterruptedLag_ = false;
    suppressBlockUntilMs_ = std::max(suppressBlockUntilMs_, NowMs() + 50);
  }
  mc.ReleaseFrame(frame);
}

bool AutoBlock::HandleRightMouseButton(bool down) {
  if (!down) {
    const bool wasCaptured = capturedRmb_;
    capturedRmb_ = false;
    return wasCaptured;
  }
  if (capturedRmb_) return true;
  if (!IsEnabled() || !combat::ForegroundIsGame()) return false;

  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return false;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) return false;
  sdk::Mc::Frame frame;
  if (!mc.BeginFrame(env, frame)) return false;
  const bool shouldCapture = frame.player && frame.world && frame.hasFocus &&
                             !frame.screen && HoldingSword(env, frame.player);
  mc.ReleaseFrame(frame);
  capturedRmb_ = shouldCapture;
  return capturedRmb_;
}

bool AutoBlock::CheckConditions(bool lmbDown, bool rmbDown) const {
  if (requireLmb.value && !lmbDown) return false;
  if (requireRmb.value && !rmbDown) return false;
  return true;
}

bool AutoBlock::ShouldPredictiveBlock(int hurtTicks) const {
  int triggerTick = static_cast<int>(std::lround(maxHurtTimeMs.value / 50.0));
  triggerTick = std::clamp(triggerTick, 1, 10);
  return hurtTicks == triggerTick;
}

bool AutoBlock::ShouldStartLag() {
  const double chance = std::clamp(lagChance.value, 0.0, 100.0);
  if (chance <= 0.0) return false;
  if (chance >= 100.0) return true;
  std::uniform_real_distribution<double> pick(0.0, 100.0);
  return pick(t_rng) < chance;
}

void AutoBlock::StartLag(long long now, long long blockStartedAt) {
  const long long maxLagMs = MsToTickDuration(lagMaxDuration.value);
  if (maxLagMs <= 0 || now - blockStartedAt >= maxLagMs) return;
  stateUntilMs_ = blockStartedAt + maxLagMs;
  state_.store(State::LagPause, std::memory_order_release);
}

void AutoBlock::ReleaseLag() {
  if (state_.load(std::memory_order_relaxed) != State::LagPause) return;
  stateUntilMs_ = 0;
  state_.store(State::Idle, std::memory_order_release);
}

bool AutoBlock::KillAuraCountsAsLmb(bool hasTarget) const {
  if (!hasTarget) return false;
  core::Module* module = core::Client::Instance().Modules().Find("Kill Aura");
  auto* aura = dynamic_cast<KillAura*>(module);
  return aura && aura->IsEnabled() && !aura->requireMouseDown.value;
}

jobject AutoBlock::FindTarget(JNIEnv* env, sdk::Mc::Frame& frame) {
  auto& id = sdk::Mc::Instance().Ids();
  if (!env || !frame.player || !id.entityPlayer || !id.ePosX || !id.ePosY ||
      !id.ePosZ) {
    return nullptr;
  }

  const double rangeSq = range.value * range.value;
  std::string selfText;
  if (ignoreTeammates.value && id.eGetName && id.ccText) {
    selfText = combat::DisplayText(env, frame.player);
  }

  auto validTarget = [&](jobject entity) {
    if (!entity || !env->IsInstanceOf(entity, id.entityPlayer) ||
        sdk::Mc::Same(env, entity, frame.player)) {
      return false;
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      return false;
    }
    if (id.eDead && sdk::Mc::Bo(env, entity, id.eDead)) return false;

    if (id.pIsSpectator) {
      const jboolean spectator = env->CallBooleanMethod(entity, id.pIsSpectator);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
      } else if (spectator) {
        return false;
      }
    }
    if (id.lGetHealth) {
      const jfloat health = env->CallFloatMethod(entity, id.lGetHealth);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
      } else if (health <= 0.0f) {
        return false;
      }
    }
    if (ignoreTeammates.value && !selfText.empty() && id.eGetName && id.ccText) {
      const std::string otherText = combat::DisplayText(env, entity);
      if (combat::IsTeammateText(selfText, otherText)) return false;
    }
    return EyeToEntityBoxDistanceSq(env, frame.player, entity, id) <= rangeSq;
  };

  if (frame.mouseOver && id.mopEntity) {
    jobject pointed = env->GetObjectField(frame.mouseOver, id.mopEntity);
    if (pointed && !env->ExceptionCheck()) {
      if (validTarget(pointed)) return pointed;
      env->DeleteLocalRef(pointed);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  jobject best = nullptr;
  double bestDistanceSq = rangeSq;
  sdk::Mc::Instance().EachPlayer(
      frame,
      [&](jobject entity) {
        if (!validTarget(entity)) return;
        const double distanceSq = EyeToEntityBoxDistanceSq(env, frame.player,
                                                           entity, id);
        if (distanceSq >= bestDistanceSq) return;
        jobject next = env->NewLocalRef(entity);
        if (!next || env->ExceptionCheck()) {
          env->ExceptionClear();
          if (next) env->DeleteLocalRef(next);
          return;
        }
        if (best) env->DeleteLocalRef(best);
        best = next;
        bestDistanceSq = distanceSq;
      },
      true);
  if (env->ExceptionCheck()) env->ExceptionClear();
  return best;
}

void AutoBlock::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;

  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) {
    sdk::Mc::Frame empty;
    ResetState(env, empty);
    return;
  }

  sdk::Mc::Frame frame;
  if (!mc.BeginFrame(env, frame)) {
    ResetState(env, frame);
    return;
  }
  auto& id = mc.Ids();

  do {
    const bool dead = frame.player && id.eDead &&
                      sdk::Mc::Bo(env, frame.player, id.eDead);
    if (!combat::ForegroundIsGame() || !frame.hasFocus || !frame.player ||
        !frame.world || dead || frame.screen) {
      ResetState(env, frame);
      break;
    }
    if (!HoldingSword(env, frame.player)) {
      ResetState(env, frame);
      break;
    }

    const long long now = NowMs();
    const bool physicalLmbDown = combat::LmbDown();
    const bool lmbPressed = physicalLmbDown && !prevLmbDown_;
    prevLmbDown_ = physicalLmbDown;
    const bool rmbDown = combat::RmbDown();

    State state = state_.load(std::memory_order_relaxed);
    if (lmbPressed && rmbDown &&
        (state == State::Blocking || state == State::LagPause)) {
      PrepareAttack(env, frame);
      attackInterruptedLag_ = false;
      manualBlock_ = false;
      suppressBlockUntilMs_ = std::max(suppressBlockUntilMs_, now + 50);
      state = state_.load(std::memory_order_relaxed);
    }

    if (!rmbDown) {
      ResetState(env, frame);
      break;
    }

    mc.EnsureExtra(env);
    jobject target = FindTarget(env, frame);
    const bool hasTarget = target != nullptr;
    if (target) env->DeleteLocalRef(target);

    const int selfHurtTime = id.lMaxHurt
                                 ? sdk::Mc::In(env, frame.player, id.lMaxHurt)
                                 : 0;
    const bool hurtAgain = selfHurtTime > lastSelfHurtTime_;
    lastSelfHurtTime_ = selfHurtTime;

    const bool auraAttackIntent = KillAuraCountsAsLmb(hasTarget);
    const bool lmbDown = physicalLmbDown || auraAttackIntent;
    const bool conditions =
        hasTarget && CheckConditions(auraAttackIntent, rmbDown);

    state = state_.load(std::memory_order_relaxed);
    if (state == State::AttackPause) {
      if (now < stateUntilMs_) break;
      stateUntilMs_ = 0;
      state_.store(State::Idle, std::memory_order_release);
      state = State::Idle;
    }

    // Physical attacks always get an open use-item window. A native WndProc
    // callback performs the first release before Minecraft consumes the click;
    // this polling fallback also catches injected or unusually short clicks.
    if (physicalLmbDown) {
      if (state == State::Blocking || state == State::LagPause) {
        PrepareAttack(env, frame);
        attackInterruptedLag_ = false;
      }
      manualBlock_ = false;
      suppressBlockUntilMs_ = std::max(suppressBlockUntilMs_, now + 50);
      break;
    }

    // Match the reference client: a held right button without an attack intent
    // is manual blocking and does not require a nearby target or LMB setting.
    if (!lmbDown) {
      if (state == State::LagPause) ReleaseLag();
      state = state_.load(std::memory_order_relaxed);
      if (state == State::AttackPause && now < stateUntilMs_) break;
      if (now < suppressBlockUntilMs_) break;
      if (state != State::Blocking) {
        StartBlocking(env, frame, now);
        manualBlock_ = IsBlocking();
      }
      break;
    }

    if (manualBlock_) {
      StopBlocking(env, frame, true);
      manualBlock_ = false;
      state = State::Idle;
    }

    if (state == State::LagPause) {
      const bool lagExpired = now >= stateUntilMs_;
      if (lagExpired || !conditions) {
        ReleaseLag();
        state = State::Idle;
        if (lagExpired && blockAgainImmediately.value && conditions &&
            now >= suppressBlockUntilMs_) {
          StartBlocking(env, frame, now);
          state = state_.load(std::memory_order_relaxed);
        }
      } else {
        break;
      }
    }

    if (!conditions) {
      if (IsBlocking()) StopBlocking(env, frame, true);
      pendingBlockAfterAttack_ = false;
      break;
    }

    if (state != State::Blocking && now >= suppressBlockUntilMs_) {
      const bool shouldStart =
          pendingBlockAfterAttack_ || !onlyWhenDamaged.value ||
          ShouldPredictiveBlock(selfHurtTime);
      if (shouldStart) {
        StartBlocking(env, frame, now);
        pendingBlockAfterAttack_ = false;
      }
    }

    if (IsBlocking()) {
      const long long maxHold = MsToTickDuration(maxHoldMs.value);
      const bool timeExpired =
          maxHold > 0 && now - blockStartedMs_ >= maxHold;
      const bool shouldStop = timeExpired || (onlyWhenDamaged.value && hurtAgain);
      if (shouldStop) {
        const long long startedAt = blockStartedMs_;
        const bool startLag = ShouldStartLag();
        StopBlocking(env, frame, true);
        if (startLag) StartLag(now, startedAt);
      }
    }
  } while (false);

  mc.ReleaseFrame(frame);
}

}  // namespace modules
}  // namespace raven
