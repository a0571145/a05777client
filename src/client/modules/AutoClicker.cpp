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

#include "modules/AutoClicker.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <windows.h>
#include "hooks/Hooks.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

namespace {
thread_local std::mt19937 t_rng(std::random_device{}());

long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

bool LmbDown() { return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0; }

bool InGameWindow() {
  HWND mcw = hooks::Hooks::Instance().Window();
  return mcw && GetForegroundWindow() == mcw;
}

bool HoldingWeapon(JNIEnv* env, jobject player) {
  auto& id = sdk::Mc::Instance().Ids();
  jobject held = env->CallObjectMethod(player, id.lGetHeld);
  if (!held || env->ExceptionCheck()) {
    env->ExceptionClear();
    return false;
  }
  jobject item = env->CallObjectMethod(held, id.stGetItem);
  bool ok = item && (env->IsInstanceOf(item, id.sword) || env->IsInstanceOf(item, id.axe));
  if (item) env->DeleteLocalRef(item);
  env->DeleteLocalRef(held);
  if (env->ExceptionCheck()) env->ExceptionClear();
  return ok;
}
}  // namespace

AutoClicker::AutoClicker() : core::Module("Auto Clicker", core::Category::Combat, 0) {
  RegisterSlider(&targetCPS);
  RegisterBool(&exhaust);
  RegisterBool(&notUsingItem);
  RegisterBool(&breakBlocks);
  RegisterBool(&weaponOnly);
  RegisterBool(&disableCreative);
  RegisterBool(&inventory);
  RegisterSlider(&startDelay);
}

void AutoClicker::OnEnable() {
  nextClickMs_ = 0;
  invNextMs_ = 0;
  holdingBreak_ = false;
}

void AutoClicker::OnDisable() {
  nextClickMs_ = 0;
  invNextMs_ = 0;
  holdingBreak_ = false;
}

long long AutoClicker::NextDelay() {
  int target = std::max(1, static_cast<int>(targetCPS.value));
  int base = 1000 / target;
  int finalDelay;
  if (exhaust.value) {
    std::uniform_int_distribution<int> var(0, base);
    finalDelay = base + (var(t_rng) - base / 2);
    std::uniform_int_distribution<int> pct(0, 99);
    if (pct(t_rng) < 15) {
      if (pct(t_rng) % 2 == 0) {
        finalDelay = 25 + (t_rng() % 16);
      } else {
        finalDelay = base + 50 + (t_rng() % 41);
      }
    }
    if (pct(t_rng) < 8) {
      int mult = 50 + (t_rng() % 151);
      finalDelay = finalDelay * mult / 100;
    }
    if (pct(t_rng) < 10) {
      finalDelay += 10 + (t_rng() % 26);
    }
  } else {
    finalDelay = base + (static_cast<int>(t_rng() % 21) - 10);
  }
  return std::max(33LL, std::min(180LL, static_cast<long long>(finalDelay)));
}

void AutoClicker::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) return;
  // Attack path needs core only; inventory + break-blocks need tier-2.
  const bool extra = mc.EnsureExtra(env);
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player || !f.settings || !f.attackKey) break;
    bool lmb = LmbDown();
    jint code = env->CallIntMethod(f.attackKey, id.kGetCode);
    if (env->ExceptionCheck() || code == 0) {
      env->ExceptionClear();
      break;
    }

    // Inventory path (mirrors the Java RenderTick handler).
    if (extra && inventory.value && f.screen &&
        env->IsInstanceOf(f.screen, id.guiContainer)) {
      if (!lmb) {
        invNextMs_ = 0;
      } else {
        long long now = NowMs();
        if (invNextMs_ == 0) invNextMs_ = now + static_cast<long long>(startDelay.value);
        int clicks = 0;
        while (invNextMs_ <= now) {
          clicks++;
          invNextMs_ += NextDelay();
        }
        if (clicks > 0) {
          jobject slot = env->GetObjectField(f.screen, id.gSlot);
          if (slot) {
            jint slotId = env->GetIntField(slot, id.slotNum);
            env->DeleteLocalRef(slot);
            if (slotId >= 0 && !env->ExceptionCheck()) {
              jobject cont = env->GetObjectField(f.screen, id.gContainer);
              jobject ctl = env->GetObjectField(f.mc, id.mcController);
              if (cont && ctl) {
                jint windowId = env->GetIntField(cont, id.contWindow);
                jboolean shift =
                    env->CallStaticBooleanMethod(id.guiScreen, id.scrShift);
                for (int i = 0; i < clicks; i++) {
                  env->CallObjectMethod(ctl, id.ctlWindowClick, windowId, slotId, 0,
                                        shift ? 1 : 0, f.player);
                }
              }
              if (cont) env->DeleteLocalRef(cont);
              if (ctl) env->DeleteLocalRef(ctl);
            }
          }
          if (env->ExceptionCheck()) env->ExceptionClear();
        }
      }
    }

    // Attack path (mirrors onPrePlayerInteract; no KillAura in Batch 1).
    if (!lmb) {
      nextClickMs_ = 0;
      if (holdingBreak_) {
        env->CallStaticVoidMethod(id.keyBinding, id.kSetState, code, JNI_FALSE);
        holdingBreak_ = false;
      }
      env->CallStaticVoidMethod(id.keyBinding, id.kSetState, code, JNI_FALSE);
      break;
    }
    if (!InGameWindow()) {
      nextClickMs_ = 0;
      break;
    }

    long long now = NowMs();
    if (nextClickMs_ == 0) nextClickMs_ = now + NextDelay();
    int clicks = 0;
    while (nextClickMs_ <= now) {
      clicks++;
      nextClickMs_ += NextDelay();
    }
    if (clicks <= 0) break;

    if (notUsingItem.value &&
        env->CallBooleanMethod(f.player, id.pIsUsingItem)) {
      if (env->ExceptionCheck()) env->ExceptionClear();
      break;
    }
    jobject caps = env->GetObjectField(f.player, id.pCaps);
    bool creative = caps && env->GetBooleanField(caps, id.cCreative);
    // Java gates on field_75099_e (inferred allowEdit): no auto-break in adventure.
    bool canEdit = !caps || env->GetBooleanField(caps, id.cCanEdit);
    if (caps) env->DeleteLocalRef(caps);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (disableCreative.value && creative) break;
    if (f.screen || !f.hasFocus) break;
    if (weaponOnly.value && !HoldingWeapon(env, f.player)) break;

    if (extra && breakBlocks.value) {
      if (!canEdit) {
        if (holdingBreak_) {
          env->CallStaticVoidMethod(id.keyBinding, id.kSetState, code, JNI_FALSE);
          holdingBreak_ = false;
        }
      } else if (f.mouseOver) {
        jobject pos = env->CallObjectMethod(f.mouseOver, id.mopBlockPos);
        if (pos && f.world) {
          jobject state = env->CallObjectMethod(f.world, id.wBlockState, pos);
          jobject block = state ? env->CallObjectMethod(state, id.ibsGetBlock) : nullptr;
          bool solid = block && !env->IsSameObject(block, id.airBlock) &&
                       !env->IsInstanceOf(block, id.liquid);
          if (solid) {
            if (!holdingBreak_) {
              env->CallStaticVoidMethod(id.keyBinding, id.kSetState, code, JNI_TRUE);
              holdingBreak_ = true;
            }
            if (state) env->DeleteLocalRef(state);
            if (block) env->DeleteLocalRef(block);
            env->DeleteLocalRef(pos);
            if (env->ExceptionCheck()) env->ExceptionClear();
            break;  // holding break; no attack clicks this tick
          }
          if (state) env->DeleteLocalRef(state);
          if (block) env->DeleteLocalRef(block);
        }
        if (pos) env->DeleteLocalRef(pos);
        if (holdingBreak_) {
          env->CallStaticVoidMethod(id.keyBinding, id.kSetState, code, JNI_FALSE);
          holdingBreak_ = false;
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
      } else {
        holdingBreak_ = false;
      }
    }

    for (int i = 0; i < clicks; i++) {
      env->CallStaticVoidMethod(id.keyBinding, id.kOnTick, code);
    }
    env->CallStaticVoidMethod(id.keyBinding, id.kSetState, code, JNI_TRUE);
    if (env->ExceptionCheck()) env->ExceptionClear();
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
