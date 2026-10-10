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

#include "modules/FastPlace.h"
#include <chrono>
#include "modules/CombatUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

FastPlace::FastPlace() : core::Module("Fast Place", core::Category::Player, 0) {
  RegisterSlider(&tickDelay);
  RegisterSlider(&activation);
  RegisterBool(&blocksOnly);
  RegisterBool(&pitchCheck);
}

long long FastPlace::NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void FastPlace::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();
  do {
    if (!f.player || !f.settings || f.screen || !combat::ForegroundIsGame()) {
      rightStartMs_ = 0;
      break;
    }
    bool useDown = combat::RmbDown();
    bool usingItem = false;
    if (id.pIsUsingItem) {
      usingItem = env->CallBooleanMethod(f.player, id.pIsUsingItem);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        usingItem = false;
      }
    }
    if (!useDown && !usingItem) {
      rightStartMs_ = 0;
      break;
    }
    long long now = NowMs();
    if (rightStartMs_ == 0) rightStartMs_ = now;
    if (blocksOnly.value && id.lGetHeld && id.stGetItem && id.itemblock) {
      jobject held = env->CallObjectMethod(f.player, id.lGetHeld);
      jobject item = (!env->ExceptionCheck() && held)
                         ? env->CallObjectMethod(held, id.stGetItem)
                         : nullptr;
      bool isBlock = item && !env->ExceptionCheck() &&
                     env->IsInstanceOf(item, id.itemblock);
      if (item) env->DeleteLocalRef(item);
      if (held) env->DeleteLocalRef(held);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (!isBlock) break;  // keep rightStartMs_ while held (Java keeps timer)
    }
    if (pitchCheck.value) {
      float pitch = sdk::Mc::Fl(env, f.player, id.ePitch);
      if (pitch < 70.f) break;
    }
    if (now - rightStartMs_ < static_cast<long long>(activation.value)) break;
    if (!id.mcRightDelay) break;
    int delay = static_cast<int>(tickDelay.value);
    if (delay <= 0) {
      sdk::Mc::SetI(env, f.mc, id.mcRightDelay, 0);
    } else {
      jint cur = sdk::Mc::In(env, f.mc, id.mcRightDelay);
      if (cur > delay) sdk::Mc::SetI(env, f.mc, id.mcRightDelay, delay);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  } while (false);
  mc.ReleaseFrame(f);
}

void FastPlace::OnDisable() { rightStartMs_ = 0; }

}  // namespace modules
}  // namespace raven
