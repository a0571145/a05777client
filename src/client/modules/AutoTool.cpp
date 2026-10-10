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

#include "modules/AutoTool.h"
#include <chrono>
#include "modules/CombatUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

AutoTool::AutoTool() : core::Module("Auto Tool", core::Category::Player, 0) {
  RegisterBool(&requireMouse);
  RegisterSlider(&activation);
  RegisterSlider(&hoverDelay);
  RegisterBool(&onlyCrouch);
  RegisterBool(&switchBack);
}

long long AutoTool::NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void AutoTool::ResetSlot(JNIEnv* env, sdk::Mc::Frame& f) {
  if (previous_ == -1) return;
  auto& id = sdk::Mc::Instance().Ids();
  if (switchBack.value) {
    jobject inv = sdk::Mc::Obj(env, f.player, id.pInventory);
    if (inv) {
      sdk::Mc::SetI(env, inv, id.invCurrent, previous_);
      if (id.mcController && id.ctlSyncItem) {
        jobject ctl = sdk::Mc::Obj(env, f.mc, id.mcController);
        if (ctl) {
          env->CallVoidMethod(ctl, id.ctlSyncItem);
          env->DeleteLocalRef(ctl);
        }
      }
      env->DeleteLocalRef(inv);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }
  previous_ = -1;
}

void AutoTool::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();
  do {
    if (!f.player || !f.world || !f.settings || f.screen ||
        !combat::ForegroundIsGame()) {
      ResetSlot(env, f);
      ResetTimers();
      break;
    }
    // Adventure gate, same as AutoClicker.
    bool canEdit = true;
    jobject caps = sdk::Mc::Obj(env, f.player, id.pCaps);
    if (caps) {
      canEdit = env->GetBooleanField(caps, id.cCanEdit);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        canEdit = true;
      }
      env->DeleteLocalRef(caps);
    }
    if (!canEdit) {
      ResetSlot(env, f);
      ResetTimers();
      break;
    }
    bool leftDown = combat::LmbDown();
    long long now = NowMs();
    if (leftDown) {
      if (leftStartMs_ == 0) leftStartMs_ = now;
    } else {
      leftStartMs_ = 0;
    }
    if (requireMouse.value && !leftDown) {
      ResetSlot(env, f);
      hoverStartMs_ = 0;
      break;
    }
    if (onlyCrouch.value && id.isSneakingM) {
      jboolean sneak = env->CallBooleanMethod(f.player, id.isSneakingM);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        sneak = JNI_FALSE;
      }
      if (!sneak) {
        ResetSlot(env, f);
        break;
      }
    }
    if (id.pIsUsingItem) {
      jboolean usingItem = env->CallBooleanMethod(f.player, id.pIsUsingItem);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        usingItem = JNI_FALSE;
      }
      if (usingItem) {
        ResetSlot(env, f);
        break;
      }
    }
    if (!f.mouseOver || !id.mopBlockPos || !id.wBlockState || !id.ibsGetBlock) break;
    jobject pos = env->CallObjectMethod(f.mouseOver, id.mopBlockPos);
    if (!pos || env->ExceptionCheck()) {
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (pos) env->DeleteLocalRef(pos);
      ResetSlot(env, f);
      hoverStartMs_ = 0;
      break;
    }
    if (hoverStartMs_ == 0) hoverStartMs_ = now;
    jobject state =
        f.world ? env->CallObjectMethod(f.world, id.wBlockState, pos) : nullptr;
    env->DeleteLocalRef(pos);
    jobject block =
        (!env->ExceptionCheck() && state)
            ? env->CallObjectMethod(state, id.ibsGetBlock)
            : nullptr;
    if (state) env->DeleteLocalRef(state);
    if (env->ExceptionCheck() || !block) {
      env->ExceptionClear();
      if (block) env->DeleteLocalRef(block);
      ResetSlot(env, f);
      break;
    }
    bool solid = !env->IsSameObject(block, id.airBlock) &&
                 !env->IsInstanceOf(block, id.liquid);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!solid) {
      env->DeleteLocalRef(block);
      ResetSlot(env, f);
      break;
    }
    if (now - leftStartMs_ < static_cast<long long>(activation.value) ||
        now - hoverStartMs_ < static_cast<long long>(hoverDelay.value)) {
      env->DeleteLocalRef(block);
      ResetSlot(env, f);
      break;
    }
    // Best hotbar slot by getStrVsBlock strength (hand == 1.0 baseline).
    int best = -1;
    float bestStr = 1.0f;
    jobject inv = sdk::Mc::Obj(env, f.player, id.pInventory);
    jobjectArray hotbar = (inv && id.invMain)
                              ? static_cast<jobjectArray>(
                                    env->GetObjectField(inv, id.invMain))
                              : nullptr;
    if (hotbar && !env->ExceptionCheck()) {
      jsize n = env->GetArrayLength(hotbar);
      if (n > 9) n = 9;
      for (jsize i = 0; i < n; ++i) {
        jobject stack = env->GetObjectArrayElement(hotbar, i);
        if (!stack || env->ExceptionCheck()) {
          env->ExceptionClear();
          if (stack) env->DeleteLocalRef(stack);
          continue;
        }
        jobject item = env->CallObjectMethod(stack, id.stGetItem);
        if (!env->ExceptionCheck() && item && id.itStrVs) {
          jfloat str = env->CallFloatMethod(item, id.itStrVs, stack, block);
          if (!env->ExceptionCheck() && str > bestStr) {
            bestStr = str;
            best = static_cast<int>(i);
          } else if (env->ExceptionCheck()) {
            env->ExceptionClear();
          }
        } else if (env->ExceptionCheck()) {
          env->ExceptionClear();
        }
        if (item) env->DeleteLocalRef(item);
        env->DeleteLocalRef(stack);
      }
    } else if (env->ExceptionCheck()) {
      env->ExceptionClear();
    }
    if (hotbar) env->DeleteLocalRef(hotbar);
    env->DeleteLocalRef(block);
    if (best == -1 || !inv) {
      if (inv) env->DeleteLocalRef(inv);
      ResetSlot(env, f);
      break;
    }
    jint cur = env->GetIntField(inv, id.invCurrent);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      env->DeleteLocalRef(inv);
      ResetSlot(env, f);
      break;
    }
    if (previous_ == -1 && best != cur) previous_ = cur;
    if (best != cur) {
      env->SetIntField(inv, id.invCurrent, best);
      if (id.mcController && id.ctlSyncItem) {
        jobject ctl = sdk::Mc::Obj(env, f.mc, id.mcController);
        if (ctl) {
          env->CallVoidMethod(ctl, id.ctlSyncItem);
          env->DeleteLocalRef(ctl);
        }
      }
    }
    env->DeleteLocalRef(inv);
    if (env->ExceptionCheck()) env->ExceptionClear();
  } while (false);
  mc.ReleaseFrame(f);
}

void AutoTool::OnDisable() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  ResetSlot(env, f);
  ResetTimers();
  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
