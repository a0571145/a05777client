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

#include "core/ModuleManager.h"
#include <mutex>
#include "modules/AimAssist.h"
#include "modules/AntiAFK.h"
#include "modules/AutoBlock.h"
#include "modules/AutoClicker.h"
#include "modules/AutoJump.h"
#include "modules/AutoSwap.h"
#include "modules/AutoTool.h"
#include "modules/BHop.h"
#include "modules/BridgeAssist.h"
#include "modules/ClickAssist.h"
#include "modules/FastPlace.h"
#include "modules/Fly.h"
#include "modules/Fullbright.h"
#include "modules/HitBox.h"
#include "modules/Hud.h"
#include "modules/JumpReset.h"
#include "modules/KeepSprint.h"
#include "modules/KillAura.h"
#include "modules/NoFall.h"
#include "modules/NoSlow.h"
#include "modules/Piercing.h"
#include "modules/PlayerESP.h"
#include "modules/RawInput.h"
#include "modules/Reach.h"
#include "modules/Reduce.h"
#include "modules/RodAimbot.h"
#include "modules/SafeWalk.h"
#include "modules/Speed.h"
#include "modules/Sprint.h"
#include "modules/TPAura.h"
#include "modules/Velocity.h"
#include "modules/WTap.h"

namespace raven {
namespace core {

void ModuleManager::RegisterDefaults() {
  // Batch 0: Sprint + HUD proved the lifecycle.
  // Batch 1: real JNI-backed ports (vanilla 1.8.9 notch mappings).
  // Full port target mirrors raven-src: combat 20 / movement 18 / player 22 /
  // render 33 / minigames 10 / fun 4 / other 6 / client 5 / world 2.
  Add(std::make_shared<modules::Sprint>());
  auto hud = std::make_shared<modules::Hud>();
  Add(hud);
  hud->SetEnabled(true);  // HUD on by default (runs OnEnable -> overlay)
  Add(std::make_shared<modules::Velocity>());
  Add(std::make_shared<modules::NoSlow>());
  Add(std::make_shared<modules::AutoBlock>());
  Add(std::make_shared<modules::AutoClicker>());
  Add(std::make_shared<modules::KeepSprint>());
  Add(std::make_shared<modules::Fullbright>());
  Add(std::make_shared<modules::PlayerESP>());
  Add(std::make_shared<modules::Reach>());
  Add(std::make_shared<modules::HitBox>());
  Add(std::make_shared<modules::KillAura>());
  Add(std::make_shared<modules::Fly>());
  Add(std::make_shared<modules::Speed>());
  Add(std::make_shared<modules::NoFall>());
  Add(std::make_shared<modules::SafeWalk>());
  Add(std::make_shared<modules::BHop>());
  Add(std::make_shared<modules::WTap>());
  Add(std::make_shared<modules::JumpReset>());
  Add(std::make_shared<modules::ClickAssist>());
  Add(std::make_shared<modules::Reduce>());
  Add(std::make_shared<modules::AimAssist>());
  Add(std::make_shared<modules::RodAimbot>());
  Add(std::make_shared<modules::Piercing>());
  Add(std::make_shared<modules::TPAura>());
  Add(std::make_shared<modules::RawInput>());
  // Batch player-lite: polling ports of AntiAFK/AutoTool/FastPlace/AutoJump.
  Add(std::make_shared<modules::AntiAFK>());
  Add(std::make_shared<modules::AutoTool>());
  Add(std::make_shared<modules::FastPlace>());
  Add(std::make_shared<modules::AutoJump>());
  // Scaffold batch: BridgeAssist-lite + AutoSwap-lite.
  Add(std::make_shared<modules::BridgeAssist>());
  Add(std::make_shared<modules::AutoSwap>());
}

void ModuleManager::Add(std::shared_ptr<Module> m) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  modules_.push_back(std::move(m));
}

void ModuleManager::TickAll() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  for (auto& m : modules_) {
    if (m->IsEnabled()) m->OnUpdate();
  }
}

void ModuleManager::RenderAll() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  for (auto& m : modules_) {
    if (m->IsEnabled()) m->OnRender();
  }
}

void ModuleManager::DisableAll() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  for (auto& m : modules_) {
    if (m->IsEnabled()) m->SetEnabled(false);
  }
}

Module* ModuleManager::Find(const std::string& name) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  for (auto& m : modules_) {
    if (m->Name() == name) return m.get();
  }
  return nullptr;
}

std::vector<std::shared_ptr<Module>> ModuleManager::Snapshot() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return modules_;
}

}  // namespace core
}  // namespace raven
