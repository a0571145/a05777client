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

#include "modules/PlayerESP.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <jni.h>
#include "hooks/Hooks.h"
#include "render/OverlayWindow.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"
#include "util/Logger.h"

namespace raven {
namespace modules {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = 0.017453292519943295;
constexpr float kEyeHeight = 1.62f;  // standing; sneak crouch (~1.54) ignored

struct Cam {
  double ex, ey, ez;
  double yaw, pitch, fovV;
  int w, h;
};

// Project world -> screen using the exact vanilla Entity.getLook basis.
// Returns false when behind the camera.
bool Project(const Cam& c, double x, double y, double z, float& sx, float& sy) {
  double f = std::cos(-c.yaw * kDeg - kPi);
  double f1 = std::sin(-c.yaw * kDeg - kPi);
  double f2 = -std::cos(-c.pitch * kDeg);
  double f3 = std::sin(-c.pitch * kDeg);
  double lx = f1 * f2, ly = f3, lz = f * f2;
  // right = normalize(look x up(0,1,0)) = normalize(-lz, 0, lx)
  double rl = std::sqrt(lz * lz + lx * lx);
  if (rl < 1e-9) return false;
  double rx = -lz / rl, rz = lx / rl;
  // up2 = right x look, with right=(rx,0,rz): (-rz*ly, rz*lx - rx*lz, rx*ly)
  double ux = -rz * ly;
  double uy = rz * lx - rx * lz;
  double uz = rx * ly;
  double dx = x - c.ex, dy = y - c.ey, dz = z - c.ez;
  double vx = dx * rx + dz * rz;
  double vy = dx * ux + dy * uy + dz * uz;
  // Depth along the look vector: positive = in front of the camera.
  // (Was negated before 2026-09-30, which showed boxes only when facing AWAY.)
  double vz = dx * lx + dy * ly + dz * lz;
  if (vz < 0.05) return false;
  double tanV = std::tan(c.fovV * kDeg * 0.5);
  if (tanV <= 0.0) return false;
  double aspect = static_cast<double>(c.w) / static_cast<double>(c.h);
  sx = static_cast<float>(c.w * 0.5 + (vx / vz) * (c.w * 0.5) / (tanV * aspect));
  sy = static_cast<float>(c.h * 0.5 - (vy / vz) * (c.h * 0.5) / tanV);
  return true;
}

uint32_t HsvToRgb(double h, double s, double v) {
  h = h - std::floor(h);
  double c = v * s, x = c * (1.0 - std::fabs(std::fmod(h * 6.0, 2.0) - 1.0));
  double m = v - c, r = 0, g = 0, b = 0;
  int i = static_cast<int>(h * 6.0) % 6;
  switch (i) {
    case 0: r = c; g = x; break;
    case 1: r = x; g = c; break;
    case 2: g = c; b = x; break;
    case 3: g = x; b = c; break;
    case 4: r = x; b = c; break;
    default: r = c; b = x; break;
  }
  return (static_cast<uint32_t>((r + m) * 255.0) << 16) |
         (static_cast<uint32_t>((g + m) * 255.0) << 8) |
         static_cast<uint32_t>((b + m) * 255.0);
}
}  // namespace

PlayerESP::PlayerESP() : core::Module("PlayerESP", core::Category::Render, 0) {
  RegisterBool(&box);
  RegisterBool(&healthBar);
  RegisterColor(&color);
  RegisterBool(&rainbow);
  RegisterBool(&renderSelf);
  RegisterBool(&showInvis);
  RegisterSlider(&maxDistance);
  RegisterSlider(&lineWidth);
}

void PlayerESP::OnEnable() {
  lastCollect_ = std::chrono::steady_clock::time_point{};
  if (!render::OverlayWindow::Instance().Start(hooks::Hooks::Instance().Window())) {
    util::Logger::Warn("PlayerESP: overlay not started (no MC window yet?)");
  }
}

void PlayerESP::OnDisable() {
  render::OverlayWindow::Instance().Stop();
}

void PlayerESP::OnUpdate() {
  // Overlay may not have existed at enable time (game window late).
  // Refresh the cached game window (see Hud.cpp).
  if (!render::OverlayWindow::Instance().IsRunning()) {
    HWND fresh = hooks::Hooks::Instance().RefreshWindow();
    if (fresh) render::OverlayWindow::Instance().Start(fresh);
  }
  if (!box.value && !healthBar.value) {
    render::OverlayWindow::Instance().Clear();
    return;
  }
  auto now = std::chrono::steady_clock::now();
  if (now - lastCollect_ < std::chrono::milliseconds(50)) return;
  lastCollect_ = now;

  JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
  if (!env) return;
  sdk::Mc& mc = sdk::Mc::Instance();
  if (!mc.Ensure(env)) return;
  if (!mc.EnsureExtra(env)) return;  // world entity list
  sdk::Mc::Frame f;
  if (!mc.BeginFrame(env, f)) return;
  auto& id = mc.Ids();

  std::vector<render::EspBox> boxes;
  do {
    if (!f.player || !f.world || !f.renderMgr) {
      if (hadWorld_) {
        hadWorld_ = false;
        render::OverlayWindow::Instance().Clear();
      }
      break;
    }
    hadWorld_ = true;
    if (f.displayW < 50 || f.displayH < 50) break;

    double selfX = sdk::Mc::D(env, f.player, id.ePosX);
    double selfY = sdk::Mc::D(env, f.player, id.ePosY);
    double selfZ = sdk::Mc::D(env, f.player, id.ePosZ);
    float yaw = sdk::Mc::Fl(env, f.player, id.eYaw);
    float pitch = sdk::Mc::Fl(env, f.player, id.ePitch);
    bool sprinting = env->CallBooleanMethod(f.player, id.eSprinting);
    jobject caps = sdk::Mc::Obj(env, f.player, id.pCaps);
    bool flying = caps && env->GetBooleanField(caps, id.cFlying);
    if (caps) env->DeleteLocalRef(caps);
    if (env->ExceptionCheck()) env->ExceptionClear();

    // Approximate vanilla dynamic FOV (sprint/fly modifiers).
    double fovEff = f.fov;
    if (sprinting) fovEff *= 1.15;
    if (flying) fovEff *= 1.10;

    jboolean sneaking = env->CallBooleanMethod(f.player, id.isSneakingM);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      sneaking = JNI_FALSE;
    }
    Cam cam{selfX, selfY + (sneaking ? 1.54 : kEyeHeight), selfZ, yaw, pitch, fovEff,
            f.displayW, f.displayH};
    double maxD = maxDistance.value;

    uint32_t baseColor =
        (static_cast<uint32_t>(color.r) << 16) | (static_cast<uint32_t>(color.g) << 8) |
        static_cast<uint32_t>(color.b);
    if (rainbow.value) {
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch())
                    .count();
      baseColor = HsvToRgb((ms % 5000) / 5000.0, 1.0, 1.0);
    }

    mc.EachPlayer(f, [&](jobject ent) {
      if (!renderSelf.value && sdk::Mc::Same(env, ent, f.player)) return;
      if (!showInvis.value && env->CallBooleanMethod(ent, id.eInvisible)) {
        if (!env->ExceptionCheck()) return;
        env->ExceptionClear();
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return;
      }
      double px = sdk::Mc::D(env, ent, id.ePosX);
      double py = sdk::Mc::D(env, ent, id.ePosY);
      double pz = sdk::Mc::D(env, ent, id.ePosZ);
      double dx = px - selfX, dy = py - selfY, dz = pz - selfZ;
      if (dx * dx + dy * dy + dz * dz > maxD * maxD) return;
      float w = sdk::Mc::Fl(env, ent, id.eWidth);
      float h = sdk::Mc::Fl(env, ent, id.eHeight);
      if (w <= 0.f || h <= 0.f) return;
      float hp = env->CallFloatMethod(ent, id.lGetHealth);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        hp = 20.f;
      }

      double lx = sdk::Mc::D(env, ent, id.eLastX);
      double ly = sdk::Mc::D(env, ent, id.eLastY);
      double lz = sdk::Mc::D(env, ent, id.eLastZ);
      double ix = lx + (px - lx) * f.partialTicks;
      double iy = ly + (py - ly) * f.partialTicks;
      double iz = lz + (pz - lz) * f.partialTicks;

      float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
      bool any = false;
      for (int cx = -1; cx <= 1; cx += 2) {
        for (int cz = -1; cz <= 1; cz += 2) {
          for (int cy = 0; cy <= 1; cy++) {
            float sx = 0, sy = 0;
            if (!Project(cam, ix + cx * w * 0.5, iy + cy * h, iz + cz * w * 0.5, sx,
                         sy)) {
              continue;
            }
            if (!any) {
              x0 = x1 = sx;
              y0 = y1 = sy;
              any = true;
            } else {
              x0 = std::min(x0, sx);
              y0 = std::min(y0, sy);
              x1 = std::max(x1, sx);
              y1 = std::max(y1, sy);
            }
          }
        }
      }
      if (!any || x1 - x0 < 2.f || y1 - y0 < 2.f) return;
      // Clamp near-screen; drop camera-inside absurd sizes.
      float bw = x1 - x0, bh = y1 - y0;
      if (bw > f.displayW * 1.5f || bh > f.displayH * 1.5f) return;
      if (x1 < -100.f || y1 < -100.f || x0 > f.displayW + 100.f ||
          y0 > f.displayH + 100.f) {
        return;
      }
      render::EspBox b;
      b.x = x0;
      b.y = y0;
      b.w = x1 - x0;
      b.h = y1 - y0;
      b.hp = hp / 20.f;
      b.color = baseColor;
      b.drawBar = healthBar.value;
      b.lineW = static_cast<int>(lineWidth.value + 0.5);
      boxes.push_back(b);
    });
    render::EspCamera ejeCam;
    ejeCam.ex = selfX;
    ejeCam.ey = selfY;
    ejeCam.ez = selfZ;
    ejeCam.yaw = yaw;
    ejeCam.pitch = pitch;
    ejeCam.fov = fovEff;
    ejeCam.w = f.displayW;
    ejeCam.h = f.displayH;
    render::OverlayWindow::Instance().SetCamera(ejeCam);
  } while (false);

  mc.ReleaseFrame(f);
  // Publish camera for /api/debug/esp before releasing locals.
  render::OverlayWindow::Instance().SetBoxes(std::move(boxes));
}

}  // namespace modules
}  // namespace raven
