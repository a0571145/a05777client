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

#include "modules/Reach.h"
#include <random>
#include <windows.h>
#include "core/Client.h"
#include "hooks/Hooks.h"
#include "modules/HitBox.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"
#include "sdk/Raycast.h"

namespace raven {
namespace modules {

namespace {
thread_local std::mt19937 t_rng(std::random_device{}());

bool LmbDown() { return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0; }

bool ForegroundIsGame() {
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

double HitBoxMult() {
  // Mirrors Java's static HitBox.getExpand (applies even when HitBox is off).
  core::Module* m = core::Client::Instance().Modules().Find("Hitboxes");
  auto* hb = dynamic_cast<HitBox*>(m);
  return hb ? hb->Expand() : 1.0;
}
}  // namespace

Reach::Reach() : core::Module("Reach", core::Category::Combat, 0) {
  RegisterSlider(&minReach);
  RegisterSlider(&maxReach);
  RegisterBool(&weaponOnly);
  RegisterBool(&movingOnly);
  RegisterBool(&sprintOnly);
  RegisterBool(&throughBlocks);
}

void Reach::OnUpdate() {
  bool lmb = LmbDown() && ForegroundIsGame();
  bool edge = lmb && !prevLmb_;
  prevLmb_ = lmb;
  if (!edge) return;

  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player || !f.world || f.screen || !f.hasFocus) break;
    if (weaponOnly.value && !HoldingWeapon(env, f.player)) break;
    if (movingOnly.value) {
      jobject mi = sdk::Mc::Obj(env, f.player, id.spMoveInput);
      float mf = sdk::Mc::Fl(env, mi, id.inForward);
      float ms = sdk::Mc::Fl(env, mi, id.inStrafe);
      if (mi) env->DeleteLocalRef(mi);
      if (mf == 0.f && ms == 0.f) break;
    }
    if (sprintOnly.value) {
      jboolean sp = env->CallBooleanMethod(f.player, id.eSprinting);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        break;
      }
      if (!sp) break;
    }
    if (!throughBlocks.value && f.mouseOver) {
      jobject pos = env->CallObjectMethod(f.mouseOver, id.mopBlockPos);
      bool solid = false;
      if (pos && f.world) {
        jobject state = env->CallObjectMethod(f.world, id.wBlockState, pos);
        jobject block = (state && !env->ExceptionCheck())
                            ? env->CallObjectMethod(state, id.ibsGetBlock)
                            : nullptr;
        solid = block && !env->IsSameObject(block, id.airBlock) &&
                !env->IsInstanceOf(block, id.liquid);
        if (state) env->DeleteLocalRef(state);
        if (block) env->DeleteLocalRef(block);
      }
      if (pos) env->DeleteLocalRef(pos);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (solid) break;
    }

    double lo = std::min(minReach.value, maxReach.value);
    double hi = std::max(minReach.value, maxReach.value);
    double reach = lo + std::uniform_real_distribution<double>(0.0, 1.0)(t_rng) * (hi - lo);

    sdk::RayHit hit;
    if (!sdk::RaycastEntity(env, f, reach, HitBoxMult(), false, hit)) break;
    sdk::ApplyPointed(env, f, hit);
    sdk::FreeRayHit(env, hit);
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
