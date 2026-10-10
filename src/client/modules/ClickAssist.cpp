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

#include "modules/ClickAssist.h"
#include <chrono>
#include <random>
#include <windows.h>
#include "hooks/Hooks.h"
#include "modules/CombatUtil.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"

namespace raven {
namespace modules {

namespace {
thread_local std::mt19937 t_rng(std::random_device{}());

long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

bool Foreground() {
  HWND mcw = hooks::Hooks::Instance().Window();
  return mcw && GetForegroundWindow() == mcw;
}
}  // namespace

ClickAssist::ClickAssist() : core::Module("ClickAssist", core::Category::Combat, 0) {
  RegisterBool(&disableCreative);
  RegisterBool(&leftClick);
  RegisterSlider(&chanceLeft);
  RegisterBool(&weaponOnly);
  RegisterBool(&onlyTargeting);
  RegisterBool(&rightClick);
  RegisterSlider(&chanceRight);
  RegisterBool(&blocksOnly);
  RegisterBool(&aboveCps);
}

void ClickAssist::OnEnable() {
  prevL_ = prevR_ = false;
  ignoreUntilMs_ = 0;
  edgePos_ = 0;
  for (auto& e : edges_) e = 0;
}

void ClickAssist::OnDisable() { OnEnable(); }

void ClickAssist::RecordEdge(long long now) {
  edges_[edgePos_ % 32] = now;
  edgePos_++;
}

bool ClickAssist::CpsAbove5(long long now) {
  int n = 0;
  for (auto t : edges_) {
    if (t > 0 && now - t < 1000) n++;
  }
  return n > 5;
}

void ClickAssist::OnUpdate() {
  bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
  bool rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
  long long now = NowMs();
  bool edgeL = lmb && !prevL_;
  bool edgeR = rmb && !prevR_;
  prevL_ = lmb;
  prevR_ = rmb;
  if ((!edgeL && !edgeR) || now < ignoreUntilMs_ || !Foreground()) return;

  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env) || !mc.EnsureExtra(env)) return;
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  do {
    if (!f.player) break;
    if (f.screen || !f.hasFocus) break;
    jobject caps = sdk::Mc::Obj(env, f.player, id.pCaps);
    bool creative = caps && sdk::Mc::Bo(env, caps, id.cCreative);
    if (caps) env->DeleteLocalRef(caps);
    if (disableCreative.value && creative) break;
    jboolean usingItem = env->CallBooleanMethod(f.player, id.pIsUsingItem);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      break;
    }
    if (usingItem) break;  // Java isConsuming gate
    if (aboveCps.value && !CpsAbove5(now)) break;

    if (edgeL) RecordEdge(now);
    if (edgeR) RecordEdge(now);

    if (edgeL && leftClick.value && chanceLeft.value > 0.0) {
      if (weaponOnly.value && !combat::HoldingWeapon(env, f.player)) break;
      if (onlyTargeting.value) {
        if (!f.mouseOver) break;
        jobject t = env->GetObjectField(f.mouseOver, id.mopEntity);
        bool has = t != nullptr;
        if (t) env->DeleteLocalRef(t);
        if (!has) break;
      }
      if (chanceLeft.value < 100.0) {
        double roll = std::uniform_real_distribution<double>(0.0, 1.0)(t_rng);
        if (roll >= chanceLeft.value / 100.0) break;
      }
      mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
      mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
      mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
      ignoreUntilMs_ = now + 30;
    } else if (edgeR && rightClick.value && chanceRight.value > 0.0) {
      if (blocksOnly.value) {
        jobject held = env->CallObjectMethod(f.player, id.lGetHeld);
        jobject item = (held && !env->ExceptionCheck())
                           ? env->CallObjectMethod(held, id.stGetItem)
                           : nullptr;
        bool block = item && env->IsInstanceOf(item, id.itemblock);
        if (item) env->DeleteLocalRef(item);
        if (held) env->DeleteLocalRef(held);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (!block) break;
      }
      if (chanceRight.value < 100.0) {
        double roll = std::uniform_real_distribution<double>(0.0, 1.0)(t_rng);
        if (roll >= chanceRight.value / 100.0) break;
      }
      mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
      mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
      mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
      ignoreUntilMs_ = now + 30;
    }
  } while (false);

  mc.ReleaseFrame(f);
}

}  // namespace modules
}  // namespace raven
