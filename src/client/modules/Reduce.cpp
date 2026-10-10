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

#include "modules/Reduce.h"
#include <jni.h>
#include <random>
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

namespace {
thread_local std::mt19937 t_rng(std::random_device{}());
}

Reduce::Reduce() : core::Module("Reduce", core::Category::Combat, 0) {
  RegisterSlider(&reduction);
  RegisterSlider(&chance);
}

void Reduce::OnDisable() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (env) ForgetTarget(env);
}

void Reduce::ForgetTarget(void* envPtr) {
  if (lastTarget_) {
    static_cast<JNIEnv*>(envPtr)->DeleteWeakGlobalRef(
        static_cast<jweak>(lastTarget_));
    lastTarget_ = nullptr;
  }
  lastHurt_ = -1;
}

void Reduce::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player || !f.mouseOver) {
      ForgetTarget(env);
      break;
    }
    jobject target = env->GetObjectField(f.mouseOver, id.mopEntity);
    if (!target || !env->IsInstanceOf(target, id.living)) {
      if (target) env->DeleteLocalRef(target);
      ForgetTarget(env);
      break;
    }
    jint hurt = env->GetIntField(target, id.lHurtTime);
    jint mx = env->GetIntField(target, id.lMaxHurt);
    bool same =
        lastTarget_ && env->IsSameObject(target, static_cast<jobject>(lastTarget_));
    bool freshHit = hurt > 0 && hurt == mx && !(same && lastHurt_ == hurt);
    ForgetTarget(env);
    lastTarget_ = env->NewWeakGlobalRef(target);
    lastHurt_ = hurt;
    env->DeleteLocalRef(target);
    if (!freshHit) break;
    if (chance.value == 0.0) break;

    double motionX = env->GetDoubleField(f.player, id.eMotionX);
    double motionZ = env->GetDoubleField(f.player, id.eMotionZ);
    if (chance.value < 100.0) {
      double roll = std::uniform_real_distribution<double>(0.0, 1.0)(t_rng);
      if (roll >= chance.value / 100.0) {
        motionX *= 0.6;
        motionZ *= 0.6;
      } else {
        double n = (100.0 - reduction.value) / 100.0;
        motionX *= n;
        motionZ *= n;
      }
    } else {
      double n = (100.0 - reduction.value) / 100.0;
      motionX *= n;
      motionZ *= n;
    }
    env->SetDoubleField(f.player, id.eMotionX, motionX);
    env->SetDoubleField(f.player, id.eMotionZ, motionZ);
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
