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

#include "modules/AimAssist.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <string>
#include <vector>
#include "modules/CombatUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"
#include "sdk/Raycast.h"

namespace raven {
namespace modules {

namespace {
thread_local std::mt19937 t_rng(std::random_device{}());
constexpr float kEyeHeight = 1.62f;

long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

struct Cand {
  jobject g = nullptr;
  double dist = 0.0;
  float hp = 20.f;
  int hurt = 0;
  double ang = 0.0;
};
}  // namespace

AimAssist::AimAssist() : core::Module("Aim Assist", core::Category::Combat, 0) {
  RegisterMode(&mode);
  RegisterSlider(&speed);
  RegisterSlider(&multiH);
  RegisterSlider(&multiV);
  RegisterSlider(&randomization);
  RegisterSlider(&fov);
  RegisterSlider(&range);
  RegisterMode(&sortMode);
  RegisterBool(&aimInvis);
  RegisterBool(&requireMouse);
  RegisterBool(&ignoreTeammates);
  RegisterBool(&ignoreWalls);
  RegisterBool(&ignoreEntities);
  RegisterBool(&stopBreaking);
  RegisterSlider(&hoverDelay);
  RegisterBool(&weaponOnly);
}

void AimAssist::OnDisable() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (env) ForgetTarget(env);
  lastMs_ = 0;
}

void AimAssist::ForgetTarget(void* envPtr) {
  if (lastTarget_) {
    static_cast<JNIEnv*>(envPtr)->DeleteWeakGlobalRef(
        static_cast<jweak>(lastTarget_));
    lastTarget_ = nullptr;
  }
}

void AimAssist::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();
  std::vector<Cand> cands;

  do {
    if (!f.player || !f.world) break;
    if (f.screen || !f.hasFocus) break;
    if (requireMouse.value && !(combat::LmbDown() && combat::ForegroundIsGame())) break;
    if (weaponOnly.value && !combat::HoldingWeapon(env, f.player)) break;

    // Mining pause (stopWhenBreaking + hover delay).
    if (stopBreaking.value && f.mouseOver) {
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
      static thread_local long long mineSince = 0;
      long long now = NowMs();
      if (solid) {
        if (mineSince == 0) mineSince = now;
        if (now - mineSince < static_cast<long long>(hoverDelay.value)) break;
      } else {
        mineSince = 0;
      }
    }

    double sx = sdk::Mc::D(env, f.player, id.ePosX);
    double sy = sdk::Mc::D(env, f.player, id.ePosY);
    double sz = sdk::Mc::D(env, f.player, id.ePosZ);
    double ex = sx, ey = sy + kEyeHeight, ez = sz;
    float yaw = sdk::Mc::Fl(env, f.player, id.eYaw);
    float pitch = sdk::Mc::Fl(env, f.player, id.ePitch);
    double lfx, lfy, lfz;
    combat::LookDir(yaw, pitch, lfx, lfy, lfz);
    std::string selfText =
        ignoreTeammates.value ? combat::DisplayText(env, f.player) : std::string();

    mc.EachPlayer(
        f,
        [&](jobject ent) {
          if (sdk::Mc::Same(env, ent, f.player)) return;
          float hp = env->CallFloatMethod(ent, id.lGetHealth);
          if (env->ExceptionCheck() || hp <= 0.f) {
            env->ExceptionClear();
            return;
          }
          jboolean invis = env->CallBooleanMethod(ent, id.eInvisible);
          if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return;
          }
          if (invis && !aimInvis.value) return;
          if (ignoreTeammates.value && !selfText.empty()) {
            std::string t = combat::DisplayText(env, ent);
            if (!t.empty() && combat::IsTeammateText(selfText, t)) return;
          }
          double px = sdk::Mc::D(env, ent, id.ePosX);
          double py = sdk::Mc::D(env, ent, id.ePosY);
          double pz = sdk::Mc::D(env, ent, id.ePosZ);
          float h = sdk::Mc::Fl(env, ent, id.eHeight);
          double cx = px, cy = py + h * 0.55, cz = pz;
          double dx = cx - ex, dy = cy - ey, dz = cz - ez;
          double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
          if (dist > range.value) return;
          double ang = combat::AngleBetween(lfx, lfy, lfz, dx, dy, dz);
          if (fov.value < 360.0 && ang > fov.value * 0.5) return;
          if (!ignoreWalls.value && !sdk::LineOfSight(env, f, cx, cy, cz)) return;
          if (!ignoreEntities.value) {
            sdk::RayHit rh;
            if (sdk::RaycastEntity(env, f, dist + 0.3, 1.0, false, rh)) {
              bool sameTarget = rh.entity && env->IsSameObject(rh.entity, ent);
              sdk::FreeRayHit(env, rh);
              if (!sameTarget) return;
            }
          }
          int hurt = sdk::Mc::In(env, ent, id.lHurtTime);
          Cand c;
          c.g = env->NewGlobalRef(ent);
          if (!c.g) return;
          c.dist = dist;
          c.hp = hp;
          c.hurt = hurt;
          c.ang = ang;
          cands.push_back(c);
        });

    if (cands.empty()) {
      ForgetTarget(env);
      lastMs_ = NowMs();
      break;
    }
    int sort = sortMode.index;
    std::sort(cands.begin(), cands.end(), [sort](const Cand& a, const Cand& b) {
      switch (sort) {
        case 0:
          if (a.hp != b.hp) return a.hp < b.hp;
          break;
        case 1:
          if (a.ang != b.ang) return a.ang < b.ang;
          break;
        case 2:
          if (a.hurt != b.hurt) return a.hurt > b.hurt;
          break;
        default:
          break;
      }
      return a.dist < b.dist;
    });
    Cand& best = cands.front();

    // Sticky aim point: multipoint/random offsets re-roll only when the
    // target changes; reusing them while tracking removes jitter.
    bool sameTarget =
        lastTarget_ && env->IsSameObject(best.g, static_cast<jobject>(lastTarget_));
    if (!sameTarget) {
      ForgetTarget(env);
      lastTarget_ = env->NewWeakGlobalRef(best.g);
      double bw = sdk::Mc::Fl(env, best.g, id.eWidth);
      double bh = sdk::Mc::Fl(env, best.g, id.eHeight);
      std::uniform_real_distribution<double> u(-1.0, 1.0);
      aimOX_ = u(t_rng) * (multiH.value / 100.0) * (bw * 0.5);
      aimOY_ = u(t_rng) * (multiV.value / 100.0) * (bh * 0.5);
      aimOZ_ = u(t_rng) * (multiH.value / 100.0) * (bw * 0.5);
      aimJY_ = u(t_rng) * (randomization.value / 100.0) * 1.5;
      aimJP_ = u(t_rng) * (randomization.value / 100.0) * 1.0;
    }
    double px = sdk::Mc::D(env, best.g, id.ePosX);
    double py = sdk::Mc::D(env, best.g, id.ePosY);
    double pz = sdk::Mc::D(env, best.g, id.ePosZ);
    float h = sdk::Mc::Fl(env, best.g, id.eHeight);
    double tyaw = 0, tpitch = 0;
    combat::AnglesTo(ex, ey, ez, px + aimOX_, py + h * 0.55 + aimOY_, pz + aimOZ_,
                     tyaw, tpitch);
    tyaw += aimJY_;
    tpitch += aimJP_;

    // Frame-rate independent exponential approach (smooth by construction).
    long long nowMs = NowMs();
    double dt = lastMs_ > 0 ? (nowMs - lastMs_) / 1000.0 : 0.01;
    lastMs_ = nowMs;
    if (dt < 0.001) dt = 0.001;
    if (dt > 0.1) dt = 0.1;
    double lambda = std::max(0.5, speed.value * 0.9);
    double k = 1.0 - std::exp(-lambda * dt);
    float nyaw = static_cast<float>(yaw + combat::WrapDiff(tyaw, yaw) * k);
    float npitch = static_cast<float>(pitch + combat::WrapDiff(tpitch, pitch) * k);
    npitch = std::clamp(npitch, -90.f, 90.f);
    env->SetFloatField(f.player, id.eYaw, nyaw);
    env->SetFloatField(f.player, id.ePitch, npitch);
  } while (false);

  for (auto& c : cands) {
    if (c.g) env->DeleteGlobalRef(c.g);
  }
  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
