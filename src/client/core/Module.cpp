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

#include "core/Module.h"

namespace raven {
namespace core {

Module::Module(std::string name, Category cat, int defaultBind)
    : name_(std::move(name)), category_(cat) {
  bind_.vk = defaultBind;
}

void Module::SetEnabled(bool on) {
  if (on == enabled_.load(std::memory_order_relaxed)) return;
  enabled_.store(on, std::memory_order_relaxed);
  if (on) {
    OnEnable();
  } else {
    OnDisable();
  }
}

}  // namespace core
}  // namespace raven
