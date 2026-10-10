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

#include "sdk/Jvm.h"
#include "util/Logger.h"

namespace raven {
namespace sdk {

namespace {
thread_local JNIEnv* t_env = nullptr;
}

Jvm& Jvm::Instance() {
  static Jvm inst;
  return inst;
}

bool Jvm::Attach() {
  if (vm_) return true;
  HMODULE jvm = GetModuleHandleW(L"jvm.dll");
  if (!jvm) {
    util::Logger::Info("Jvm::Attach: no jvm.dll, running outside javaw (expected for smoke test)");
    return false;
  }
  using Fn = jint(JNICALL*)(JavaVM**, jsize, jsize*);
  auto getVms = reinterpret_cast<Fn>(GetProcAddress(jvm, "JNI_GetCreatedJavaVMs"));
  if (!getVms) return false;
  JavaVM* vms[4] = {};
  jsize count = 0;
  if (getVms(vms, 4, &count) != JNI_OK || count == 0) return false;
  vm_ = vms[0];
  util::Logger::Info("Jvm::Attach: JavaVM resolved inside javaw.exe");
  return true;
}

void Jvm::Detach() {
  vm_ = nullptr;
  t_env = nullptr;
}

JNIEnv* Jvm::EnvForCurrentThread() {
  if (t_env) return t_env;
  if (!vm_ && !Attach()) return nullptr;
  if (!vm_) return nullptr;
  JNIEnv* env = nullptr;
  if (vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
    t_env = env;
    return env;
  }
  // Attach as daemon so a lingering game thread can't block JVM shutdown.
  if (vm_->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
    return nullptr;
  }
  t_env = env;
  return env;
}

}  // namespace sdk
}  // namespace raven
