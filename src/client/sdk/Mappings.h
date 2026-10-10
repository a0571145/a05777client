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
// MCP 1.8.9 mappings used by the DLL. Single place where obfuscated
// (searge) names live; everything else talks through Jvm/Minecraft helper.
//
// Vanilla 1.8.9 (obfuscated) -> MCP snapshot names of interest:
//   net.minecraft.client.Minecraft:
//     field_71439_g (thePlayer : EntityPlayerSP)
//     field_71441_e (theWorld  : WorldClient)
//     field_71474_y (gameSettings)
//     func_71410_x() getMinecraft()
//   EntityPlayerSP: field_70173_aa (ticksExisted), func_110143_aJ (getHealth)
//   GameSettings: field_151444_V (keyBindSprint)
// Full JNI caching lands in sdk/Jvm.* in Phase 2; scaffold compiles without a JVM.
namespace raven {
namespace sdk {

inline constexpr const char* kMcClass = "net/minecraft/client/Minecraft";
inline constexpr const char* kGetMinecraft = "func_71410_x";
inline constexpr const char* kFieldThePlayer = "field_71439_g";
inline constexpr const char* kFieldTheWorld = "field_71441_e";
inline constexpr const char* kFieldGameSettings = "field_71474_y";

}  // namespace sdk
}  // namespace raven
