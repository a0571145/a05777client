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

#include "modules/HitBox.h"
#include <windows.h>
#include "hooks/Hooks.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"
#include "sdk/Raycast.h"

namespace raven {
namespace modules {

namespace {
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
}  // namespace

HitBox::HitBox() : core::Module("Hitboxes", core::Category::Combat, 0) {
  RegisterSlider(&multiplier);
  RegisterBool(&playersOnly);
  RegisterBool(&weaponOnly);
}

void HitBox::OnUpdate() {
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
    // Friends check needs Relationships (absent); spectator/world/focus gates kept.
    if (multiplier.value == 1.0) break;
    if (!f.player || !f.world || f.screen || !f.hasFocus) break;
    jboolean spec = env->CallBooleanMethod(f.player, id.pIsSpectator);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      break;
    }
    if (spec) break;
    if (weaponOnly.value && !HoldingWeapon(env, f.player)) break;

    jobject ctl = sdk::Mc::Obj(env, f.mc, id.mcController);
    bool ext = ctl && env->CallBooleanMethod(ctl, id.ctlExtReach);
    if (ctl) env->DeleteLocalRef(ctl);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      break;
    }
    double reach = ext ? 6.0 : 3.0;

    sdk::RayHit hit;
    if (!sdk::RaycastEntity(env, f, reach, multiplier.value, playersOnly.value, hit)) {
      break;
    }
    sdk::ApplyPointed(env, f, hit);
    sdk::FreeRayHit(env, hit);
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
