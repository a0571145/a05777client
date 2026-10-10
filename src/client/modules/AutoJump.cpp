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

#include "modules/AutoJump.h"
#include "modules/MoveUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

AutoJump::AutoJump() : core::Module("Auto Jump", core::Category::Player, 0) {}

void AutoJump::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();
  do {
    if (!f.player || f.screen || !id.jumpM) break;
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
    // Java: not flying + on ground + moving into a wall -> jump now.
    // jump() is only fired while grounded, so holding it cannot fly.
    bool onGround = sdk::Mc::Bo(env, f.player, id.eOnGround);
    bool collided = sdk::Mc::Bo(env, f.player, id.collidedH);
    if (onGround && collided && MovingInput(f)) {
      env->CallVoidMethod(f.player, id.jumpM);
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
  } while (false);
  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
