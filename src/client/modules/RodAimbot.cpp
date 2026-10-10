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

#include "modules/RodAimbot.h"
#include <algorithm>
#include <chrono>
#include <jni.h>
#include <string>
#include <vector>
#include "modules/CombatUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

namespace {
long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
}  // namespace

RodAimbot::RodAimbot() : core::Module("RodAimbot", core::Category::Combat, 0) {
  RegisterSlider(&fov);
  RegisterSlider(&predicted);
  RegisterSlider(&distance);
  RegisterBool(&aimInvis);
  RegisterBool(&ignoreTeammates);
}

void RodAimbot::OnDisable() {
  rotating_ = false;
  prevRmb_ = false;
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (env) ForgetTarget(env);
}

void RodAimbot::ForgetTarget(void* envPtr) {
  if (target_) {
    static_cast<JNIEnv*>(envPtr)->DeleteWeakGlobalRef(
        static_cast<jweak>(target_));
    target_ = nullptr;
  }
}

static bool HoldingRod(JNIEnv* env, jobject player) {
  auto& id = sdk::Mc::Instance().Ids();
  jobject held = env->CallObjectMethod(player, id.lGetHeld);
  if (!held || env->ExceptionCheck()) {
    env->ExceptionClear();
    return false;
  }
  jobject item = env->CallObjectMethod(held, id.stGetItem);
  bool rod = item && env->IsInstanceOf(item, id.rod);
  if (item) env->DeleteLocalRef(item);
  env->DeleteLocalRef(held);
  if (env->ExceptionCheck()) env->ExceptionClear();
  return rod;
}

void RodAimbot::OnUpdate() {
  bool rmb = combat::RmbDown() && combat::ForegroundIsGame();
  bool edge = rmb && !prevRmb_;
  prevRmb_ = rmb;

  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player || !f.world || f.screen || !f.hasFocus) {
      rotating_ = false;
      ForgetTarget(env);
      break;
    }
    if (!HoldingRod(env, f.player)) {
      rotating_ = false;
      ForgetTarget(env);
      break;
    }
    // A live hook already out: stop re-aiming a new target.
    jobject hook = sdk::Mc::Obj(env, f.player, id.fishHook);
    bool hooked = hook != nullptr;
    if (hook) env->DeleteLocalRef(hook);
    if (hooked) {
      rotating_ = false;
      ForgetTarget(env);
      break;
    }

    if (edge && !rotating_) {
      // Target select (mirrors getEntity, minus friends/AntiBot).
      double sx = sdk::Mc::D(env, f.player, id.ePosX);
      double sy = sdk::Mc::D(env, f.player, id.ePosY);
      double sz = sdk::Mc::D(env, f.player, id.ePosZ);
      float yaw = sdk::Mc::Fl(env, f.player, id.eYaw);
      float pitch = sdk::Mc::Fl(env, f.player, id.ePitch);
      double lfx, lfy, lfz;
      combat::LookDir(yaw, pitch, lfx, lfy, lfz);
      std::string selfText =
          ignoreTeammates.value ? combat::DisplayText(env, f.player) : std::string();
      jobject best = nullptr;
      double bestD2 = distance.value * distance.value;
      // Iterate players; keep nearest in FOV.
      struct Pick {
        jobject g = nullptr;
        double d2 = 0;
      };
      std::vector<Pick> found;
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
            double dx = px - sx, dy = py - sy, dz = pz - sz;
            double d2 = dx * dx + dy * dy + dz * dz;
            if (d2 > bestD2) return;
            double ang = combat::AngleBetween(lfx, lfy, lfz, dx, dy, dz);
            if (fov.value < 360.0 && ang > fov.value * 0.5) return;
            Pick p;
            p.g = env->NewGlobalRef(ent);
            if (!p.g) return;
            p.d2 = d2;
            found.push_back(p);
          });
      for (auto& p : found) {
        if (p.d2 < bestD2) {
          bestD2 = p.d2;
          if (best) env->DeleteGlobalRef(best);
          best = p.g;
        } else {
          env->DeleteGlobalRef(p.g);
        }
      }
      if (best) {
        ForgetTarget(env);
        target_ = env->NewWeakGlobalRef(best);
        env->DeleteGlobalRef(best);
        rotating_ = target_ != nullptr;
        stopAtMs_ = NowMs() + 1500;
      }
    }

    if (!rotating_ || !target_ || NowMs() > stopAtMs_) {
      if (!rotating_) ForgetTarget(env);
      if (NowMs() > stopAtMs_) {
        rotating_ = false;
        ForgetTarget(env);
      }
      break;
    }
    jobject target = env->NewLocalRef(static_cast<jobject>(target_));
    if (!target) {
      rotating_ = false;
      ForgetTarget(env);
      break;
    }
    float hp = env->CallFloatMethod(target, id.lGetHealth);
    if (env->ExceptionCheck() || hp <= 0.f) {
      env->ExceptionClear();
      env->DeleteLocalRef(target);
      rotating_ = false;
      ForgetTarget(env);
      break;
    }
    double px = sdk::Mc::D(env, target, id.ePosX);
    double py = sdk::Mc::D(env, target, id.ePosY);
    double pz = sdk::Mc::D(env, target, id.ePosZ);
    float h = sdk::Mc::Fl(env, target, id.eHeight);
    double tmx = sdk::Mc::D(env, target, id.eMotionX);
    double tmy = sdk::Mc::D(env, target, id.eMotionY);
    double tmz = sdk::Mc::D(env, target, id.eMotionZ);
    env->DeleteLocalRef(target);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      break;
    }
    // Predicted aim point (target motion * ticks, like getRotationsPredicated).
    double pt = predicted.value;
    double sx = sdk::Mc::D(env, f.player, id.ePosX);
    double sy = sdk::Mc::D(env, f.player, id.ePosY);
    double sz = sdk::Mc::D(env, f.player, id.ePosZ);
    double tyaw = 0, tpitch = 0;
    combat::AnglesTo(sx, sy + 1.62, sz, px + tmx * pt, py + h * 0.5 + tmy * pt,
                     pz + tmz * pt, tyaw, tpitch);
    env->SetFloatField(f.player, id.eYaw, static_cast<float>(tyaw));
    env->SetFloatField(f.player, id.ePitch,
                       std::clamp(static_cast<float>(tpitch), -90.f, 90.f));
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
