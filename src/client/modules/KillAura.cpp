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

#include "modules/KillAura.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <string>
#include <vector>
#include <windows.h>
#include "core/Client.h"
#include "modules/AutoBlock.h"
#include "hooks/Hooks.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"
#include "sdk/Raycast.h"

namespace raven {
namespace modules {

namespace {
thread_local std::mt19937 t_rng(std::random_device{}());
constexpr double kPi = 3.14159265358979323846;
constexpr float kEyeHeight = 1.62f;

long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

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

// Vanilla EntityLookHelper yaw/pitch toward a point.
void AnglesTo(double sx, double sy, double sz, double tx, double ty, double tz,
              double& yaw, double& pitch) {
  double dx = tx - sx, dy = ty - sy, dz = tz - sz;
  double dxz = std::sqrt(dx * dx + dz * dz);
  yaw = std::atan2(dz, dx) * 180.0 / kPi - 90.0;
  pitch = -(std::atan2(dy, dxz < 1e-9 ? 1e-9 : dxz) * 180.0 / kPi);
}

double WrapDiff(double target, double cur) {
  double d = std::fmod(target - cur, 360.0);
  if (d > 180.0) d -= 360.0;
  if (d < -180.0) d += 360.0;
  return d;
}

// Forward vector from yaw/pitch (same basis as ESP projection).
void LookDir(double yaw, double pitch, double& fx, double& fy, double& fz) {
  constexpr double kDeg = 0.017453292519943295;
  double f = std::cos(-yaw * kDeg - kPi);
  double f1 = std::sin(-yaw * kDeg - kPi);
  double f2 = -std::cos(-pitch * kDeg);
  double f3 = std::sin(-pitch * kDeg);
  fx = f1 * f2;
  fy = f3;
  fz = f * f2;
}

double AngleBetween(double ax, double ay, double az, double bx, double by, double bz) {
  double la = std::sqrt(ax * ax + ay * ay + az * az);
  double lb = std::sqrt(bx * bx + by * by + bz * bz);
  if (la < 1e-9 || lb < 1e-9) return 180.0;
  double cosA = (ax * bx + ay * by + az * bz) / (la * lb);
  cosA = std::clamp(cosA, -1.0, 1.0);
  return std::acos(cosA) * 180.0 / kPi;
}

std::string DisplayText(JNIEnv* env, jobject player) {
  auto& id = sdk::Mc::Instance().Ids();
  jobject comp = env->CallObjectMethod(player, id.eGetName);
  std::string out;
  if (comp && !env->ExceptionCheck()) {
    jstring s = static_cast<jstring>(env->CallObjectMethod(comp, id.ccText));
    if (s && !env->ExceptionCheck()) {
      const char* c = env->GetStringUTFChars(s, nullptr);
      if (c) out = c;
      if (c) env->ReleaseStringUTFChars(s, c);
      env->DeleteLocalRef(s);
    } else {
      env->ExceptionClear();
    }
    env->DeleteLocalRef(comp);
  } else {
    env->ExceptionClear();
  }
  return out;
}

bool IsTeammateText(const std::string& self, const std::string& other) {
  // Hypixel-style: equal 2-char color prefix means same team.
  if (self.size() < 2 || other.size() < 2) return false;
  return self.compare(0, 2, other, 0, 2) == 0;
}

bool IsMonster(JNIEnv* env, jobject ent) {
  jclass objCls = env->FindClass("java/lang/Object");
  jclass clsCls = env->FindClass("java/lang/Class");
  bool monster = false;
  if (objCls && clsCls && !env->ExceptionCheck()) {
    jmethodID getClass = env->GetMethodID(objCls, "getClass", "()Ljava/lang/Class;");
    jmethodID getName = env->GetMethodID(clsCls, "getName", "()Ljava/lang/String;");
    if (getClass && getName) {
      jobject lc = env->CallObjectMethod(ent, getClass);
      jstring ln = (lc && !env->ExceptionCheck())
                       ? static_cast<jstring>(env->CallObjectMethod(lc, getName))
                       : nullptr;
      if (ln && !env->ExceptionCheck()) {
        const char* c = env->GetStringUTFChars(ln, nullptr);
        if (c) {
          monster = std::string(c).rfind("net.minecraft.entity.monster.", 0) == 0;
          env->ReleaseStringUTFChars(ln, c);
        }
        env->DeleteLocalRef(ln);
      } else {
        env->ExceptionClear();
      }
      if (lc) env->DeleteLocalRef(lc);
    }
  } else {
    env->ExceptionClear();
  }
  if (objCls) env->DeleteLocalRef(objCls);
  if (clsCls) env->DeleteLocalRef(clsCls);
  return monster;
}

struct Cand {
  jobject g = nullptr;  // global ref
  double dist = 0.0;
  float hp = 20.f;
  int hurt = 0;
  double ang = 0.0;
};
}  // namespace

KillAura::KillAura() : core::Module("Kill Aura", core::Category::Combat, 0) {
  RegisterSlider(&targetCPS);
  RegisterSlider(&fov);
  RegisterSlider(&attackRange);
  RegisterSlider(&swingRange);
  RegisterSlider(&aimRange);
  RegisterMode(&rotationMode);
  RegisterMode(&sortMode);
  RegisterSlider(&speed);
  RegisterBool(&targetInvis);
  RegisterBool(&attackMobs);
  RegisterBool(&throughWalls);
  RegisterBool(&disableInInventory);
  RegisterBool(&disableWhileMining);
  RegisterBool(&ignoreTeammates);
  RegisterBool(&notUsingItem);
  RegisterBool(&requireMouseDown);
  RegisterBool(&weaponOnly);
}

void KillAura::OnEnable() { nextClickMs_ = 0; }
void KillAura::OnDisable() { nextClickMs_ = 0; }

void KillAura::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();
  core::Module* autoblockModule =
      core::Client::Instance().Modules().Find("Auto Block");
  auto* autoblock = dynamic_cast<AutoBlock*>(autoblockModule);
  std::vector<Cand> cands;

  do {
    if (!f.player || !f.world || !f.settings) break;
    if (f.screen && disableInInventory.value) break;
    if (!f.hasFocus) break;
    jboolean usingItem = env->CallBooleanMethod(f.player, id.pIsUsingItem);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      break;
    }
    const bool autoblockAttackWindow =
        autoblock && autoblock->IsEnabled() &&
        autoblock->CanAttackWhileUsingItem();
    if (notUsingItem.value && usingItem && !autoblockAttackWindow) break;
    if (weaponOnly.value && !HoldingWeapon(env, f.player)) break;
    if (requireMouseDown.value && !(LmbDown() && ForegroundIsGame())) break;
    if (disableWhileMining.value && LmbDown() && f.mouseOver) {
      // Mining approx: crosshair on a solid block while holding LMB.
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

    double sx = sdk::Mc::D(env, f.player, id.ePosX);
    double sy = sdk::Mc::D(env, f.player, id.ePosY);
    double sz = sdk::Mc::D(env, f.player, id.ePosZ);
    double ex = sx, ey = sy + kEyeHeight, ez = sz;
    float yaw = sdk::Mc::Fl(env, f.player, id.eYaw);
    float pitch = sdk::Mc::Fl(env, f.player, id.ePitch);
    double lfx, lfy, lfz;
    LookDir(yaw, pitch, lfx, lfy, lfz);
    double maxR = std::max(attackRange.value, aimRange.value);
    std::string selfText =
        ignoreTeammates.value ? DisplayText(env, f.player) : std::string();

    mc.EachPlayer(
        f,
        [&](jobject ent) {
          bool isPlayer = env->IsInstanceOf(ent, id.entityPlayer);
          if (!isPlayer && !attackMobs.value) return;
          if (sdk::Mc::Same(env, ent, f.player)) return;
          if (!isPlayer && !IsMonster(env, ent)) return;
          float hp = env->CallFloatMethod(ent, id.lGetHealth);
          if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return;
          }
          if (hp <= 0.f) return;
          jboolean invis = env->CallBooleanMethod(ent, id.eInvisible);
          if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return;
          }
          if (invis && !targetInvis.value) return;
          if (isPlayer && ignoreTeammates.value && !selfText.empty()) {
            std::string t = DisplayText(env, ent);
            if (!t.empty() && IsTeammateText(selfText, t)) return;
          }
          double px = sdk::Mc::D(env, ent, id.ePosX);
          double py = sdk::Mc::D(env, ent, id.ePosY);
          double pz = sdk::Mc::D(env, ent, id.ePosZ);
          float h = sdk::Mc::Fl(env, ent, id.eHeight);
          double cx = px, cy = py + h * 0.55, cz = pz;
          double dx = cx - ex, dy = cy - ey, dz = cz - ez;
          double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
          if (dist > maxR) return;
          double ang = AngleBetween(lfx, lfy, lfz, dx, dy, dz);
          if (fov.value < 360.0 && ang > fov.value * 0.5) return;
          if (!throughWalls.value && !sdk::LineOfSight(env, f, cx, cy, cz)) return;
          int hurt = sdk::Mc::In(env, ent, id.lHurtTime);
          Cand c;
          c.g = env->NewGlobalRef(ent);
          if (!c.g) return;
          c.dist = dist;
          c.hp = hp;
          c.hurt = hurt;
          c.ang = ang;
          cands.push_back(c);
        },
        false /* include mobs */);

    if (cands.empty()) break;
    int sort = sortMode.index;
    std::sort(cands.begin(), cands.end(), [sort](const Cand& a, const Cand& b) {
      switch (sort) {
        case 1:
          if (a.hp != b.hp) return a.hp < b.hp;
          break;
        case 2:
          if (a.hurt != b.hurt) return a.hurt > b.hurt;
          break;
        case 3:
          if (a.ang != b.ang) return a.ang < b.ang;
          break;
        default:
          break;
      }
      return a.dist < b.dist;
    });
    Cand& best = cands.front();

    // Rotations (Silent behaves as Lock view: no pre-motion hook exists here).
    if (best.dist <= aimRange.value && rotationMode.index != 2) {
      double px = sdk::Mc::D(env, best.g, id.ePosX);
      double py = sdk::Mc::D(env, best.g, id.ePosY);
      float h = sdk::Mc::Fl(env, best.g, id.eHeight);
      double pz = sdk::Mc::D(env, best.g, id.ePosZ);
      double tyaw = 0, tpitch = 0;
      AnglesTo(ex, ey, ez, px, py + h * 0.55, pz, tyaw, tpitch);
      double k = std::min(1.0, speed.value / 10.0 * 0.35);
      float nyaw = static_cast<float>(yaw + WrapDiff(tyaw, yaw) * k);
      float npitch = static_cast<float>(pitch + WrapDiff(tpitch, pitch) * k);
      npitch = std::clamp(npitch, -90.f, 90.f);
      env->SetFloatField(f.player, id.eYaw, nyaw);
      env->SetFloatField(f.player, id.ePitch, npitch);
    }

    // CPS attack.
    if (best.dist <= attackRange.value) {
      long long now = NowMs();
      if (nextClickMs_ == 0) nextClickMs_ = now + 50;
      int target = std::max(1, static_cast<int>(targetCPS.value));
      int base = 1000 / target;
      int clicks = 0;
      while (nextClickMs_ <= now) {
        clicks++;
        int jitter = base * 80 / 100 +
                     static_cast<int>(t_rng() % (static_cast<unsigned>(base * 40 / 100) + 1));
        nextClickMs_ += std::max(25, jitter);
      }
      if (clicks > 0) {
        if (autoblock && autoblock->IsEnabled()) {
          autoblock->PrepareAttack(env, f);
        }
        jobject ctl = sdk::Mc::Obj(env, f.mc, id.mcController);
        if (best.dist <= swingRange.value) {
          env->CallVoidMethod(f.player, id.pSwing);
        }
        bool didSubmitAttack = false;
        if (ctl && !env->ExceptionCheck()) {
          for (int i = 0; i < clicks; i++) {
            env->CallVoidMethod(ctl, id.ctlAttack, f.player, best.g);
            if (env->ExceptionCheck()) break;
            didSubmitAttack = true;
          }
        }
        if (ctl) env->DeleteLocalRef(ctl);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (didSubmitAttack && autoblock && autoblock->IsEnabled()) {
          autoblock->NotifyAttack(true);
        }
      }
    }
  } while (false);

  for (auto& c : cands) {
    if (c.g) env->DeleteGlobalRef(c.g);
  }
  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
