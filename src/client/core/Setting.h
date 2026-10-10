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
#include <functional>
#include <memory>
#include <string>
#include <vector>

// C++ port of keystrokesmod/module/setting/*Setting.java (scaffold: Bool/Slider/Key/Mode only).
namespace raven {
namespace core {

struct BoolSetting {
  std::string name;
  bool value = false;
  BoolSetting(std::string n, bool v) : name(std::move(n)), value(v) {}
};

struct SliderSetting {
  std::string name;
  double value = 0.0, min = 0.0, max = 0.0, step = 0.0;
  SliderSetting(std::string n, double v, double mn, double mx, double s)
      : name(std::move(n)), value(v), min(mn), max(mx), step(s) {}
};

struct KeySetting {
  std::string name = "Bind";
  int vk = 0;  // 0 = none, else Win32 VK code
};

struct ModeSetting {
  std::string name;
  std::vector<std::string> options;
  int index = 0;
  ModeSetting(std::string n, std::vector<std::string> o, int i = 0)
      : name(std::move(n)), options(std::move(o)), index(i) {}
  std::string Current() const { return options.empty() ? "" : options[index]; }
};

struct ColorSetting {
  std::string name;
  int r = 255, g = 255, b = 255;
  ColorSetting(std::string n, int rr, int gg, int bb)
      : name(std::move(n)), r(rr), g(gg), b(bb) {}
};

}  // namespace core
}  // namespace raven
