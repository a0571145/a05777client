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

#include "modules/Fullbright.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

Fullbright::Fullbright() : core::Module("Fullbright", core::Category::Render, 0) {
  RegisterSlider(&brightness);
}

void Fullbright::OnEnable() {
  saved_ = -1.f;
  OnUpdate();  // apply immediately; saves current gamma first
}

void Fullbright::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  if (f.settings) {
    auto& id = mc.Ids();
    float cur = env->GetFloatField(f.settings, id.sGamma);
    if (saved_ < 0.f) saved_ = cur;
    float want = static_cast<float>(brightness.value);
    if (cur < want - 1e-3f || cur > want + 1e-3f) {
      env->SetFloatField(f.settings, id.sGamma, want);
    }
  }
  mc.ReleaseFrame(f);
}

void Fullbright::OnDisable() {
  if (saved_ < 0.f) return;
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  if (f.settings) {
    env->SetFloatField(f.settings, mc.Ids().sGamma, saved_);
  }
  mc.ReleaseFrame(f);
  saved_ = -1.f;
}

}  // namespace modules
}  // namespace raven
