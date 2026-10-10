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

#include "modules/Piercing.h"
#include <string>
#include "modules/CombatUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"
#include "sdk/Raycast.h"

namespace raven {
namespace modules {

Piercing::Piercing() : core::Module("Piercing", core::Category::Combat, 0) {
  RegisterBool(&ignoreBlocks);
  RegisterBool(&ignoreNonPlayer);
  RegisterBool(&ignoreTeammates);
  RegisterBool(&weaponOnly);
  RegisterBool(&insideOnly);
}

void Piercing::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player || !f.world || f.screen || !f.hasFocus) break;
    if (weaponOnly.value && !combat::HoldingWeapon(env, f.player)) break;

    // shouldOverrideMouseOver port: skip when the crosshair sits on a block
    // unless Ignore blocks is on.
    if (!ignoreBlocks.value && f.mouseOver) {
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

    jobject ctl = sdk::Mc::Obj(env, f.mc, id.mcController);
    float reach = 4.5f;
    if (ctl && !env->ExceptionCheck()) {
      reach = env->CallFloatMethod(ctl, id.reachDist);
    }
    if (ctl) env->DeleteLocalRef(ctl);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      break;
    }

    sdk::RayHit hit;
    if (!sdk::RaycastEntity(env, f, reach, 1.0, ignoreNonPlayer.value, hit)) break;
    bool drop = false;
    do {
      if (ignoreTeammates.value && env->IsInstanceOf(hit.entity, id.entityPlayer)) {
        std::string selfText = combat::DisplayText(env, f.player);
        std::string t = combat::DisplayText(env, hit.entity);
        if (!selfText.empty() && !t.empty() && combat::IsTeammateText(selfText, t)) {
          drop = true;
          break;
        }
      }
      if (insideOnly.value && hit.dist > 3.0) drop = true;
    } while (false);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      drop = true;
    }
    if (!drop) sdk::ApplyPointed(env, f, hit);
    sdk::FreeRayHit(env, hit);
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
