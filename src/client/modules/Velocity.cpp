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

#include "modules/Velocity.h"
#include <random>
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

namespace {
thread_local std::mt19937 t_rng(std::random_device{}());
}

Velocity::Velocity() : core::Module("Velocity", core::Category::Combat, 0) {
  RegisterSlider(&horizontal);
  RegisterSlider(&vertical);
  RegisterSlider(&chance);
  RegisterBool(&onlyTargeting);
  RegisterBool(&disableS);
}

void Velocity::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) return;
  if (!mc.EnsureExtra(env)) return;  // movementInput for Disable-S
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player) break;
    // Same trigger as Java: fresh knockback tick (hurtTime == maxHurtTime).
    jint hurt = sdk::Mc::In(env, f.player, id.lHurtTime);
    jint maxHurt = sdk::Mc::In(env, f.player, id.lMaxHurt);
    if (hurt <= 0 || hurt != maxHurt) break;

    if (onlyTargeting.value) {
      if (!f.mouseOver) break;
      jobject target = env->GetObjectField(f.mouseOver, id.mopEntity);
      bool has = target != nullptr;
      if (target) env->DeleteLocalRef(target);
      if (!has) break;
    }

    if (disableS.value) {
      jobject mi = sdk::Mc::Obj(env, f.player, id.spMoveInput);
      float fwd = sdk::Mc::Fl(env, mi, id.inForward);
      if (mi) env->DeleteLocalRef(mi);
      if (fwd < -1e-4f) break;  // pulling back (S) without keycode lookups
    }

    double ch = chance.value;
    if (ch <= 0.0) break;
    if (ch < 100.0) {
      double roll = std::uniform_real_distribution<double>(0.0, 1.0)(t_rng);
      if (roll >= ch / 100.0) break;
    }

    double h = horizontal.value;
    if (h != 100.0) {
      sdk::Mc::SetD(env, f.player, id.eMotionX,
                    sdk::Mc::D(env, f.player, id.eMotionX) * h / 100.0);
      sdk::Mc::SetD(env, f.player, id.eMotionZ,
                    sdk::Mc::D(env, f.player, id.eMotionZ) * h / 100.0);
    }
    double v = vertical.value;
    if (v != 100.0) {
      sdk::Mc::SetD(env, f.player, id.eMotionY,
                    sdk::Mc::D(env, f.player, id.eMotionY) * v / 100.0);
    }
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
