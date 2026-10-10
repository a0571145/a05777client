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
#include <jni.h>
#include "sdk/Mc.h"

// Shared entity raycast for Reach / HitBox / KillAura.
// Mirrors Reach.getEntity + HitBox.getEntity (vanilla EntityRenderer raycast):
// AABB sweep along the render-view look vector with per-entity border
// expansion, riding-entity handling, and closest-hit selection.
namespace raven {
namespace sdk {

struct RayHit {
  bool hit = false;
  jobject entity = nullptr;  // GLOBAL ref; release with FreeRayHit
  double hx = 0.0, hy = 0.0, hz = 0.0;
  double dist = 0.0;
};

void FreeRayHit(JNIEnv* env, RayHit& h);

// Casts from the render-view eyes along look, up to reach blocks.
// mult: HitBox-style border multiplier (1.0 = vanilla).
// playersOnly: skip non-EntityPlayer hits (HitBox option).
bool RaycastEntity(JNIEnv* env, Mc::Frame& f, double reach, double mult,
                   bool playersOnly, RayHit& out);

// Writes objectMouseOver + pointedEntity for a confirmed hit.
void ApplyPointed(JNIEnv* env, Mc::Frame& f, const RayHit& h);

// True when the segment eyes->(tx,ty,tz) hits no block first.
bool LineOfSight(JNIEnv* env, Mc::Frame& f, double tx, double ty, double tz);

}  // namespace sdk
}  // namespace raven
