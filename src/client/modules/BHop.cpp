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

#include "modules/BHop.h"
#include <cmath>
#include <random>
#include "modules/MoveUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

namespace {
thread_local std::mt19937 t_rng(std::random_device{}());
}

BHop::BHop() : core::Module("BHop", core::Category::Movement, 0) {
  RegisterMode(&mode);
  RegisterSlider(&speed);
  RegisterBool(&liquidDisable);
  RegisterBool(&sneakDisable);
  RegisterBool(&jumpMoving);
}

void BHop::OnDisable() { hopping_ = false; }

void BHop::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player) break;
    if (liquidDisable.value) {
      jboolean water = env->CallBooleanMethod(f.player, id.inWaterM);
      jboolean lava = env->CallBooleanMethod(f.player, id.inLavaM);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        break;
      }
      if (water || lava) break;
    }
    jboolean sneaking = env->CallBooleanMethod(f.player, id.isSneakingM);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      break;
    }
    if (sneaking && sneakDisable.value) break;

    bool onGround = sdk::Mc::Bo(env, f.player, id.eOnGround);
    jobject caps = sdk::Mc::Obj(env, f.player, id.pCaps);
    bool flying = caps && sdk::Mc::Bo(env, caps, id.cFlying);
    if (caps) env->DeleteLocalRef(caps);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      break;
    }

    // Post-input: suppress vanilla jump while hopping on ground.
    if (onGround && !flying && hopping_) {
      jobject mi = sdk::Mc::Obj(env, f.player, id.spMoveInput);
      if (mi) {
        sdk::Mc::SetB(env, mi, id.inJump, false);
        env->DeleteLocalRef(mi);
      }
    }

    bool moving = MovingInput(f);
    double hSpeed = HSpeed(f);

    if (mode.index >= 1) {
      if (onGround && (!jumpMoving.value || moving)) {
        env->CallVoidMethod(f.player, id.jumpM);
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          break;
        }
        double base = speed.value - 0.52;
        int amp = SpeedAmplifier(f);
        double mod = base;
        if (amp == 1) {
          mod = base + 0.02;
        } else if (amp == 2) {
          mod = base + 0.04;
        } else if (amp >= 3) {
          mod = base + 0.1;
        }
        if (moving) {
          double jitter =
              std::uniform_real_distribution<double>(0.0001, 0.0003)(t_rng);
          SetStrafeSpeed(f, mod - jitter);
        }
        hopping_ = true;
      }
      if (!onGround) hopping_ = false;
    }

    if (mode.index == 0) {
      if (moving) {
        if (onGround) env->CallVoidMethod(f.player, id.jumpM);
        env->CallVoidMethod(f.player, id.setSprintM, JNI_TRUE);
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          break;
        }
        SetStrafeSpeed(f, hSpeed + 0.005 * speed.value);
        hopping_ = true;
      }
    } else {
      // 9/8/7-tick Y tweaks (ModuleUtils simpleY magic numbers).
      airTicks_ = onGround ? 0 : airTicks_ + 1;
      double fracY = sdk::Mc::D(env, f.player, id.ePosY);
      fracY = fracY - std::floor(fracY);
      int simpleY = static_cast<int>(std::round(fracY * 10000.0));
      double motionY = sdk::Mc::D(env, f.player, id.eMotionY);
      auto addY = [&](double d) {
        sdk::Mc::SetD(env, f.player, id.eMotionY, motionY + d);
      };
      if (mode.index == 1 || mode.index == 2) {  // Ground + 9 tick share case 2 shape
        if (mode.index == 2) {
          if (simpleY == 13) {
            addY(-0.02483);
          } else if (simpleY == 2000) {
            addY(-0.1913);
          } else if (simpleY == 7016) {
            addY(0.08);
          }
          if (airTicks_ > 6 && moving) SetStrafeSpeed(f, hSpeed);
          if (airTicks_ > 8) lowhopAir_ = false;
        }
      } else if (mode.index == 3) {  // 8 tick
        if (simpleY == 13) {
          addY(-0.045);
        } else if (simpleY == 2000) {
          addY(-0.175);
          lowhopAir_ = false;
        }
      } else if (mode.index == 4) {  // 7 tick
        if (simpleY == 1138) {
          addY(-0.13);
        } else if (simpleY == 2031) {
          addY(-0.2);
          lowhopAir_ = false;
        } else if (simpleY == 4200) {
          sdk::Mc::SetD(env, f.player, id.eMotionY, 0.39);
        }
      }
    }

    if (!onGround) {
      lowhopAir_ = true;
    } else if (lowhopAir_) {
      lowhopAir_ = false;
    }

    // Air damp (canSlow branch).
    if (!onGround) {
      double motionVal =
          0.9 - airTicks_ / 10000.0 -
          std::uniform_real_distribution<double>(1.0e-5, 6.0e-5)(t_rng);
      jint hurt = sdk::Mc::In(env, f.player, id.lHurtTime);
      jint maxHurt = sdk::Mc::In(env, f.player, id.lMaxHurt);
      if (maxHurt == 0 && airTicks_ > 4 && !setSlow_) {
        sdk::Mc::SetD(env, f.player, id.eMotionX,
                      sdk::Mc::D(env, f.player, id.eMotionX) * motionVal);
        sdk::Mc::SetD(env, f.player, id.eMotionZ,
                      sdk::Mc::D(env, f.player, id.eMotionZ) * motionVal);
        setSlow_ = true;
      }
      if (hurt != 0) {
        // Java keys on maxHurt==0 (peaceful-ish); otherwise leave motion.
      }
    } else {
      setSlow_ = false;
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
