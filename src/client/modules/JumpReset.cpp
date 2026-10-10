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

#include "modules/JumpReset.h"
#include <jni.h>
#include <random>
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

namespace {
thread_local std::mt19937 t_rng(std::random_device{}());
}

JumpReset::JumpReset() : core::Module("Jump Reset", core::Category::Combat, 0) {
  RegisterSlider(&chance);
  RegisterBool(&requireMouse);
  RegisterBool(&requireForward);
  RegisterBool(&requireAim);
}

void JumpReset::OnDisable() {
  setJump_ = false;
  ignoreNext_ = false;
}

static bool HasBadEffect(JNIEnv* env, jobject player) {
  auto& id = sdk::Mc::Instance().Ids();
  const jfieldID pots[] = {id.potJump, id.potPoison, id.potWither};
  for (jfieldID pf : pots) {
    jobject potion = env->GetStaticObjectField(id.potionCls, pf);
    jobject fx = (potion && !env->ExceptionCheck())
                     ? env->CallObjectMethod(player, id.isPotionActive, potion)
                     : nullptr;
    if (potion) env->DeleteLocalRef(potion);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      continue;
    }
    if (fx) {
      env->DeleteLocalRef(fx);
      return true;
    }
  }
  return false;
}

void JumpReset::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player || !f.settings) break;
    jint hurt = sdk::Mc::In(env, f.player, id.lMaxHurt);
    bool onGround = sdk::Mc::Bo(env, f.player, id.eOnGround);
    float fall = sdk::Mc::Fl(env, f.player, id.fallDist);
    jobject caps = sdk::Mc::Obj(env, f.player, id.pCaps);
    bool allowFly = caps && sdk::Mc::Bo(env, caps, id.cAllowFly);
    if (caps) env->DeleteLocalRef(caps);

    if (onGround && lastFall_ > 3.f && !allowFly) ignoreNext_ = true;

    if (hurt > lastHurt_) {
      bool mouse = !requireMouse.value;
      if (!mouse) {
        jobject atk = sdk::Mc::Obj(env, f.settings, id.sKeyAttack);
        // KeyBinding.isKeyDown ~ pressed field (no LWJGL here).
        mouse = atk && sdk::Mc::Bo(env, atk, id.kPressed);
        if (atk) env->DeleteLocalRef(atk);
      }
      bool aim = !requireAim.value;
      if (!aim && f.mouseOver) {
        jobject t = env->GetObjectField(f.mouseOver, id.mopEntity);
        aim = t && env->IsInstanceOf(t, id.entityPlayer);
        if (t) env->DeleteLocalRef(t);
      }
      bool forward = !requireForward.value;
      if (!forward) {
        jobject mi = sdk::Mc::Obj(env, f.player, id.spMoveInput);
        forward = sdk::Mc::Fl(env, mi, id.inForward) > 0.f;
        if (mi) env->DeleteLocalRef(mi);
      }
      bool random = chance.value >= 100.0 ||
                    std::uniform_real_distribution<double>(0.0, 100.0)(t_rng) < chance.value;
      jboolean sneaking = env->CallBooleanMethod(f.player, id.isSneakingM);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        break;
      }
      if (!ignoreNext_ && !sneaking && onGround && aim && forward && mouse && random &&
          !HasBadEffect(env, f.player)) {
        jobject jumpKey = sdk::Mc::Obj(env, f.settings, id.jumpKey);
        if (jumpKey) {
          jint code = env->CallIntMethod(jumpKey, id.kGetCode);
          env->DeleteLocalRef(jumpKey);
          if (!env->ExceptionCheck() && code != 0) {
            env->CallStaticVoidMethod(id.keyBinding, id.kSetState, code, JNI_TRUE);
            setJump_ = true;
          } else {
            env->ExceptionClear();
          }
        }
      }
      ignoreNext_ = false;
    }
    lastHurt_ = hurt;
    lastFall_ = fall;

    // Release the tapped jump once the input clears (mirrors onPostMotion).
    if (setJump_) {
      jobject mi = sdk::Mc::Obj(env, f.player, id.spMoveInput);
      bool held = sdk::Mc::Bo(env, mi, id.inJump);
      if (mi) env->DeleteLocalRef(mi);
      if (!held) {
        jobject jumpKey = sdk::Mc::Obj(env, f.settings, id.jumpKey);
        if (jumpKey) {
          jint code = env->CallIntMethod(jumpKey, id.kGetCode);
          env->DeleteLocalRef(jumpKey);
          if (!env->ExceptionCheck() && code != 0) {
            env->CallStaticVoidMethod(id.keyBinding, id.kSetState, code, JNI_FALSE);
          } else {
            env->ExceptionClear();
          }
        }
        setJump_ = false;
      }
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
