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

#include "modules/WTap.h"
#include <chrono>
#include <jni.h>
#include <random>
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

namespace {
thread_local std::mt19937 t_rng(std::random_device{}());

long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
}  // namespace

WTap::WTap() : core::Module("WTap", core::Category::Combat, 0) {
  RegisterSlider(&chance);
  RegisterSlider(&delayBetween);
  RegisterSlider(&delayUntil);
  RegisterBool(&playersOnly);
}

void WTap::OnEnable() {
  pendingAtMs_ = 0;
  lastResetMs_ = 0;
  waitingRestart_ = false;
  wasSprinting_ = false;
  resetting_ = false;
}

void WTap::OnDisable() { OnEnable(); }

void WTap::ForgetTarget(void* envPtr) {
  if (lastTarget_) {
    static_cast<JNIEnv*>(envPtr)->DeleteWeakGlobalRef(
        static_cast<jweak>(lastTarget_));
    lastTarget_ = nullptr;
  }
  lastHurt_ = -1;
}

void WTap::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player) {
      pendingAtMs_ = 0;
      waitingRestart_ = false;
      wasSprinting_ = false;
      resetting_ = false;
      ForgetTarget(env);
      break;
    }
    long long now = NowMs();
    jboolean sprintingNow = env->CallBooleanMethod(f.player, id.eSprinting);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      break;
    }
    if (waitingRestart_ && sprintingNow && !wasSprinting_) {
      lastResetMs_ = now;
      waitingRestart_ = false;
      resetting_ = false;
    }
    if (pendingAtMs_ > 0 && now >= pendingAtMs_) {
      resetting_ = true;  // mirrors Java stopSprint (consumed by Sprint)
      pendingAtMs_ = 0;
      waitingRestart_ = true;
    }
    wasSprinting_ = sprintingNow;

    // Attack edge: crosshair target freshly hurt (replaces AttackEntityEvent).
    if (!f.mouseOver) {
      ForgetTarget(env);
      break;
    }
    jobject target = env->GetObjectField(f.mouseOver, id.mopEntity);
    if (!target || !env->IsInstanceOf(target, id.living)) {
      if (target) env->DeleteLocalRef(target);
      ForgetTarget(env);
      break;
    }
    bool valid = true;
    if (playersOnly.value && !env->IsInstanceOf(target, id.entityPlayer)) valid = false;
    float hp = env->CallFloatMethod(target, id.lGetHealth);
    if (env->ExceptionCheck() || hp <= 0.f) {
      env->ExceptionClear();
      valid = false;
    }
    jint hurt = env->GetIntField(target, id.lHurtTime);
    jint mx = env->GetIntField(target, id.lMaxHurt);
    bool same =
        lastTarget_ && env->IsSameObject(target, static_cast<jobject>(lastTarget_));
    bool freshHit = valid && hurt > 0 && hurt == mx && !(same && lastHurt_ == hurt);
    ForgetTarget(env);
    if (valid) {
      lastTarget_ = env->NewWeakGlobalRef(target);
      lastHurt_ = hurt;
    }
    env->DeleteLocalRef(target);
    if (!freshHit) break;
    if (!sprintingNow) break;  // Java gates on attacker sprinting
    if (chance.value == 0.0) break;
    if (pendingAtMs_ > 0) break;
    long long between = static_cast<long long>(delayBetween.value);
    if (lastResetMs_ > 0 && now - lastResetMs_ < between) break;
    if (chance.value < 100.0) {
      double roll = std::uniform_real_distribution<double>(0.0, 1.0)(t_rng);
      if (roll >= chance.value / 100.0) break;
    }
    pendingAtMs_ = now + static_cast<long long>(delayUntil.value);
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
