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

#include "modules/Speed.h"
#include <cmath>
#include "modules/MoveUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

Speed::Speed() : core::Module("Speed", core::Category::Movement, 0) {
  RegisterMode(&mode);
  RegisterSlider(&multiplier);
  RegisterBool(&onlyForward);
  RegisterBool(&onlyStrafe);
}

void Speed::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player) break;
    double hSpeed = HSpeed(f);
    if (hSpeed == 0.0) break;
    bool onGround = sdk::Mc::Bo(env, f.player, id.eOnGround);
    jobject caps = sdk::Mc::Obj(env, f.player, id.pCaps);
    bool flying = caps && sdk::Mc::Bo(env, caps, id.cFlying);
    if (caps) env->DeleteLocalRef(caps);
    if (!onGround || flying) break;

    jint hurt = sdk::Mc::In(env, f.player, id.lHurtTime);
    jint maxHurt = sdk::Mc::In(env, f.player, id.lMaxHurt);
    if (hurt > 0 && hurt == maxHurt) break;  // freshly hurt: leave motion alone

    jobject mi = sdk::Mc::Obj(env, f.player, id.spMoveInput);
    float mf = sdk::Mc::Fl(env, mi, id.inForward);
    float ms = sdk::Mc::Fl(env, mi, id.inStrafe);
    bool jump = sdk::Mc::Bo(env, mi, id.inJump);
    bool sneak = sdk::Mc::Bo(env, mi, id.inSneak);
    if (mi) env->DeleteLocalRef(mi);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      break;
    }
    if (jump) break;
    if (onlyForward.value && mf <= 0.f) break;
    if (onlyStrafe.value && ms == 0.f) break;

    if (mode.index == 0) {
      double val = multiplier.value - (multiplier.value - 1.0) * 0.5;
      SetStrafeSpeed(f, hSpeed * val);
    } else {
      // Float-lite: sustain ground speed with potion levels.
      groundTicks_ = onGround ? groundTicks_ + 1 : 0;
      stillTicks_ = (mf != 0.f || ms != 0.f) ? 0 : stillTicks_ + 1;
      double frac = sdk::Mc::D(env, f.player, id.ePosY);
      frac = frac - std::floor(frac);
      int edgeY = static_cast<int>(std::round(frac * 100.0));
      bool reqJump = requireJump_;
      if (groundTicks_ <= 8) {
        // keep floating (Java: canFloat via groundTicks)
      } else if (stillTicks_ > 20) {
        reqJump = true;
      } else if (frac != 0.0 && edgeY >= 10) {
        reqJump = true;  // snow/carpet special-case omitted
      } else if (!onGround) {
        reqJump = false;
      } else if (sneak) {
        reqJump = false;
      }
      requireJump_ = reqJump;
      if (!onGround) requireJump_ = false;
      if (!reqJump) {
        sdk::Mc::SetD(env, f.player, id.ePosY,
                      sdk::Mc::D(env, f.player, id.ePosY) + 2.01e-11);
        if (mf != 0.f || ms != 0.f) {
          int level = SpeedAmplifier(f);
          double minAdj = (mf != 0.f && ms != 0.f) ? 0.003 : 0.0;
          int idx = level >= 0 ? level : 0;
          if (idx > 4) idx = 4;
          SetStrafeSpeed(f, kLevels[idx] - minAdj);
        }
      }
    }
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
