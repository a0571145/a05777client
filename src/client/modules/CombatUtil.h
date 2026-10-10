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

#pragma once
#include <algorithm>
#include <cmath>
#include <jni.h>
#include <string>
#include <windows.h>
#include "hooks/Hooks.h"
#include "sdk/Mc.h"

// Shared combat helpers (input state, weapon checks, aim math, teammate text).
// KillAura keeps its own copies (untouched); new modules use these.
namespace raven {
namespace combat {

constexpr double kPi = 3.14159265358979323846;

inline bool LmbDown() { return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0; }
inline bool RmbDown() { return (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0; }

inline bool ForegroundIsGame() {
  // Compare against the real game window handle (title matching breaks on
  // custom clients like FPSMaster whose title lacks "Minecraft").
  HWND mcw = hooks::Hooks::Instance().Window();
  return mcw && GetForegroundWindow() == mcw;
}

inline bool HoldingWeapon(JNIEnv* env, jobject player) {
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
inline void AnglesTo(double sx, double sy, double sz, double tx, double ty, double tz,
                     double& yaw, double& pitch) {
  double dx = tx - sx, dy = ty - sy, dz = tz - sz;
  double dxz = std::sqrt(dx * dx + dz * dz);
  yaw = std::atan2(dz, dx) * 180.0 / kPi - 90.0;
  pitch = -(std::atan2(dy, dxz < 1e-9 ? 1e-9 : dxz) * 180.0 / kPi);
}

inline double WrapDiff(double target, double cur) {
  double d = std::fmod(target - cur, 360.0);
  if (d > 180.0) d -= 360.0;
  if (d < -180.0) d += 360.0;
  return d;
}

// Forward vector from yaw/pitch (same basis as ESP projection).
inline void LookDir(double yaw, double pitch, double& fx, double& fy, double& fz) {
  constexpr double kDeg = 0.017453292519943295;
  double f = std::cos(-yaw * kDeg - kPi);
  double f1 = std::sin(-yaw * kDeg - kPi);
  double f2 = -std::cos(-pitch * kDeg);
  double f3 = std::sin(-pitch * kDeg);
  fx = f1 * f2;
  fy = f3;
  fz = f * f2;
}

inline double AngleBetween(double ax, double ay, double az, double bx, double by,
                           double bz) {
  double la = std::sqrt(ax * ax + ay * ay + az * az);
  double lb = std::sqrt(bx * bx + by * by + bz * bz);
  if (la < 1e-9 || lb < 1e-9) return 180.0;
  double cosA = (ax * bx + ay * by + az * bz) / (la * lb);
  cosA = std::clamp(cosA, -1.0, 1.0);
  return std::acos(cosA) * 180.0 / kPi;
}

inline std::string DisplayText(JNIEnv* env, jobject player) {
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

// Hypixel-style: equal 2-char color prefix means same team.
inline bool IsTeammateText(const std::string& self, const std::string& other) {
  if (self.size() < 2 || other.size() < 2) return false;
  return self.compare(0, 2, other, 0, 2) == 0;
}

inline bool IsMonster(JNIEnv* env, jobject ent) {
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

}  // namespace combat
}  // namespace raven
