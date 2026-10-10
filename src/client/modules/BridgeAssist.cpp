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

#include "modules/BridgeAssist.h"
#include <chrono>
#include <cmath>
#include <limits>
#include <windows.h>
#include "modules/CombatUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

BridgeAssist::BridgeAssist()
    : core::Module("Bridge Assist", core::Category::Player, 0) {
  RegisterSlider(&edgeOffset);
  RegisterSlider(&unsneakDelay);
  RegisterSlider(&sneakOnJump);
  RegisterBool(&edgeLock);
  RegisterBool(&holdingBlocks);
  RegisterBool(&lookingDown);
  RegisterBool(&notMovingForward);
}

long long BridgeAssist::NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void BridgeAssist::OnEnable() {
  sneaking_ = false;
  unsneakAtMs_ = -1;
  jumpHoldUntilMs_ = 0;
  tickAcc_ = 0;
}

void BridgeAssist::SetSneak(JNIEnv* env, sdk::Mc::Frame& f, bool pressed) {
  auto& id = sdk::Mc::Instance().Ids();
  jobject key = sdk::Mc::Obj(env, f.settings, id.sneakKey);
  if (!key) return;
  jint code = env->CallIntMethod(key, id.kGetCode);
  env->DeleteLocalRef(key);
  if (env->ExceptionCheck() || code == 0) {
    env->ExceptionClear();
    return;
  }
  env->CallStaticVoidMethod(id.keyBinding, id.kSetState, code,
                            pressed ? JNI_TRUE : JNI_FALSE);
  if (pressed) env->CallStaticVoidMethod(id.keyBinding, id.kOnTick, code);
  if (env->ExceptionCheck()) env->ExceptionClear();
}

bool BridgeAssist::PhysicalShift() {
  // Win32 physical state: the in-game pressed flag echoes our own virtual
  // sneak presses (self-lock: press once -> reads as manual forever).
  return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
}

void BridgeAssist::Press(JNIEnv* env, sdk::Mc::Frame& f) {
  SetSneak(env, f, true);
  sneaking_ = true;
  unsneakAtMs_ = -1;
}

void BridgeAssist::Release(JNIEnv* env, sdk::Mc::Frame& f) {
  SetSneak(env, f, false);
  sneaking_ = false;
  unsneakAtMs_ = -1;
  jumpHoldUntilMs_ = 0;
}

double BridgeAssist::EdgeOffset(JNIEnv* env, sdk::Mc::Frame& f) {
  auto& id = sdk::Mc::Instance().Ids();
  if (!id.eGetBox || !id.boxCtor || !id.wCollide || !id.listToArray ||
      !id.boxMinX || !id.boxMaxX) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  double off = std::numeric_limits<double>::quiet_NaN();
  jobject box = env->CallObjectMethod(f.player, id.eGetBox);
  if (!box || env->ExceptionCheck()) {
    env->ExceptionClear();
    if (box) env->DeleteLocalRef(box);
    return off;
  }
  double minX = sdk::Mc::D(env, box, id.boxMinX);
  double minY = sdk::Mc::D(env, box, id.boxMinY);
  double minZ = sdk::Mc::D(env, box, id.boxMinZ);
  double maxX = sdk::Mc::D(env, box, id.boxMaxX);
  double maxY = sdk::Mc::D(env, box, id.boxMaxY);
  double maxZ = sdk::Mc::D(env, box, id.boxMaxZ);
  env->DeleteLocalRef(box);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return off;
  }
  // Feet slab 1cm tall, like Java's groundCheck box.
  jobject slab = env->NewObject(id.aabb, id.boxCtor, minX, minY - 0.01, minZ,
                                maxX, minY, maxZ);
  if (!slab || env->ExceptionCheck()) {
    env->ExceptionClear();
    if (slab) env->DeleteLocalRef(slab);
    return off;
  }
  jobject list = env->CallObjectMethod(f.world, id.wCollide, f.player, slab);
  env->DeleteLocalRef(slab);
  if (!list || env->ExceptionCheck()) {
    env->ExceptionClear();
    if (list) env->DeleteLocalRef(list);
    return off;
  }
  jobjectArray arr =
      static_cast<jobjectArray>(env->CallObjectMethod(list, id.listToArray));
  env->DeleteLocalRef(list);
  if (!arr || env->ExceptionCheck()) {
    env->ExceptionClear();
    if (arr) env->DeleteLocalRef(arr);
    return off;
  }
  double feetX = (minX + maxX) * 0.5;
  double feetZ = (minZ + maxZ) * 0.5;
  double best = std::numeric_limits<double>::max();
  jsize n = env->GetArrayLength(arr);
  for (jsize i = 0; i < n; ++i) {
    jobject b = env->GetObjectArrayElement(arr, i);
    if (!b || env->ExceptionCheck()) {
      env->ExceptionClear();
      if (b) env->DeleteLocalRef(b);
      continue;
    }
    double bx0 = sdk::Mc::D(env, b, id.boxMinX);
    double bz0 = sdk::Mc::D(env, b, id.boxMinZ);
    double bx1 = sdk::Mc::D(env, b, id.boxMaxX);
    double bz1 = sdk::Mc::D(env, b, id.boxMaxZ);
    env->DeleteLocalRef(b);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      continue;
    }
    double cx = std::max(bx0, std::min(feetX, bx1));
    double cz = std::max(bz0, std::min(feetZ, bz1));
    double dist = std::max(std::fabs(feetX - cx), std::fabs(feetZ - cz));
    if (dist < best) best = dist;
  }
  env->DeleteLocalRef(arr);
  if (best != std::numeric_limits<double>::max()) off = best;
  return off;
}

void BridgeAssist::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  // ~50ms cadence like the Java PreUpdate tick.
  if (++tickAcc_ < 5) return;
  tickAcc_ = 0;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();
  do {
    if (!f.player || !f.world || !f.settings || f.screen) break;
    bool flying = false;
    jobject caps = sdk::Mc::Obj(env, f.player, id.pCaps);
    if (caps) {
      flying = env->GetBooleanField(caps, id.cFlying);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        flying = false;
      }
      env->DeleteLocalRef(caps);
    }
    if (flying) break;

    // Edge lock: physical SHIFT opens the door, placing (RMB) arms it.
    // Off the place key the module goes hands-off WITHOUT touching the
    // sneak key, so native SHIFT just crouches.
    bool physical = PhysicalShift();
    bool placing = combat::RmbDown();
    if (!placing) {
      if (sneaking_ && !physical) Release(env, f);  // clear stale press
      // physical held: key state stays theirs now.
      sneaking_ = false;
      unsneakAtMs_ = -1;
      jumpHoldUntilMs_ = 0;
      break;
    }
    float forward = 0.f;
    bool jumpIn = false;
    jobject mi = sdk::Mc::Obj(env, f.player, id.spMoveInput);
    if (mi) {
      forward = sdk::Mc::Fl(env, mi, id.inForward);
      jumpIn = sdk::Mc::Bo(env, mi, id.inJump);
      env->DeleteLocalRef(mi);
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (notMovingForward.value && forward > 0.f) {
      if (sneaking_) Release(env, f);
      unsneakAtMs_ = -1;
      jumpHoldUntilMs_ = 0;
      break;
    }
    if (lookingDown.value) {
      float pitch = sdk::Mc::Fl(env, f.player, id.ePitch);
      if (pitch < 70.f) {
        if (sneaking_) Release(env, f);
        unsneakAtMs_ = -1;
        jumpHoldUntilMs_ = 0;
        break;
      }
    }
    if (holdingBlocks.value && id.lGetHeld && id.stGetItem && id.itemblock) {
      jobject held = env->CallObjectMethod(f.player, id.lGetHeld);
      jobject item = (!env->ExceptionCheck() && held)
                         ? env->CallObjectMethod(held, id.stGetItem)
                         : nullptr;
      bool isBlock = item && !env->ExceptionCheck() &&
                     env->IsInstanceOf(item, id.itemblock);
      if (item) env->DeleteLocalRef(item);
      if (held) env->DeleteLocalRef(held);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (!isBlock) {
        if (sneaking_) Release(env, f);
        unsneakAtMs_ = -1;
        jumpHoldUntilMs_ = 0;
        break;
      }
    }

    long long now = NowMs();
    bool onGround = sdk::Mc::Bo(env, f.player, id.eOnGround);
    bool moving = (forward != 0.f);
    // Sneak-on-jump window (Java uses tick counts; ms here). Letting go of
    // SHIFT mid-air cancels it.
    if (physical && jumpIn && onGround && moving && sneakOnJump.value > 0.0) {
      jumpHoldUntilMs_ = now + static_cast<long long>(sneakOnJump.value);
      Press(env, f);
      break;
    }
    if (now < jumpHoldUntilMs_) {
      if (physical) {
        Press(env, f);
      } else {
        jumpHoldUntilMs_ = 0;
        if (sneaking_) Release(env, f);
      }
      break;
    }
    double offset = EdgeOffset(env, f);
    // offset = foot overhang (0 = safely inside, NaN = no ground underfoot).
    // Grounded-NaN cannot happen on solid ground: press to be safe.
    bool needSneak = std::isnan(offset) ? onGround : (offset > edgeOffset.value);
    if (physical && (needSneak || !edgeLock.value)) {
      Press(env, f);
    } else if (sneaking_) {
      // Hands off or off edge: delayed release (jump bridges keep cover).
      if (unsneakAtMs_ == -1) {
        long long wait =
            static_cast<long long>(unsneakDelay.value > 50.0
                                       ? unsneakDelay.value - 50.0
                                       : 0.0);
        unsneakAtMs_ = now + wait;
      }
      if (now >= unsneakAtMs_) {
        Release(env, f);
      } else {
        Press(env, f);
      }
    }
  } while (false);
  mc.ReleaseFrame(f);
}

void BridgeAssist::OnDisable() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  SetSneak(env, f, false);
  sneaking_ = false;
  unsneakAtMs_ = -1;
  jumpHoldUntilMs_ = 0;
  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
