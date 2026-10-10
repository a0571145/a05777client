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

#include "modules/RawInput.h"
#include <windows.h>
#include "util/Logger.h"

namespace raven {
namespace modules {

RawInput::RawInput() : core::Module("Raw Input", core::Category::Combat, 0) {}

void RawInput::OnEnable() {
  int cur[3] = {0, 0, 0};
  if (!SystemParametersInfoW(SPI_GETMOUSE, 0, cur, 0)) {
    util::Logger::Warn("RawInput: SPI_GETMOUSE failed");
    return;
  }
  mouseParams_[0] = cur[0];
  mouseParams_[1] = cur[1];
  mouseParams_[2] = cur[2];
  saved_ = true;
  int off[3] = {cur[0], cur[1], 0};  // threshold1/2 kept, acceleration off
  if (!SystemParametersInfoW(SPI_SETMOUSE, 0, off, SPIF_SENDCHANGE)) {
    util::Logger::Warn("RawInput: SPI_SETMOUSE failed");
  } else {
    util::Logger::Info("RawInput: OS pointer precision off (raw-like input)");
  }
}

void RawInput::OnDisable() {
  if (!saved_) return;
  saved_ = false;
  SystemParametersInfoW(SPI_SETMOUSE, 0, mouseParams_, SPIF_SENDCHANGE);
  util::Logger::Info("RawInput: OS pointer precision restored");
}

}  // namespace modules
}  // namespace raven
