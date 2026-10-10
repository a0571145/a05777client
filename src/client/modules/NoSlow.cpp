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

#include "modules/NoSlow.h"
#include <cmath>
#include <jni.h>
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

NoSlow::NoSlow() : core::Module("NoSlow", core::Category::Movement, 0) {
  RegisterMode(&mode);
  RegisterSlider(&slowed);
  RegisterBool(&disableBow);
  RegisterBool(&disablePotions);
  RegisterBool(&swordOnly);
  RegisterBool(&vanillaSword);
}

void NoSlow::OnDisable() { noSlowing = false; }

float NoSlow::SlowedFactor(void* envPtr, void* playerPtr) {
  JNIEnv* env = static_cast<JNIEnv*>(envPtr);
  jobject player = static_cast<jobject>(playerPtr);
  auto& id = sdk::Mc::Instance().Ids();

  jobject held = env->CallObjectMethod(player, id.lGetHeld);
  if (!held || env->ExceptionCheck()) {
    env->ExceptionClear();
    return 0.2f;
  }
  jobject item = env->CallObjectMethod(held, id.stGetItem);
  float factor = 0.2f;
  do {
    if (!item) break;
    bool isSword = env->IsInstanceOf(item, id.sword);
    if (swordOnly.value && !isSword) break;
    if (disableBow.value && env->IsInstanceOf(item, id.bow)) break;
    if (disablePotions.value && env->IsInstanceOf(item, id.potion)) {
      jint meta = env->CallIntMethod(held, id.stGetMeta);
      jboolean splash = env->CallStaticBooleanMethod(id.potion, id.potSplash, meta);
      if (!splash) break;
    }
    factor = (100.f - static_cast<float>(slowed.value)) / 100.f;
  } while (false);
  if (item) env->DeleteLocalRef(item);
  env->DeleteLocalRef(held);
  if (env->ExceptionCheck()) env->ExceptionClear();
  return factor;
}

static bool HoldingSword(JNIEnv* env, jobject player) {
  auto& id = sdk::Mc::Instance().Ids();
  jobject held = env->CallObjectMethod(player, id.lGetHeld);
  if (!held || env->ExceptionCheck()) {
    env->ExceptionClear();
    return false;
  }
  jobject item = env->CallObjectMethod(held, id.stGetItem);
  bool sword = item && env->IsInstanceOf(item, id.sword);
  if (item) env->DeleteLocalRef(item);
  env->DeleteLocalRef(held);
  if (env->ExceptionCheck()) env->ExceptionClear();
  return sword;
}

void NoSlow::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) return;
  if (!mc.EnsureExtra(env)) return;  // movementInput + packets
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player) {
      noSlowing = false;
      break;
    }
    float factor = SlowedFactor(env, f.player);
    jboolean usingItem = env->CallBooleanMethod(f.player, id.pIsUsingItem);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      noSlowing = false;
      break;
    }
    noSlowing = usingItem && factor != 0.2f;
    if (!noSlowing) break;

    // 1) Input rescale (replaces the movement-input Mixin hook).
    jobject mi = env->GetObjectField(f.player, id.spMoveInput);
    if (mi) {
      float mf = env->GetFloatField(mi, id.inForward);
      float ms = env->GetFloatField(mi, id.inStrafe);
      if (std::fabs(mf) > 1e-4f) {
        env->SetFloatField(mi, id.inForward, mf / 0.2f * factor);
      }
      if (std::fabs(ms) > 1e-4f) {
        env->SetFloatField(mi, id.inStrafe, ms / 0.2f * factor);
      }
      env->DeleteLocalRef(mi);
    }

    // 2) Beta-mode packets (mirrors onPreUpdate).
    if (mode.index == 1 && !(vanillaSword.value && HoldingSword(env, f.player))) {
      jobject queue = env->GetObjectField(f.player, id.spSendQueue);
      if (queue) {
        jobject inv = env->GetObjectField(f.player, id.pInventory);
        jint cur = inv ? env->GetIntField(inv, id.invCurrent) : 0;
        if (inv) env->DeleteLocalRef(inv);
        jobject p1 = env->NewObject(id.c09, id.c09Ctor, (cur % 8) + 1);
        jobject p2 = env->NewObject(id.c09, id.c09Ctor, cur);
        jobject p3 = env->NewObject(id.c07, id.c07Ctor, id.releaseAction,
                                    id.originPos, id.downFace);
        if (p1) {
          env->CallVoidMethod(queue, id.nhSend, p1);
          env->DeleteLocalRef(p1);
        }
        if (p2) {
          env->CallVoidMethod(queue, id.nhSend, p2);
          env->DeleteLocalRef(p2);
        }
        if (p3) {
          env->CallVoidMethod(queue, id.nhSend, p3);
          env->DeleteLocalRef(p3);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(queue);
      }
    }
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
