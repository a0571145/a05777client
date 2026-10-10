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

#include "render/Overlay.h"

namespace raven {
namespace render {

Overlay& Overlay::Instance() {
  static Overlay inst;
  return inst;
}

void Overlay::BeginFrame() { ++frames_; }
void Overlay::Text(int /*x*/, int /*y*/, const char* /*text*/) {}
void Overlay::EndFrame() {}

}  // namespace render
}  // namespace raven
