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

#include "modules/AntiAFK.h"
#include <algorithm>
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

AntiAFK::AntiAFK() : core::Module("AntiAFK", core::Category::Player, 0) {
  RegisterMode(&afk);
  RegisterBool(&jump);
  RegisterBool(&jumpCollided);
  RegisterBool(&swapItem);
  RegisterMode(&spin);
  RegisterBool(&randDelta);
  RegisterBool(&randPitch);
  RegisterSlider(&minDelay);
  RegisterSlider(&maxDelay);
}

void AntiAFK::OnEnable() {
  ticks_ = RollDelayTicks();
  random_ = (rng_() & 1) != 0;
  tickAcc_ = 0;
}

int AntiAFK::RollDelayTicks() {
  int lo = static_cast<int>(minDelay.value);
  int hi = static_cast<int>(maxDelay.value);
  if (hi < lo) std::swap(lo, hi);
  if (lo == hi) return lo;
  std::uniform_int_distribution<int> d(lo, hi);
  return d(rng_);
}

double AntiAFK::SpinDelta(bool right) {
  int n = right ? 1 : -1;
  if (!randDelta.value) return 3.0 * n;
  // Java: uniform 1.00..10.00, nudge off exact integers like Utils does.
  std::uniform_int_distribution<int> d(100, 1000);
  double v = d(rng_) / 100.0;
  if (v == static_cast<int>(v)) v += (rng_() % 10 + 1) / 10.0 * n;
  return v * n;
}

double AntiAFK::PitchDelta() {
  int n = (rng_() & 1) ? 1 : -1;
  if (!randDelta.value) return 2.0 * n;
  std::uniform_int_distribution<int> d(100, 500);
  double v = d(rng_) / 100.0;
  if (v == static_cast<int>(v)) v += (rng_() % 10 + 1) / 10.0 * n;
  return v * n;
}

void AntiAFK::SetMoveKey(JNIEnv* env, sdk::Mc::Frame& f, jfieldID keyField,
                         bool pressed) {
  if (!keyField) return;
  auto& id = sdk::Mc::Instance().Ids();
  jobject key = sdk::Mc::Obj(env, f.settings, keyField);
  if (!key) return;
  jint code = env->CallIntMethod(key, id.kGetCode);
  env->DeleteLocalRef(key);
  if (env->ExceptionCheck() || code == 0) {
    env->ExceptionClear();
    return;
  }
  env->CallStaticVoidMethod(id.keyBinding, id.kSetState, code,
                            pressed ? JNI_TRUE : JNI_FALSE);
  if (pressed) env->CallStaticVoidMethod(id.keyBinding, id.kOnTick, code);
  if (env->ExceptionCheck()) env->ExceptionClear();
}

void AntiAFK::ReleaseKeys(JNIEnv* env, sdk::Mc::Frame& f) {
  auto& id = sdk::Mc::Instance().Ids();
  SetMoveKey(env, f, id.sKeyForward, false);
  SetMoveKey(env, f, id.sKeyBack, false);
  SetMoveKey(env, f, id.sKeyLeft, false);
  SetMoveKey(env, f, id.sKeyRight, false);
}

void AntiAFK::JavaTick(JNIEnv* env, sdk::Mc::Frame& f) {
  auto& id = sdk::Mc::Instance().Ids();
  if (!f.player || !f.settings) return;
  // Java acts with no GUI open (chat allowed); we require no screen.
  if (f.screen) return;

  // Never fight physical keys when both drivers are off; release once after
  // driving so a mode switch back to None does not stick keys down.
  if (afk.index == 0) {
    if (droveKeys_) {
      ReleaseKeys(env, f);
      droveKeys_ = false;
    }
  } else {
    droveKeys_ = true;
  }

  bool r = (rng_() & 1) != 0;
  switch (afk.index) {
    case 1:  // Wander
      if (random_) {
        SetMoveKey(env, f, id.sKeyForward, r);
        SetMoveKey(env, f, id.sKeyRight, r);
      } else {
        SetMoveKey(env, f, id.sKeyBack, r);
        SetMoveKey(env, f, id.sKeyLeft, r);
      }
      break;
    case 2:  // Lateral shuffle
      SetMoveKey(env, f, id.sKeyRight, random_);
      SetMoveKey(env, f, id.sKeyLeft, !random_);
      break;
    case 3:  // Forward
      SetMoveKey(env, f, id.sKeyForward, true);
      break;
    case 4:  // Backward
      SetMoveKey(env, f, id.sKeyBack, true);
      break;
    default:
      break;
  }

  switch (spin.index) {
    case 1: {  // Random
      float yaw = sdk::Mc::Fl(env, f.player, id.eYaw);
      sdk::Mc::SetF(env, f.player, id.eYaw,
                    static_cast<float>(yaw + SpinDelta(random_)));
      break;
    }
    case 2: {  // Right
      float yaw = sdk::Mc::Fl(env, f.player, id.eYaw);
      sdk::Mc::SetF(env, f.player, id.eYaw,
                    static_cast<float>(yaw + SpinDelta(true)));
      break;
    }
    case 3: {  // Left
      float yaw = sdk::Mc::Fl(env, f.player, id.eYaw);
      sdk::Mc::SetF(env, f.player, id.eYaw,
                    static_cast<float>(yaw + SpinDelta(false)));
      break;
    }
    default:
      break;
  }
  if (spin.index != 0 && randPitch.value) {
    float pitch = sdk::Mc::Fl(env, f.player, id.ePitch);
    float np = static_cast<float>(pitch + PitchDelta());
    np = std::clamp(np, -90.f, 90.f);
    sdk::Mc::SetF(env, f.player, id.ePitch, np);
  }

  if (jump.value && id.jumpM) {
    bool onGround = sdk::Mc::Bo(env, f.player, id.eOnGround);
    bool collided = sdk::Mc::Bo(env, f.player, id.collidedH);
    if (onGround && (!jumpCollided.value || collided)) {
      env->CallVoidMethod(f.player, id.jumpM);
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
  }

  if (--ticks_ <= 0) {
    if (swapItem.value && id.pInventory && id.invCurrent) {
      jobject inv = sdk::Mc::Obj(env, f.player, id.pInventory);
      if (inv) {
        std::uniform_int_distribution<int> d(0, 8);
        sdk::Mc::SetI(env, inv, id.invCurrent, d(rng_));
        env->DeleteLocalRef(inv);
      }
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
    ticks_ = RollDelayTicks();
    random_ = !random_;
  }
  if (env->ExceptionCheck()) env->ExceptionClear();
}

void AntiAFK::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  // ~50ms cadence like the Java PreUpdate tick.
  if (++tickAcc_ < 5) return;
  tickAcc_ = 0;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  JavaTick(env, f);
  mc.ReleaseFrame(f);
}

void AntiAFK::OnDisable() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  ReleaseKeys(env, f);
  droveKeys_ = false;
  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
