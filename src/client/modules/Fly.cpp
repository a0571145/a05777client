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

#include "modules/Fly.h"
#include <random>
#include "modules/MoveUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

namespace {
thread_local std::mt19937 t_rng(std::random_device{}());
}

Fly::Fly() : core::Module("Fly", core::Category::Movement, 0) {
  RegisterMode(&mode);
  RegisterSlider(&hSpeed);
  RegisterSlider(&vSpeed);
}

void Fly::OnEnable() {
  canFly_ = false;
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  if (f.player) {
    jobject caps = sdk::Mc::Obj(env, f.player, mc.Ids().pCaps);
    canFly_ = caps && sdk::Mc::Bo(env, caps, mc.Ids().cAllowFly);
    if (caps) env->DeleteLocalRef(caps);
  }
  mc.ReleaseFrame(f);
}

void Fly::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player) break;
    jobject caps = sdk::Mc::Obj(env, f.player, id.pCaps);
    if (!caps) break;
    jobject mi = sdk::Mc::Obj(env, f.player, id.spMoveInput);
    bool jump = sdk::Mc::Bo(env, mi, id.inJump);
    bool sneak = sdk::Mc::Bo(env, mi, id.inSneak);
    if (mi) env->DeleteLocalRef(mi);

    switch (mode.index) {
      case 0: {  // Vanilla
        sdk::Mc::SetD(env, f.player, id.eMotionY, 0.0);
        env->CallVoidMethod(caps, id.capsFlySpeed, 0.05f * static_cast<float>(hSpeed.value));
        sdk::Mc::SetB(env, caps, id.cFlying, true);
        break;
      }
      case 1: {  // Fast
        sdk::Mc::SetB(env, f.player, id.eOnGround, true);
        if (!f.screen) {
          if (jump) {
            sdk::Mc::SetD(env, f.player, id.eMotionY, 0.3 * vSpeed.value);
          } else if (sneak) {
            // Java repeats jumpDown() here (apparent bug); intent is sneak-down.
            sdk::Mc::SetD(env, f.player, id.eMotionY, -0.3 * vSpeed.value);
          } else {
            sdk::Mc::SetD(env, f.player, id.eMotionY, 0.0);
          }
        } else {
          sdk::Mc::SetD(env, f.player, id.eMotionY, 0.0);
        }
        env->CallVoidMethod(caps, id.capsFlySpeed, 0.2f);
        sdk::Mc::SetB(env, caps, id.cFlying, true);
        SetStrafeSpeed(f, 0.85 * hSpeed.value);
        break;
      }
      case 2: {  // Fast 2
        double jitter =
            std::uniform_real_distribution<double>(1.0e-7, 1.2e-7)(t_rng);
        jint ticks = sdk::Mc::In(env, f.player, id.ticksEx);
        if (ticks % 2 == 0) jitter = -jitter;
        if (!sdk::Mc::Bo(env, f.player, id.eOnGround)) {
          env->CallVoidMethod(f.player, id.setPosM, sdk::Mc::D(env, f.player, id.ePosX),
                              sdk::Mc::D(env, f.player, id.ePosY) + jitter,
                              sdk::Mc::D(env, f.player, id.ePosZ));
        }
        sdk::Mc::SetD(env, f.player, id.eMotionY, 0.0);
        SetStrafeSpeed(f, 0.4 * hSpeed.value);
        break;
      }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(caps);
  } while (false);

  mc.ReleaseFrame(f);
}

void Fly::OnDisable() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) {
    canFly_ = false;
    return;
  }
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();
  if (f.player) {
    jobject caps = sdk::Mc::Obj(env, f.player, id.pCaps);
    if (caps) {
      bool allow = sdk::Mc::Bo(env, caps, id.cAllowFly);
      // Mirrors Java: keep prior creative flight, else stop flying.
      sdk::Mc::SetB(env, caps, id.cFlying, allow && canFly_);
      env->DeleteLocalRef(caps);
    }
    sdk::Mc::SetD(env, f.player, id.eMotionX, 0.0);
    sdk::Mc::SetD(env, f.player, id.eMotionY, 0.0);
    sdk::Mc::SetD(env, f.player, id.eMotionZ, 0.0);
  }
  mc.ReleaseFrame(f);
  canFly_ = false;
}

}  // namespace modules
}  // namespace raven
