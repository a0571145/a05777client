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

#include "modules/SafeWalk.h"
#include <jni.h>
#include "modules/MoveUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

SafeWalk::SafeWalk() : core::Module("SafeWalk", core::Category::Player, 0) {
  RegisterSlider(&sneakDelay);
  RegisterSlider(&motion);
  RegisterBool(&blocksOnly);
  RegisterBool(&disableOnForward);
  RegisterBool(&pitchCheck);
  RegisterBool(&sneak);
}

bool SafeWalk::SettingsMet(void* envPtr, void* playerPtr) {
  JNIEnv* env = static_cast<JNIEnv*>(envPtr);
  jobject player = static_cast<jobject>(playerPtr);
  auto& id = sdk::Mc::Instance().Ids();
  if (blocksOnly.value) {
    jobject held = env->CallObjectMethod(player, id.lGetHeld);
    jobject item =
        (held && !env->ExceptionCheck()) ? env->CallObjectMethod(held, id.stGetItem) : nullptr;
    bool block = item && env->IsInstanceOf(item, id.itemblock);
    if (item) env->DeleteLocalRef(item);
    if (held) env->DeleteLocalRef(held);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      return false;
    }
    if (!block) return false;
  }
  if (disableOnForward.value) {
    jobject mi = env->GetObjectField(player, id.spMoveInput);
    float mf = sdk::Mc::Fl(env, mi, id.inForward);
    if (mi) env->DeleteLocalRef(mi);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (mf > 0.1f) return false;
  }
  if (pitchCheck.value) {
    float pitch = sdk::Mc::Fl(env, player, id.ePitch);
    if (pitch < 70.f) return false;
  }
  return true;
}

void SafeWalk::SetSneak(void* envPtr, bool state) {
  JNIEnv* env = static_cast<JNIEnv*>(envPtr);
  if (!state) unsneakTicks_ = 0;
  if (sneaking_ == state) return;
  auto& id = sdk::Mc::Instance().Ids();
  sdk::Mc::Frame f;
  if (!sdk::Mc::Instance().BeginFrame(env, f)) return;
  do {
    if (!f.settings) break;
    jobject key = sdk::Mc::Obj(env, f.settings, id.sneakKey);
    if (!key) break;
    jint code = env->CallIntMethod(key, id.kGetCode);
    env->DeleteLocalRef(key);
    if (env->ExceptionCheck() || code == 0) {
      env->ExceptionClear();
      break;
    }
    // Don't fight a physically held sneak key when releasing.
    jobject mi = sdk::Mc::Obj(env, f.player, id.spMoveInput);
    bool held = sdk::Mc::Bo(env, mi, id.inSneak);
    if (mi) env->DeleteLocalRef(mi);
    if (!state && held) break;
    env->CallStaticVoidMethod(id.keyBinding, id.kSetState, code,
                              state ? JNI_TRUE : JNI_FALSE);
    if (state) env->CallStaticVoidMethod(id.keyBinding, id.kOnTick, code);
    if (env->ExceptionCheck()) env->ExceptionClear();
    sneaking_ = state;
  } while (false);
  sdk::Mc::Instance().ReleaseFrame(f);
}

void SafeWalk::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player) break;
    bool onGround = sdk::Mc::Bo(env, f.player, id.eOnGround);
    if (motion.value != 1.0 && onGround && MovingInput(f) && SettingsMet(env, f.player)) {
      sdk::Mc::SetD(env, f.player, id.eMotionX,
                    sdk::Mc::D(env, f.player, id.eMotionX) * motion.value);
      sdk::Mc::SetD(env, f.player, id.eMotionZ,
                    sdk::Mc::D(env, f.player, id.eMotionZ) * motion.value);
    }
    if (!sneak.value) break;

    bool edge = onGround && EdgeOfBlock(f);
    if (edge) {
      if (!SettingsMet(env, f.player)) {
        SetSneak(env, false);
        break;
      }
      if (!sneaking_) {
        SetSneak(env, true);
        unsneakTicks_ = static_cast<int>(sneakDelay.value);
      }
    } else if (sneaking_) {
      if (!SettingsMet(env, f.player)) {
        SetSneak(env, false);
        break;
      }
      if (unsneakTicks_ > 0) {
        unsneakTicks_--;
        break;
      }
      SetSneak(env, false);
    }
    if (sneaking_) {
      jobject caps = sdk::Mc::Obj(env, f.player, id.pCaps);
      bool flying = caps && sdk::Mc::Bo(env, caps, id.cFlying);
      if (caps) env->DeleteLocalRef(caps);
      if (flying || !SettingsMet(env, f.player)) SetSneak(env, false);
    }
  } while (false);

  mc.ReleaseFrame(f);
}

void SafeWalk::OnDisable() {
  unsneakTicks_ = 0;
  if (sneaking_) {
    // Release the forced sneak key so it never sticks.
    JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
    if (env && sdk::Mc::Instance().Ensure(env)) SetSneak(env, false);
  }
  sneaking_ = false;
}

}  // namespace modules
}  // namespace raven
