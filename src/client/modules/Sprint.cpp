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

#include "modules/Sprint.h"
#include <jni.h>
#include "core/Client.h"
#include "modules/WTap.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

Sprint::Sprint() : core::Module("Sprint", core::Category::Movement, /*bind*/ 0) {
  RegisterBool(&allowInInventory);
}

static void SetSprintKey(JNIEnv* env, sdk::Mc::Frame& f, bool pressed) {
  if (!f.sprintKey) return;
  auto& id = sdk::Mc::Instance().Ids();
  jint code = env->CallIntMethod(f.sprintKey, id.kGetCode);
  if (!env->ExceptionCheck() && code != 0) {
    env->CallStaticVoidMethod(id.keyBinding, id.kSetState, code,
                              pressed ? JNI_TRUE : JNI_FALSE);
  }
  if (env->ExceptionCheck()) env->ExceptionClear();
}

void Sprint::OnUpdate() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  bool allowed = f.screen == nullptr || (allowInInventory.value && f.screen != nullptr);
  if (allowed) {
    // WTap cooperation (mirrors Java stopSprint consumed by the sprint Mixin).
    core::Module* w = core::Client::Instance().Modules().Find("WTap");
    auto* wtap = dynamic_cast<WTap*>(w);
    if (wtap && wtap->IsEnabled() && wtap->IsResetting()) {
      SetSprintKey(env, f, false);
      mc.ReleaseFrame(f);
      return;
    }
    SetSprintKey(env, f, true);
  }
  mc.ReleaseFrame(f);
}

void Sprint::OnDisable() {
  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  SetSprintKey(env, f, false);
  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
