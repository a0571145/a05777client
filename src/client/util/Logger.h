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
#include <windows.h>
#include <string>

// Minimal logger: OutputDebugString + optional file. No deps.
namespace raven {
namespace util {

enum class LogLevel { Info, Warn, Error };

class Logger {
public:
  static void Init(const wchar_t* filePath = nullptr);
  static void Shutdown();
  static void Log(LogLevel level, const std::string& msg);
  static void Info(const std::string& msg);
  static void Warn(const std::string& msg);
  static void Error(const std::string& msg);
};

}  // namespace util
}  // namespace raven
