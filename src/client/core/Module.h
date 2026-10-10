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
#include <atomic>
#include <string>
#include <vector>
#include "core/Setting.h"

// C++ port of keystrokesmod/module/Module.java (scaffold).
// Java: enable()/disable() == EVENT_BUS.register/unregister + onEnable/onDisable.
// C++:  modules are ticked by Client each frame; enabled modules get onUpdate/onRender.
namespace raven {
namespace core {

enum class Category {
  Combat,
  Movement,
  Player,
  Render,
  World,
  Minigames,
  Other,
  Client,
};

inline const char* CategoryName(Category c) {
  switch (c) {
    case Category::Combat: return "Combat";
    case Category::Movement: return "Movement";
    case Category::Player: return "Player";
    case Category::Render: return "Render";
    case Category::World: return "World";
    case Category::Minigames: return "Minigames";
    case Category::Other: return "Other";
    case Category::Client: return "Client";
    default: return "?";
  }
}

class Module {
public:
  Module(std::string name, Category cat, int defaultBind = 0);
  virtual ~Module() = default;

  Module(const Module&) = delete;
  Module& operator=(const Module&) = delete;

  const std::string& Name() const { return name_; }
  Category GetCategory() const { return category_; }
  bool IsEnabled() const { return enabled_.load(std::memory_order_relaxed); }
  int Bind() const { return bind_.vk; }
  KeySetting& BindSetting() { return bind_; }

  void SetEnabled(bool on);
  void Toggle() { SetEnabled(!IsEnabled()); }

  // Setting registry: derived modules own the setting objects as members
  // and register pointers here so the web UI (REST) can enumerate + mutate
  // them generically. Mirrors Module.settings in keystrokesmod.
  void RegisterBool(BoolSetting* s) { boolSettings_.push_back(s); }
  void RegisterSlider(SliderSetting* s) { sliderSettings_.push_back(s); }
  void RegisterMode(ModeSetting* s) { modeSettings_.push_back(s); }
  void RegisterColor(ColorSetting* s) { colorSettings_.push_back(s); }

  const std::vector<BoolSetting*>& Bools() const { return boolSettings_; }
  const std::vector<SliderSetting*>& Sliders() const { return sliderSettings_; }
  const std::vector<ModeSetting*>& Modes() const { return modeSettings_; }
  const std::vector<ColorSetting*>& Colors() const { return colorSettings_; }

  // Per-frame callbacks (driven by wglSwapBuffers hook in Phase 2; timer fallback in scaffold).
  virtual void OnEnable() {}
  virtual void OnDisable() {}
  virtual void OnUpdate() {}  // game tick-ish
  virtual void OnRender() {}  // frame / overlay pass

protected:
  std::string name_;
  Category category_;
  std::atomic<bool> enabled_{false};  // read cross-thread (game tick vs web)
  KeySetting bind_;
  std::vector<BoolSetting*> boolSettings_;
  std::vector<SliderSetting*> sliderSettings_;
  std::vector<ModeSetting*> modeSettings_;
  std::vector<ColorSetting*> colorSettings_;
};

}  // namespace core
}  // namespace raven
