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

#include "modules/TPAura.h"
#include <random>
#include "modules/CombatUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"
#include "util/Logger.h"

namespace raven {
namespace modules {

namespace {
thread_local std::mt19937 t_rng(std::random_device{}());
}

TPAura::TPAura() : core::Module("TPAura", core::Category::Combat, 0) {
  RegisterSlider(&range);
  RegisterBool(&weaponOnly);
}

void TPAura::RandomizeOffset() {
  std::uniform_int_distribution<int> xz(-15, 15);
  std::uniform_int_distribution<int> y(10, 15);
  ox_ = xz(t_rng) / 10.0;
  oy_ = y(t_rng) / 10.0;
  oz_ = xz(t_rng) / 10.0;
}

void TPAura::OnEnable() {
  if (range.value == 0.0) {
    util::Logger::Warn("TPAura: range is 0, disabling");
    SetEnabled(false);
    return;
  }
  RandomizeOffset();
}

void TPAura::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player || !f.world || range.value == 0.0) break;
    if (weaponOnly.value && !combat::HoldingWeapon(env, f.player)) break;

    // Re-randomize offset on our own fresh hurt (mirrors onLivingUpdate).
    jint hurt = sdk::Mc::In(env, f.player, id.lHurtTime);
    jint mx = sdk::Mc::In(env, f.player, id.lMaxHurt);
    if (hurt > 0 && hurt == mx) RandomizeOffset();

    double rangeSq = range.value * range.value;
    double sx = sdk::Mc::D(env, f.player, id.ePosX);
    double sy = sdk::Mc::D(env, f.player, id.ePosY);
    double sz = sdk::Mc::D(env, f.player, id.ePosZ);

    bool attacked = false;
    mc.EachPlayer(
        f,
        [&](jobject ent) {
          if (attacked) return;
          if (sdk::Mc::Same(env, ent, f.player)) return;
          float hp = env->CallFloatMethod(ent, id.lGetHealth);
          if (env->ExceptionCheck() || hp <= 0.f) {
            env->ExceptionClear();
            return;
          }
          double px = sdk::Mc::D(env, ent, id.ePosX);
          double py = sdk::Mc::D(env, ent, id.ePosY);
          double pz = sdk::Mc::D(env, ent, id.ePosZ);
          double dx = px - sx, dy = py - sy, dz = pz - sz;
          if (dx * dx + dy * dy + dz * dz > rangeSq) return;
          env->CallVoidMethod(f.player, id.setPosM, px + ox_, py + oy_, pz + oz_);
          if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return;
          }
          jobject ctl = sdk::Mc::Obj(env, f.mc, id.mcController);
          if (ctl && !env->ExceptionCheck()) {
            env->CallVoidMethod(ctl, id.ctlAttack, f.player, ent);
          }
          if (ctl) env->DeleteLocalRef(ctl);
          if (env->ExceptionCheck()) env->ExceptionClear();
          attacked = true;
        });
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
