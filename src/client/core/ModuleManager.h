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
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "core/Module.h"

// C++ port of keystrokesmod/module/ModuleManager.java (scaffold: register + lookup).
namespace raven {
namespace core {

class ModuleManager {
public:
  void RegisterDefaults();
  void Add(std::shared_ptr<Module> m);

  void TickAll();    // calls OnUpdate on enabled modules
  void RenderAll();  // calls OnRender on enabled modules
  void DisableAll(); // releases module-owned game/native state before unload

  Module* Find(const std::string& name) const;
  const std::vector<std::shared_ptr<Module>>& All() const { return modules_; }
  // Copy of the list (safe to iterate from module callbacks: re-entrant).
  std::vector<std::shared_ptr<Module>> Snapshot() const;

  // REST thread + game thread share modules; guard every access.
  // Recursive: module callbacks (e.g. HUD) may re-enter the manager.
  std::recursive_mutex& Mutex() const { return mutex_; }

private:
  std::vector<std::shared_ptr<Module>> modules_;
  mutable std::recursive_mutex mutex_;
};

}  // namespace core
}  // namespace raven
