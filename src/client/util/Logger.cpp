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

#include "util/Logger.h"
#include <cstdio>
#include <mutex>

namespace raven {
namespace util {

namespace {
std::FILE* g_file = nullptr;
std::mutex g_mutex;

const char* Tag(LogLevel l) {
  switch (l) {
    case LogLevel::Warn: return "[WARN]";
    case LogLevel::Error: return "[ERR] ";
    default: return "[INFO]";
  }
}
}  // namespace

void Logger::Init(const wchar_t* filePath) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (filePath && !g_file) {
    // Plain narrow append (no ccs flag): our content is ASCII, so the file
    // stays valid UTF-8, and narrow fputs/fflush remain legal. (A ccs=...
    // stream is wide-oriented; narrow writes on it trip UCRT invalid-parameter
    // -> Watson fastfail 0xC0000409.)
    _wfopen_s(&g_file, filePath, L"a");
  }
}

void Logger::Shutdown() {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_file) {
    std::fclose(g_file);
    g_file = nullptr;
  }
}

void Logger::Log(LogLevel level, const std::string& msg) {
  std::string line = std::string("[a05777Client] ") + Tag(level) + " " + msg + "\n";
  OutputDebugStringA(line.c_str());
  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_file) {
    std::fputs(line.c_str(), g_file);
    std::fflush(g_file);
  }
}

void Logger::Info(const std::string& msg) { Log(LogLevel::Info, msg); }
void Logger::Warn(const std::string& msg) { Log(LogLevel::Warn, msg); }
void Logger::Error(const std::string& msg) { Log(LogLevel::Error, msg); }

}  // namespace util
}  // namespace raven
