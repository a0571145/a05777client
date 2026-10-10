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

#include <windows.h>
#include "core/Client.h"

namespace {
DWORD WINAPI RunThread(LPVOID param) {
  raven::core::Client::Instance().Start(static_cast<HMODULE>(param));
  return 0;
}
}  // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*reserved*/) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      DisableThreadLibraryCalls(hModule);
      // Never block under loader lock: hand off to a worker thread.
      if (HANDLE t = CreateThread(nullptr, 0, RunThread, hModule, 0, nullptr)) {
        CloseHandle(t);
      }
      break;
    case DLL_PROCESS_DETACH:
      raven::core::Client::Instance().Stop();
      break;
    default:
      break;
  }
  return TRUE;
}
