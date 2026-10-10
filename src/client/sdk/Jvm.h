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
#include <windows.h>
#include <jni.h>

// Real JVM bridge (headers from third_party/jni).
// Attaches calling threads as daemons; safe to call from worker/render threads.
namespace raven {
namespace sdk {

class Jvm {
public:
  static Jvm& Instance();

  // Resolves jvm.dll + JavaVM*. False outside javaw (smoke tests).
  bool Attach();
  void Detach();
  bool HasJvm() const { return vm_ != nullptr; }
  JavaVM* Vm() const { return vm_; }

  // JNIEnv for the calling thread, attaching as daemon on first use.
  // Returns nullptr when no JVM is present.
  JNIEnv* EnvForCurrentThread();

private:
  Jvm() = default;
  JavaVM* vm_ = nullptr;
};

}  // namespace sdk
}  // namespace raven
