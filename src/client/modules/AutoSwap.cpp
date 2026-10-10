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

#include "modules/AutoSwap.h"
#include <chrono>
#include "modules/CombatUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

AutoSwap::AutoSwap() : core::Module("Auto Swap", core::Category::Player, 0) {}

long long AutoSwap::NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void AutoSwap::ResetTrack() {
  // Called with a valid env from OnUpdate/OnDisable (needs JNIEnv, so the
  // global ref is released by the caller, not here).
  haveTrack_ = false;
  trackedMeta_ = 0;
  lastSwapSlot_ = -1;
  lastSwapMs_ = 0;
}

void AutoSwap::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();
  do {
    if (!f.player || !f.settings || f.screen || !combat::ForegroundIsGame() ||
        !combat::RmbDown()) {
      break;
    }
    if (!id.lGetHeld || !id.stGetItem || !id.itemblock || !id.stSize ||
        !id.pInventory || !id.invCurrent || !id.invSlot || !id.stGetMeta) {
      break;
    }
    jobject held = env->CallObjectMethod(f.player, id.lGetHeld);
    if (!held || env->ExceptionCheck()) {
      env->ExceptionClear();
      if (held) env->DeleteLocalRef(held);
      break;
    }
    jobject item = env->CallObjectMethod(held, id.stGetItem);
    bool heldBlock = item && !env->ExceptionCheck() &&
                     env->IsInstanceOf(item, id.itemblock);
    jint size = heldBlock ? env->GetIntField(held, id.stSize) : 0;
    jint meta = heldBlock ? env->CallIntMethod(held, id.stGetMeta) : 0;
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      heldBlock = false;
    }
    if (heldBlock && size > 0) {
      // Track the placed type (fresh global ref each time the type changes).
      jclass cls = env->GetObjectClass(item);
      bool same = haveTrack_ && trackedCls_ && cls &&
                  env->IsSameObject(trackedCls_, cls) && trackedMeta_ == meta;
      if (cls) {
        if (!same) {
          if (trackedCls_) env->DeleteGlobalRef(trackedCls_);
          trackedCls_ = static_cast<jclass>(env->NewGlobalRef(cls));
          trackedMeta_ = meta;
          haveTrack_ = trackedCls_ != nullptr;
          lastSwapSlot_ = -1;
        }
        env->DeleteLocalRef(cls);
      }
      if (item) env->DeleteLocalRef(item);
      env->DeleteLocalRef(held);
      if (env->ExceptionCheck()) env->ExceptionClear();
      break;  // stocked: nothing to do
    }
    if (item) env->DeleteLocalRef(item);
    env->DeleteLocalRef(held);
    if (env->ExceptionCheck()) env->ExceptionClear();
    // Out of blocks while placing: refill from the hotbar (8 -> 0).
    if (!haveTrack_ || !trackedCls_) break;
    jobject inv = sdk::Mc::Obj(env, f.player, id.pInventory);
    if (!inv) break;
    jint cur = env->GetIntField(inv, id.invCurrent);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      env->DeleteLocalRef(inv);
      break;
    }
    long long now = NowMs();
    for (int slot = 8; slot >= 0; --slot) {
      if (slot == cur) continue;
      if (slot == lastSwapSlot_ && now - lastSwapMs_ < 300) continue;
      jobject cand = env->CallObjectMethod(inv, id.invSlot, slot);
      if (!cand || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (cand) env->DeleteLocalRef(cand);
        continue;
      }
      jobject cItem = env->CallObjectMethod(cand, id.stGetItem);
      bool match = false;
      if (!env->ExceptionCheck() && cItem) {
        jint cMeta = env->CallIntMethod(cand, id.stGetMeta);
        jint cSize = env->GetIntField(cand, id.stSize);
        jclass cCls =
            !env->ExceptionCheck() ? env->GetObjectClass(cItem) : nullptr;
        if (!env->ExceptionCheck() && cSize > 0 && cMeta == trackedMeta_ &&
            cCls && env->IsSameObject(cCls, trackedCls_)) {
          match = true;
        } else if (env->ExceptionCheck()) {
          env->ExceptionClear();
        }
        if (cCls) env->DeleteLocalRef(cCls);
      } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
      }
      if (cItem) env->DeleteLocalRef(cItem);
      env->DeleteLocalRef(cand);
      if (match) {
        env->SetIntField(inv, id.invCurrent, slot);
        if (id.mcController && id.ctlSyncItem) {
          jobject ctl = sdk::Mc::Obj(env, f.mc, id.mcController);
          if (ctl) {
            env->CallVoidMethod(ctl, id.ctlSyncItem);
            env->DeleteLocalRef(ctl);
          }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        lastSwapSlot_ = slot;
        lastSwapMs_ = now;
        break;
      }
    }
    env->DeleteLocalRef(inv);
    if (env->ExceptionCheck()) env->ExceptionClear();
  } while (false);
  mc.ReleaseFrame(f);
}

void AutoSwap::OnDisable() {
  if (trackedCls_) {
    JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
    if (env) env->DeleteGlobalRef(trackedCls_);
    trackedCls_ = nullptr;
  }
  ResetTrack();
}

}  // namespace modules
}  // namespace raven
