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

#include "modules/NoFall.h"
#include <cmath>
#include <jni.h>
#include <random>
#include "modules/MoveUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

namespace {
thread_local std::mt19937 t_rng(std::random_device{}());
}

NoFall::NoFall() : core::Module("NoFall", core::Category::Player, 0) {
  RegisterMode(&mode);
  RegisterSlider(&minFall);
  RegisterBool(&ignoreVoid);
}

void NoFall::SetTimer(JNIEnv* env, float speed) {
  sdk::Mc& mc = sdk::Mc::Instance();
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  jobject timer = sdk::Mc::Obj(env, f.mc, mc.Ids().mcTimer);
  if (timer) {
    env->SetFloatField(timer, mc.Ids().timerSpeed, speed);
    env->DeleteLocalRef(timer);
  }
  if (env->ExceptionCheck()) env->ExceptionClear();
  mc.ReleaseFrame(f);
}

void NoFall::SendGround(JNIEnv* env, bool onGround) {
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();
  do {
    if (!f.player) break;
    jobject queue = sdk::Mc::Obj(env, f.player, id.spSendQueue);
    if (!queue) break;
    jobject pkt = env->NewObject(id.c03, id.c03Ctor, onGround ? JNI_TRUE : JNI_FALSE);
    if (pkt && !env->ExceptionCheck()) {
      env->CallVoidMethod(queue, id.nhSend, pkt);
      env->DeleteLocalRef(pkt);
    } else {
      env->ExceptionClear();
    }
    env->DeleteLocalRef(queue);
  } while (false);
  mc.ReleaseFrame(f);
}

void NoFall::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  // reset() port (adventure/replay gates omitted: need WorldInfo).
  bool reset = false;
  do {
    if (!f.player || !f.world) {
      reset = true;
      break;
    }
    double px = sdk::Mc::D(env, f.player, id.ePosX);
    double py = sdk::Mc::D(env, f.player, id.ePosY);
    double pz = sdk::Mc::D(env, f.player, id.ePosZ);
    double motionY = sdk::Mc::D(env, f.player, id.eMotionY);
    bool onGround = sdk::Mc::Bo(env, f.player, id.eOnGround);
    jobject caps = sdk::Mc::Obj(env, f.player, id.pCaps);
    bool creative = caps && sdk::Mc::Bo(env, caps, id.cCreative);
    bool flying = caps && sdk::Mc::Bo(env, caps, id.cFlying);
    if (caps) env->DeleteLocalRef(caps);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      reset = true;
      break;
    }
    jboolean spec = env->CallBooleanMethod(f.player, id.pIsSpectator);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      reset = true;
      break;
    }
    if (spec) {
      reset = true;
      break;
    }
    if (onGround) {
      reset = true;
      break;
    }
    jobject below = BlockAt(f, static_cast<int>(std::floor(px)),
                            static_cast<int>(std::floor(py)) - 1,
                            static_cast<int>(std::floor(pz)));
    bool solidBelow = below && !IsAirBlock(f, below);
    if (below) env->DeleteLocalRef(below);
    if (solidBelow) {
      reset = true;
      break;
    }
    if (motionY > -0.0784) {
      reset = true;
      break;
    }
    if (creative) {
      reset = true;
      break;
    }
    bool isVoid = false;
    if (motionY < -0.3) isVoid = OverVoid(f);
    if (ignoreVoid.value && isVoid) {
      reset = true;
      break;
    }
    if (isVoid && py <= 41.0) {
      reset = true;
      break;
    }
    if (flying) reset = true;
  } while (false);

  if (reset) {
    SetTimer(env, 1.0f);
    if (f.player) initialY_ = sdk::Mc::D(env, f.player, id.ePosY);
    falling_ = false;
    mc.ReleaseFrame(f);
    return;
  }

  float fallDist = sdk::Mc::Fl(env, f.player, id.fallDist);
  double posY = sdk::Mc::D(env, f.player, id.ePosY);
  double motionY = sdk::Mc::D(env, f.player, id.eMotionY);
  jint ticks = sdk::Mc::In(env, f.player, id.ticksEx);
  if (fallDist >= minFall.value) falling_ = true;
  double predictedY = posY + motionY;
  double distanceFallen = initialY_ - predictedY;
  if (motionY >= -1.0) {
    dynamic_ = 3.0;
  } else if (motionY >= -2.0) {
    dynamic_ = 4.0;
  } else {
    dynamic_ = 5.0;
  }
  mc.ReleaseFrame(f);

  int m = mode.index;
  if (falling_ && m == 2 && distanceFallen >= dynamic_) {
    double jitter = std::uniform_real_distribution<double>(-0.012, 0.012)(t_rng);
    SetTimer(env, static_cast<float>(0.7399789 + jitter));
    SendGround(env, true);
    initialY_ = posY;
  } else if (falling_ && m == 3) {
    SetTimer(env, (ticks % 2 == 0)
                      ? static_cast<float>(
                            std::uniform_real_distribution<double>(0.5, 0.50201)(t_rng))
                      : 1.0f);
    if (distanceFallen >= 3.0) {
      SendGround(env, true);
      initialY_ = posY;
    }
  } else if (falling_ && m == 0 && distanceFallen >= minFall.value) {
    SendGround(env, true);  // Spoof ~= grounded C03 while falling
  } else if (falling_ && m == 1 && distanceFallen >= minFall.value) {
    SendGround(env, false);  // NoGround ~= ungrounded C03 while falling
  }
}

void NoFall::OnDisable() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  if (!sdk::Mc::Instance().Ensure(env)) return;
  SetTimer(env, 1.0f);
  falling_ = false;
}

}  // namespace modules
}  // namespace raven
