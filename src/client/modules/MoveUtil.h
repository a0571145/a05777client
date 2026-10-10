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
#include <cmath>
#include <cstring>
#include <jni.h>
#include "sdk/Mc.h"

// Shared movement helpers (Raven Utils.setSpeed / potion / block probes).
namespace raven {
namespace modules {

// Horizontal motion magnitude.
inline double HSpeed(sdk::Mc::Frame& f) {
  JNIEnv* e = f.env;
  auto& id = sdk::Mc::Instance().Ids();
  double x = sdk::Mc::D(e, f.player, id.eMotionX);
  double z = sdk::Mc::D(e, f.player, id.eMotionZ);
  return std::sqrt(x * x + z * z);
}

// Raven Utils.setSpeed port: redirects motion along current input+yaw.
inline void SetStrafeSpeed(sdk::Mc::Frame& f, double speed) {
  JNIEnv* e = f.env;
  auto& id = sdk::Mc::Instance().Ids();
  if (!f.player) return;
  jobject mi = sdk::Mc::Obj(e, f.player, id.spMoveInput);
  double mf = sdk::Mc::Fl(e, mi, id.inForward);
  double ms = sdk::Mc::Fl(e, mi, id.inStrafe);
  float yaw = sdk::Mc::Fl(e, f.player, id.eYaw);
  if (mi) e->DeleteLocalRef(mi);
  if (e->ExceptionCheck()) {
    e->ExceptionClear();
    return;
  }
  if (speed == 0.0) {
    sdk::Mc::SetD(e, f.player, id.eMotionX, 0.0);
    sdk::Mc::SetD(e, f.player, id.eMotionZ, 0.0);
    return;
  }
  if (mf == 0.0 && ms == 0.0) {
    sdk::Mc::SetD(e, f.player, id.eMotionX, 0.0);
    sdk::Mc::SetD(e, f.player, id.eMotionZ, 0.0);
    return;
  }
  if (mf != 0.0) {
    if (ms > 0.0) {
      yaw += mf > 0.0 ? -45.f : 45.f;
    } else if (ms < 0.0) {
      yaw += mf > 0.0 ? 45.f : -45.f;
    }
    ms = 0.0;
    mf = mf > 0.0 ? 1.0 : -1.0;
  }
  constexpr double kPi = 3.14159265358979323846;
  double rad = (yaw + 90.0) * kPi / 180.0;
  double s = std::sin(rad), c = std::cos(rad);
  sdk::Mc::SetD(e, f.player, id.eMotionX, mf * speed * c + ms * speed * s);
  sdk::Mc::SetD(e, f.player, id.eMotionZ, mf * speed * s - ms * speed * c);
}

inline bool MovingInput(sdk::Mc::Frame& f) {
  JNIEnv* e = f.env;
  auto& id = sdk::Mc::Instance().Ids();
  jobject mi = sdk::Mc::Obj(e, f.player, id.spMoveInput);
  float mf = sdk::Mc::Fl(e, mi, id.inForward);
  float ms = sdk::Mc::Fl(e, mi, id.inStrafe);
  if (mi) e->DeleteLocalRef(mi);
  if (e->ExceptionCheck()) {
    e->ExceptionClear();
    return false;
  }
  return mf != 0.f || ms != 0.f;
}

// Potion speed amplifier (0 = no effect). Needs tier-2 potion mappings.
inline int SpeedAmplifier(sdk::Mc::Frame& f) {
  JNIEnv* e = f.env;
  auto& id = sdk::Mc::Instance().Ids();
  if (!f.player || !id.activeFx || !id.potionFx) return 0;
  jobject coll = e->CallObjectMethod(f.player, id.activeFx);
  if (!coll || e->ExceptionCheck()) {
    e->ExceptionClear();
    return 0;
  }
  int amp = 0;
  jobject it = e->CallObjectMethod(coll, id.collIter);
  if (it && !e->ExceptionCheck()) {
    while (e->CallBooleanMethod(it, id.collHas)) {
      if (e->ExceptionCheck()) break;
      jobject fx = e->CallObjectMethod(it, id.collNext);
      if (!fx || e->ExceptionCheck()) {
        e->ExceptionClear();
        break;
      }
      if (e->IsInstanceOf(fx, id.potionFx)) {
        jstring nm = static_cast<jstring>(e->CallObjectMethod(fx, id.fxName));
        if (nm && !e->ExceptionCheck()) {
          const char* c = e->GetStringUTFChars(nm, nullptr);
          bool speed = c && std::strcmp(c, "potion.moveSpeed") == 0;
          if (c) e->ReleaseStringUTFChars(nm, c);
          if (speed) {
            amp = e->CallIntMethod(fx, id.fxAmp) + 1;
            e->DeleteLocalRef(nm);
            e->DeleteLocalRef(fx);
            break;
          }
          e->DeleteLocalRef(nm);
        } else {
          e->ExceptionClear();
        }
      }
      e->DeleteLocalRef(fx);
    }
  } else {
    e->ExceptionClear();
  }
  if (it) e->DeleteLocalRef(it);
  e->DeleteLocalRef(coll);
  if (e->ExceptionCheck()) e->ExceptionClear();
  return amp < 0 ? 0 : amp;
}

// Block object at integer coords (local ref, caller deletes; null on error).
inline jobject BlockAt(sdk::Mc::Frame& f, int x, int y, int z) {
  JNIEnv* e = f.env;
  auto& id = sdk::Mc::Instance().Ids();
  if (!f.world) return nullptr;
  jobject pos = e->NewObject(id.blockPos, id.posCtor, x, y, z);
  if (!pos || e->ExceptionCheck()) {
    e->ExceptionClear();
    return nullptr;
  }
  jobject state = e->CallObjectMethod(f.world, id.wBlockState, pos);
  e->DeleteLocalRef(pos);
  if (!state || e->ExceptionCheck()) {
    e->ExceptionClear();
    return nullptr;
  }
  jobject block = e->CallObjectMethod(state, id.ibsGetBlock);
  e->DeleteLocalRef(state);
  if (!block || e->ExceptionCheck()) {
    e->ExceptionClear();
    return nullptr;
  }
  return block;
}

inline bool IsAirBlock(sdk::Mc::Frame& f, jobject block) {
  JNIEnv* e = f.env;
  auto& id = sdk::Mc::Instance().Ids();
  return !block || e->IsSameObject(block, id.airBlock);
}

// Edge-of-block approx: solid below center, air below at least one neighbor.
inline bool EdgeOfBlock(sdk::Mc::Frame& f) {
  JNIEnv* e = f.env;
  auto& id = sdk::Mc::Instance().Ids();
  if (!f.player) return false;
  double px = sdk::Mc::D(e, f.player, id.ePosX);
  double py = sdk::Mc::D(e, f.player, id.ePosY);
  double pz = sdk::Mc::D(e, f.player, id.ePosZ);
  int cx = static_cast<int>(std::floor(px));
  int cy = static_cast<int>(std::floor(py));
  int cz = static_cast<int>(std::floor(pz));
  jobject center = BlockAt(f, cx, cy - 1, cz);
  bool centerSolid = center && !IsAirBlock(f, center);
  if (center) e->DeleteLocalRef(center);
  if (!centerSolid) return false;
  const int dx[4] = {1, -1, 0, 0};
  const int dz[4] = {0, 0, 1, -1};
  for (int i = 0; i < 4; ++i) {
    jobject b = BlockAt(f, cx + dx[i], cy - 1, cz + dz[i]);
    bool air = !b || IsAirBlock(f, b);
    if (b) e->DeleteLocalRef(b);
    if (air) return true;
  }
  return false;
}

// True when the whole column below is air (caller should throttle to falls).
inline bool OverVoid(sdk::Mc::Frame& f) {
  JNIEnv* e = f.env;
  auto& id = sdk::Mc::Instance().Ids();
  if (!f.player) return false;
  double px = sdk::Mc::D(e, f.player, id.ePosX);
  double py = sdk::Mc::D(e, f.player, id.ePosY);
  double pz = sdk::Mc::D(e, f.player, id.ePosZ);
  int cx = static_cast<int>(std::floor(px));
  int cz = static_cast<int>(std::floor(pz));
  for (int y = static_cast<int>(std::floor(py)) - 1; y > 0; --y) {
    jobject b = BlockAt(f, cx, y, cz);
    bool air = !b || IsAirBlock(f, b);
    if (b) e->DeleteLocalRef(b);
    if (!air) return false;
    if (e->ExceptionCheck()) {
      e->ExceptionClear();
      return false;
    }
  }
  return true;
}

}  // namespace modules
}  // namespace raven
