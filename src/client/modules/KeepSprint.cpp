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

#include "modules/KeepSprint.h"
#include <jni.h>
#include "core/Client.h"
#include "modules/Reduce.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

KeepSprint::KeepSprint() : core::Module("Keep Sprint", core::Category::Movement, 0) {
  RegisterSlider(&slow);
  RegisterBool(&stopSprint);
  RegisterBool(&disableJump);
  RegisterBool(&reachHits);
}

void KeepSprint::ForgetTarget(void* envPtr) {
  if (lastTarget_) {
    static_cast<JNIEnv*>(envPtr)->DeleteWeakGlobalRef(
        static_cast<jweak>(lastTarget_));
    lastTarget_ = nullptr;
  }
  lastHurt_ = -1;
}

void KeepSprint::OnDisable() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (env) ForgetTarget(env);
}

static double EyesDistance(JNIEnv* env, jobject self, jobject target) {
  auto& id = sdk::Mc::Instance().Ids();
  jobject a = env->CallObjectMethod(self, id.eGetEyes, 1.0f);
  jobject b = env->CallObjectMethod(target, id.eGetEyes, 1.0f);
  double d = -1.0;
  if (a && b && !env->ExceptionCheck()) {
    d = env->CallDoubleMethod(a, id.vDist, b);
  }
  if (a) env->DeleteLocalRef(a);
  if (b) env->DeleteLocalRef(b);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return -1.0;
  }
  return d;
}

void KeepSprint::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    // Reduce overrides KeepSprint (mirrors Java statics wiring).
    core::Module* r = core::Client::Instance().Modules().Find("Reduce");
    if (r && r->IsEnabled()) {
      ForgetTarget(env);
      break;
    }
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
    if (hurt > 0 && hurt == mx && !(same && lastHurt_ == hurt)) {
      bool vanilla = false;
      bool onGround = env->GetBooleanField(f.player, id.eOnGround);
      if (disableJump.value && !onGround) {
        vanilla = true;
      } else if (reachHits.value && mc.ExtraReady()) {
        double d = EyesDistance(env, f.player, target);
        if (d >= 0.0 && d <= 3.0) vanilla = true;
      }

      double motionX = env->GetDoubleField(f.player, id.eMotionX);
      double motionZ = env->GetDoubleField(f.player, id.eMotionZ);
      if (vanilla) {
        motionX *= 0.6;
        motionZ *= 0.6;
      } else {
        float mult = (100.f - static_cast<float>(slow.value)) / 100.f;
        motionX *= mult;
        motionZ *= mult;
      }
      if (stopSprint.value) {
        motionX *= 0.5;
        motionZ *= 0.5;
      }
      env->SetDoubleField(f.player, id.eMotionX, motionX);
      env->SetDoubleField(f.player, id.eMotionZ, motionZ);
    }

    ForgetTarget(env);
    lastTarget_ = env->NewWeakGlobalRef(target);
    lastHurt_ = hurt;
    env->DeleteLocalRef(target);
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
