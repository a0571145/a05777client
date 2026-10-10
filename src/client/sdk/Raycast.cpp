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

#include "sdk/Raycast.h"
#include <cmath>
#include "sdk/Mc.h"

namespace raven {
namespace sdk {

void FreeRayHit(JNIEnv* env, RayHit& h) {
  if (env && h.entity) env->DeleteGlobalRef(h.entity);
  h = RayHit();
}

bool RaycastEntity(JNIEnv* env, Mc::Frame& f, double reach, double mult, bool playersOnly,
                   RayHit& out) {
  out = RayHit();
  Mc& mc = Mc::Instance();
  if (!env || !f.mc || !f.world || !mc.ExtraReady()) return false;
  McIds& id = mc.Ids();

  jobject view = env->CallObjectMethod(f.mc, id.mcRenderView);
  if (!view || env->ExceptionCheck()) {
    env->ExceptionClear();
    return false;
  }
  jobject eyes = nullptr, look = nullptr, end = nullptr, box = nullptr, grown = nullptr,
          list = nullptr, arr = nullptr;
  jobject winner = nullptr;
  double best = reach, hx = 0, hy = 0, hz = 0;

  do {
    eyes = env->CallObjectMethod(view, id.eGetEyes, f.partialTicks);
    look = env->CallObjectMethod(view, id.eGetLook, f.partialTicks);
    if (!eyes || !look || env->ExceptionCheck()) break;
    double lx = env->GetDoubleField(look, id.vecX);
    double ly = env->GetDoubleField(look, id.vecY);
    double lz = env->GetDoubleField(look, id.vecZ);
    if (env->ExceptionCheck()) break;
    end = env->CallObjectMethod(eyes, id.vecAdd, lx * reach, ly * reach, lz * reach);
    box = env->CallObjectMethod(view, id.eGetBox);
    if (!end || !box || env->ExceptionCheck()) break;
    grown = env->CallObjectMethod(box, id.boxExpand, lx * reach, ly * reach, lz * reach);
    if (!grown || env->ExceptionCheck()) break;
    jobject grown2 = env->CallObjectMethod(grown, id.boxGrow, 1.0, 1.0, 1.0);
    env->DeleteLocalRef(grown);
    grown = grown2;
    if (!grown || env->ExceptionCheck()) break;
    list = env->CallObjectMethod(f.world, id.wGetEntities, view, grown);
    if (!list || env->ExceptionCheck()) break;
    arr = env->CallObjectMethod(list, id.listToArray);
    if (!arr || env->ExceptionCheck()) break;
    jsize n = env->GetArrayLength(static_cast<jarray>(arr));
    jobject riding = env->GetObjectField(view, id.eRiding);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      riding = nullptr;
    }
    for (jsize i = 0; i < n; ++i) {
      jobject ent =
          env->GetObjectArrayElement(static_cast<jobjectArray>(arr), i);
      if (!ent) continue;
      bool collide = env->CallBooleanMethod(ent, id.eCanCollide);
      if (!collide || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(ent);
        continue;
      }
      if (playersOnly && !env->IsInstanceOf(ent, id.entityPlayer)) {
        env->DeleteLocalRef(ent);
        continue;
      }
      float border = env->CallFloatMethod(ent, id.eBorder);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(ent);
        continue;
      }
      double ex = static_cast<double>(border) * mult;
      jobject eb = env->CallObjectMethod(ent, id.eGetBox);
      if (!eb || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(ent);
        continue;
      }
      jobject eb2 = env->CallObjectMethod(eb, id.boxGrow, ex, ex, ex);
      env->DeleteLocalRef(eb);
      if (!eb2 || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(ent);
        continue;
      }
      jobject mop = env->CallObjectMethod(eb2, id.boxIntercept, eyes, end);
      bool inside = env->CallBooleanMethod(eb2, id.boxIsVec, eyes);
      env->DeleteLocalRef(eb2);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (mop) env->DeleteLocalRef(mop);
        env->DeleteLocalRef(ent);
        continue;
      }
      if (inside) {
        if (best > 0.0 || best == 0.0) {
          if (winner) env->DeleteLocalRef(winner);
          winner = ent;
          ent = nullptr;
          double exx = env->GetDoubleField(eyes, id.vecX);
          double eyy = env->GetDoubleField(eyes, id.vecY);
          double ezz = env->GetDoubleField(eyes, id.vecZ);
          hx = exx;
          hy = eyy;
          hz = ezz;
          best = 0.0;
          if (mop) env->DeleteLocalRef(mop);
        } else {
          if (mop) env->DeleteLocalRef(mop);
        }
      } else if (mop) {
        jobject hitVec = env->GetObjectField(mop, id.mopHitVec);
        double vx = 0, vy = 0, vz = 0;
        bool haveVec = false;
        if (hitVec && !env->ExceptionCheck()) {
          vx = env->GetDoubleField(hitVec, id.vecX);
          vy = env->GetDoubleField(hitVec, id.vecY);
          vz = env->GetDoubleField(hitVec, id.vecZ);
          haveVec = !env->ExceptionCheck();
        } else {
          env->ExceptionClear();
        }
        if (hitVec) env->DeleteLocalRef(hitVec);
        env->DeleteLocalRef(mop);
        if (env->ExceptionCheck() || !haveVec) {
          env->ExceptionClear();
          env->DeleteLocalRef(ent);
          continue;
        }
        // Real (non-squared) distance, like the Java reference.
        double exx = env->GetDoubleField(eyes, id.vecX);
        double eyy = env->GetDoubleField(eyes, id.vecY);
        double ezz = env->GetDoubleField(eyes, id.vecZ);
        double dx = vx - exx, dy = vy - eyy, dz = vz - ezz;
        double real = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (real < best || best == 0.0) {
          bool skipRider = false;
          if (riding && env->IsSameObject(ent, riding)) {
            if (best != 0.0) {
              skipRider = true;  // riding entity only wins at zero distance
            } else {
              // Zero-distance riding case (mirrors Java): pass the real
              // hit vector, never null, so game code cannot NPE on it.
              jobject hv = env->NewObject(id.vec3, id.vecCtor, vx, vy, vz);
              jboolean can = JNI_FALSE;
              if (hv && !env->ExceptionCheck()) {
                can = env->CallBooleanMethod(ent, id.eCanRide, f.player, hv);
              }
              if (hv) env->DeleteLocalRef(hv);
              if (env->ExceptionCheck()) {
                env->ExceptionClear();
              } else if (!can) {
                skipRider = true;
              }
            }
          }
          if (!skipRider) {
            if (winner) env->DeleteLocalRef(winner);
            winner = ent;
            ent = nullptr;
            hx = vx;
            hy = vy;
            hz = vz;
            best = real;
          }
        }
      }
      if (ent) env->DeleteLocalRef(ent);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        break;
      }
    }
    if (riding) env->DeleteLocalRef(riding);
  } while (false);

  if (env->ExceptionCheck()) env->ExceptionClear();
  jobject refs[] = {eyes, look, end, box, grown, list, arr, view};
  for (jobject r : refs) {
    if (r) env->DeleteLocalRef(r);
  }
  if (winner) {
    out.hit = true;
    out.entity = env->NewGlobalRef(winner);
    env->DeleteLocalRef(winner);
    out.hx = hx;
    out.hy = hy;
    out.hz = hz;
    out.dist = best;
  }
  return out.hit;
}

void ApplyPointed(JNIEnv* env, Mc::Frame& f, const RayHit& h) {
  if (!env || !h.hit || !h.entity) return;
  Mc& mc = Mc::Instance();
  McIds& id = mc.Ids();
  jobject vec = env->NewObject(id.vec3, id.vecCtor, h.hx, h.hy, h.hz);
  if (!vec || env->ExceptionCheck()) {
    env->ExceptionClear();
    return;
  }
  jobject mop = env->NewObject(id.mop, id.mopCtor, h.entity, vec);
  env->DeleteLocalRef(vec);
  if (!mop || env->ExceptionCheck()) {
    env->ExceptionClear();
    return;
  }
  env->SetObjectField(f.mc, id.mcMouseOver, mop);
  env->SetObjectField(f.mc, id.mcPointed, h.entity);
  env->DeleteLocalRef(mop);
  if (env->ExceptionCheck()) env->ExceptionClear();
}

bool LineOfSight(JNIEnv* env, Mc::Frame& f, double tx, double ty, double tz) {
  Mc& mc = Mc::Instance();
  if (!env || !f.world) return true;
  McIds& id = mc.Ids();
  jobject view = env->CallObjectMethod(f.mc, id.mcRenderView);
  if (!view || env->ExceptionCheck()) {
    env->ExceptionClear();
    return true;
  }
  jobject eyes = env->CallObjectMethod(view, id.eGetEyes, f.partialTicks);
  jobject tgt = env->NewObject(id.vec3, id.vecCtor, tx, ty, tz);
  bool visible = true;
  if (eyes && tgt && !env->ExceptionCheck()) {
    jobject mop = env->CallObjectMethod(f.world, id.wRayTrace, eyes, tgt, JNI_FALSE,
                                        JNI_FALSE, JNI_FALSE);
    if (mop && !env->ExceptionCheck()) {
      jobject hitVec = env->GetObjectField(mop, id.mopHitVec);
      if (hitVec && !env->ExceptionCheck()) {
        double hx = env->GetDoubleField(hitVec, id.vecX);
        double hy = env->GetDoubleField(hitVec, id.vecY);
        double hz = env->GetDoubleField(hitVec, id.vecZ);
        double ex = env->GetDoubleField(eyes, id.vecX);
        double ey = env->GetDoubleField(eyes, id.vecY);
        double ez = env->GetDoubleField(eyes, id.vecZ);
        double db = (hx - ex) * (hx - ex) + (hy - ey) * (hy - ey) + (hz - ez) * (hz - ez);
        double dt = (tx - ex) * (tx - ex) + (ty - ey) * (ty - ey) + (tz - ez) * (tz - ez);
        visible = db + 0.36 >= dt;  // block hit at/after the target
        env->DeleteLocalRef(hitVec);
      }
      env->DeleteLocalRef(mop);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  } else {
    env->ExceptionClear();
  }
  if (eyes) env->DeleteLocalRef(eyes);
  if (tgt) env->DeleteLocalRef(tgt);
  env->DeleteLocalRef(view);
  return visible;
}

}  // namespace sdk
}  // namespace raven
