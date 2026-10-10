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

// a05777Client injector: auto-detects the running game, no manual pid needed.
//
//   injector.exe                       -> auto-detect + inject (DLL next to exe)
//   injector.exe --list                -> show detected game processes
//   injector.exe --all [dll]           -> inject every detected game
//   injector.exe --pid <pid> [dll]     -> inject explicit pid
//   injector.exe <pid|javaw.exe> [dll] -> legacy explicit target
//
// Detection: top-level windows whose title mentions Minecraft (or LWJGL
// class) AND whose process image is java/javaw. Falls back to any
// java/javaw process when no window matches (e.g. custom titles).
// Targets must be 64-bit (our DLL is x64); 32-bit games are refused
// with a clear message.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdio>
#include <string>
#include <tlhelp32.h>
#include <vector>

namespace {

struct Candidate {
  DWORD pid = 0;
  std::wstring title;
  std::wstring exe;
  bool byWindow = false;
  bool hasGl = false;  // LWJGL module loaded => a real running game
};

std::wstring ExeBaseName(DWORD pid) {
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h) return L"?";
  wchar_t path[MAX_PATH] = {};
  DWORD n = MAX_PATH;
  std::wstring base = L"?";
  if (QueryFullProcessImageNameW(h, 0, path, &n)) {
    std::wstring p = path;
    size_t s = p.find_last_of(L"\\/");
    base = (s == std::wstring::npos) ? p : p.substr(s + 1);
  }
  CloseHandle(h);
  return base;
}

bool IsJavaImage(const std::wstring& exe) {
  return _wcsicmp(exe.c_str(), L"javaw.exe") == 0 ||
         _wcsicmp(exe.c_str(), L"java.exe") == 0;
}

bool HasLwjgl(DWORD pid);  // defined below (needs module snapshot)

BOOL CALLBACK EnumCb(HWND w, LPARAM lp) {
  auto* out = reinterpret_cast<std::vector<Candidate>*>(lp);
  if (!IsWindowVisible(w)) return TRUE;
  DWORD pid = 0;
  GetWindowThreadProcessId(w, &pid);
  if (!pid) return TRUE;
  wchar_t title[256] = {};
  GetWindowTextW(w, title, 256);
  wchar_t cls[64] = {};
  GetClassNameW(w, cls, 64);
  std::wstring t = title;
  bool mcTitle = t.find(L"Minecraft") != std::wstring::npos ||
                 t.find(L"minecraft") != std::wstring::npos;
  bool lwjgl = _wcsicmp(cls, L"LWJGL") == 0;
  if (!mcTitle && !lwjgl) return TRUE;
  std::wstring exe = ExeBaseName(pid);
  if (!IsJavaImage(exe)) return TRUE;
  for (const auto& c : *out) {
    if (c.pid == pid) return TRUE;
  }
  out->push_back({pid, t.empty() ? L"(no title)" : t, exe, true, false});
  return TRUE;
}

std::vector<Candidate> DetectGames() {
  std::vector<Candidate> found;
  EnumWindows(EnumCb, reinterpret_cast<LPARAM>(&found));
  if (!found.empty()) {
    for (auto& c : found) c.hasGl = HasLwjgl(c.pid);
    return found;
  }
  // Fallback: any java/javaw process (custom window titles).
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return found;
  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  if (Process32FirstW(snap, &pe)) {
    do {
      if (IsJavaImage(pe.szExeFile)) {
        Candidate c{pe.th32ProcessID, L"(no window match)", pe.szExeFile, false, false};
        c.hasGl = HasLwjgl(c.pid);
        found.push_back(c);
      }
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return found;
}

bool IsTarget64Bit(HANDLE proc, bool& out64) {
  // On 64-bit Windows, WOW64 processes are 32-bit.
  BOOL wow = FALSE;
  using Fn = BOOL(WINAPI*)(HANDLE, PBOOL);
  auto isWow = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "IsWow64Process"));
  if (!isWow) return false;
  if (!isWow(proc, &wow)) return false;
#if defined(_WIN64)
  out64 = !wow;
#else
  out64 = false;
#endif
  return true;
}

// True when the process has LWJGL loaded: a real running game, not the
// launcher bootstrap or an idle java process.
bool HasLwjgl(DWORD pid) {
  HANDLE snap =
      CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
  if (snap == INVALID_HANDLE_VALUE) return false;
  bool found = false;
  MODULEENTRY32W me{};
  me.dwSize = sizeof(me);
  if (Module32FirstW(snap, &me)) {
    do {
      std::wstring n = me.szModule;
      for (auto& ch : n) ch = towlower(ch);
      if (n == L"lwjgl.dll" || n == L"lwjgl64.dll") {
        found = true;
        break;
      }
    } while (Module32NextW(snap, &me));
  }
  CloseHandle(snap);
  return found;
}

// Polls 127.0.0.1:8080 until OUR status answers (raw socket, no proxy).
bool PanelUp(int timeoutMs) {
  WSADATA wd{};
  if (WSAStartup(MAKEWORD(2, 2), &wd) != 0) return false;
  bool ok = false;
  unsigned long long end = GetTickCount64() + static_cast<unsigned long long>(timeoutMs);
  while (GetTickCount64() < end && !ok) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s != INVALID_SOCKET) {
      sockaddr_in a{};
      a.sin_family = AF_INET;
      a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      a.sin_port = htons(8080);
      u_long nb = 1;
      ioctlsocket(s, FIONBIO, &nb);
      connect(s, reinterpret_cast<sockaddr*>(&a), sizeof(a));
      fd_set wf;
      FD_ZERO(&wf);
      FD_SET(s, &wf);
      timeval tv{0, 300000};
      if (select(0, nullptr, &wf, nullptr, &tv) > 0) {
        const char* req = "GET /api/status HTTP/1.0\r\nHost: x\r\n\r\n";
        if (send(s, req, static_cast<int>(strlen(req)), 0) > 0) {
          timeval rtv{1, 0};
          setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&rtv),
                     sizeof(rtv));
          std::string acc;
          char buf[2048];
          int n = 0;
          while ((n = recv(s, buf, sizeof(buf) - 1, 0)) > 0) {
            buf[n] = 0;
            acc += buf;
          }
          if (acc.find("a05777Client") != std::string::npos) ok = true;
        }
      }
      closesocket(s);
    }
    if (!ok) Sleep(500);
  }
  WSACleanup();
  return ok;
}

bool AlreadyLoaded(DWORD pid) {
  HANDLE snap =
      CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
  if (snap == INVALID_HANDLE_VALUE) return false;
  bool found = false;
  MODULEENTRY32W me{};
  me.dwSize = sizeof(me);
  if (Module32FirstW(snap, &me)) {
    do {
      std::wstring name = me.szModule;
      if (_wcsicmp(name.c_str(), L"a05777Client.dll") == 0 ||
          (name.size() > 6 && _wcsnicmp(name.c_str(), L"a05777", 6) == 0)) {
        found = true;
        break;
      }
    } while (Module32NextW(snap, &me));
  }
  CloseHandle(snap);
  return found;
}

bool InjectOne(DWORD pid, const std::wstring& dll) {
  if (AlreadyLoaded(pid)) {
    std::wprintf(L"[=] pid %lu: already injected, skipping (use --eject first to reload)\n",
                 pid);
    return true;
  }
  HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                            FALSE, pid);
  if (!proc) {
    std::fwprintf(stderr, L"[!] pid %lu: OpenProcess failed: %lu (try admin?)\n", pid,
                  GetLastError());
    return false;
  }
  bool is64 = false;
  if (IsTarget64Bit(proc, is64) && !is64) {
    std::fwprintf(stderr, L"[!] pid %lu: 32-bit game, need 64-bit java for x64 DLL\n", pid);
    CloseHandle(proc);
    return false;
  }

  size_t bytes = (dll.size() + 1) * sizeof(wchar_t);
  void* remote = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!remote) {
    std::fwprintf(stderr, L"[!] pid %lu: VirtualAllocEx failed: %lu\n", pid, GetLastError());
    CloseHandle(proc);
    return false;
  }
  if (!WriteProcessMemory(proc, remote, dll.c_str(), bytes, nullptr)) {
    std::fwprintf(stderr, L"[!] pid %lu: WriteProcessMemory failed: %lu\n", pid,
                  GetLastError());
    CloseHandle(proc);
    return false;
  }

  auto loadLib = reinterpret_cast<LPTHREAD_START_ROUTINE>(
      GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
  HANDLE thread = CreateRemoteThread(proc, nullptr, 0, loadLib, remote, 0, nullptr);
  if (!thread) {
    std::fwprintf(stderr, L"[!] pid %lu: CreateRemoteThread failed: %lu\n", pid,
                  GetLastError());
    CloseHandle(proc);
    return false;
  }
  WaitForSingleObject(thread, INFINITE);
  DWORD exitCode = 0;
  GetExitCodeThread(thread, &exitCode);
  CloseHandle(thread);
  CloseHandle(proc);

  if (!exitCode) {
    std::fwprintf(stderr, L"[!] pid %lu: remote LoadLibraryW failed\n", pid);
    return false;
  }
  std::wprintf(L"[+] pid %lu: a05777Client injected (base 0x%08lX), panel http://127.0.0.1:8080/\n",
               pid, exitCode);
  return true;
}

bool EjectOne(DWORD pid) {
  HANDLE snap =
      CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
  if (snap == INVALID_HANDLE_VALUE) {
    std::fwprintf(stderr, L"[!] pid %lu: module snapshot failed: %lu\n", pid,
                  GetLastError());
    return false;
  }
  HMODULE target = nullptr;
  std::wstring found;
  MODULEENTRY32W me{};
  me.dwSize = sizeof(me);
  if (Module32FirstW(snap, &me)) {
    do {
      std::wstring name = me.szModule;
      if (_wcsicmp(name.c_str(), L"a05777Client.dll") == 0 ||
          (name.size() > 6 && _wcsnicmp(name.c_str(), L"a05777", 6) == 0)) {
        target = me.hModule;
        found = name;
        break;
      }
    } while (Module32NextW(snap, &me));
  }
  CloseHandle(snap);
  if (!target) {
    std::fwprintf(stderr, L"[!] pid %lu: a05777Client.dll not loaded\n", pid);
    return false;
  }
  HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                            FALSE, pid);
  if (!proc) {
    std::fwprintf(stderr, L"[!] pid %lu: OpenProcess failed: %lu\n", pid,
                  GetLastError());
    return false;
  }
  auto freeLib = reinterpret_cast<LPTHREAD_START_ROUTINE>(
      GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "FreeLibrary"));
  HANDLE thread = CreateRemoteThread(proc, nullptr, 0, freeLib, target, 0, nullptr);
  if (!thread) {
    std::fwprintf(stderr, L"[!] pid %lu: CreateRemoteThread failed: %lu\n", pid,
                  GetLastError());
    CloseHandle(proc);
    return false;
  }
  DWORD wait = WaitForSingleObject(thread, 15000);
  DWORD exitCode = 0;
  GetExitCodeThread(thread, &exitCode);
  CloseHandle(thread);
  CloseHandle(proc);
  if (wait != WAIT_OBJECT_0 || !exitCode) {
    std::fwprintf(stderr, L"[!] pid %lu: remote FreeLibrary failed\n", pid);
    return false;
  }
  std::wprintf(L"[+] pid %lu: %s ejected, safe to re-inject\n", pid, found.c_str());
  return true;
}

std::wstring DefaultDllPath() {  wchar_t self[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, self, MAX_PATH);
  std::wstring p = self;
  size_t s = p.find_last_of(L"\\/");
  std::wstring dir = (s == std::wstring::npos) ? L"." : p.substr(0, s);
  return dir + L"\\a05777Client.dll";
}

void PrintList(const std::vector<Candidate>& v) {
  if (v.empty()) {
    std::wprintf(L"[-] no game found. Start Minecraft 1.8.9 first, then retry.\n");
    return;
  }
  std::wprintf(L"[*] detected %zu game candidate(s):\n", v.size());
  for (const auto& c : v) {
    std::wprintf(L"    pid=%-8lu %-12s %s %s%s\n", c.pid, c.exe.c_str(), c.title.c_str(),
                 c.byWindow ? L"[window]" : L"[process]", c.hasGl ? L"[lwjgl]" : L"");
  }
}

void Usage() {
  std::fwprintf(stderr,
                L"a05777Client injector (auto-detects the game)\n"
                L"  injector.exe                 auto-detect + inject\n"
                L"  injector.exe --list          show detected games\n"
                L"  injector.exe --all [dll]     inject every detected game\n"
                L"  injector.exe --pid <pid> [dll]\n"
                L"  injector.exe <pid|javaw.exe> [dll]\n"
                L"  injector.exe --eject [pid|javaw.exe]   unload the DLL (no game restart)\n"
                L"  [dll] defaults to a05777Client.dll next to injector.exe\n");
}

DWORD FindPidByName(const wchar_t* processName) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return 0;
  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  DWORD pid = 0;
  if (Process32FirstW(snap, &pe)) {
    do {
      if (_wcsicmp(pe.szExeFile, processName) == 0) {
        pid = pe.th32ProcessID;
        break;
      }
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return pid;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  std::wstring dll;
  std::vector<DWORD> targets;
  bool listOnly = false;

  if (argc >= 2 && _wcsicmp(argv[1], L"--list") == 0) {
    listOnly = true;
  } else if (argc >= 2 && _wcsicmp(argv[1], L"--all") == 0) {
    dll = (argc >= 3) ? argv[2] : DefaultDllPath();
    for (const auto& c : DetectGames()) targets.push_back(c.pid);
    if (targets.empty()) {
      std::fwprintf(stderr, L"[-] no game found. Start Minecraft 1.8.9 first.\n");
      return 1;
    }
  } else if (argc >= 2 && _wcsicmp(argv[1], L"--pid") == 0) {
    if (argc < 3) {
      Usage();
      return 2;
    }
    targets.push_back(static_cast<DWORD>(_wtoi(argv[2])));
    dll = (argc >= 4) ? argv[3] : DefaultDllPath();
  } else if (argc >= 2 && _wcsicmp(argv[1], L"--eject") == 0) {
    // Unload without restarting the game: --eject [pid|javaw.exe]
    if (argc >= 3) {
      DWORD pid = 0;
      if (DWORD v = static_cast<DWORD>(_wtoi(argv[2])); v != 0) {
        pid = v;
      } else {
        pid = FindPidByName(argv[2]);
      }
      if (!pid) {
        std::fwprintf(stderr, L"[!] process not found: %s\n", argv[2]);
        return 1;
      }
      targets.push_back(pid);
    } else {
      auto found = DetectGames();
      if (found.empty()) {
        std::fwprintf(stderr, L"[-] no game found.\n");
        return 1;
      }
      if (found.size() > 1) {
        PrintList(found);
        std::fwprintf(stderr, L"[!] multiple games: re-run with --eject <pid>\n");
        return 3;
      }
      targets.push_back(found[0].pid);
    }
    int ok = 0;
    for (DWORD pid : targets) {
      if (EjectOne(pid)) ok++;
    }
    return ok > 0 ? 0 : 1;
  } else if (argc == 1) {
    // Default: wait for the game if needed, pick the best candidate, inject.
    dll = DefaultDllPath();
    auto found = DetectGames();
    if (found.empty()) {
      std::wprintf(L"[*] no game yet - waiting up to 90s (launch Minecraft 1.8.9)...\n");
      for (int i = 0; i < 90 && found.empty(); ++i) {
        Sleep(1000);
        found = DetectGames();
      }
    }
    if (found.empty()) {
      std::fwprintf(stderr, L"[-] no game found. Launch Minecraft 1.8.9, then retry.\n");
      return 1;
    }
    // Prefer window match + LWJGL (a real running game over idle java).
    std::vector<Candidate> scored = found;
    if (scored.size() > 1) {
      std::vector<Candidate> strong;
      for (const auto& c : scored) {
        if (c.byWindow && c.hasGl) strong.push_back(c);
      }
      if (!strong.empty()) scored = strong;
    }
    if (scored.size() > 1) {
      // Prefer the classic vanilla title when it singles one out.
      std::vector<Candidate> exact;
      for (const auto& c : scored) {
        if (c.title == L"Minecraft 1.8.9") exact.push_back(c);
      }
      if (exact.size() == 1) scored = exact;
    }
    if (scored.size() > 1) {
      PrintList(found);
      std::fwprintf(stderr, L"[!] multiple games: re-run with --pid <pid> or --all\n");
      return 3;
    }
    std::wprintf(L"[*] target: pid=%lu %s \"%s\"%s\n", scored[0].pid,
                 scored[0].exe.c_str(), scored[0].title.c_str(),
                 scored[0].hasGl ? L" [lwjgl]" : L"");
    targets.push_back(scored[0].pid);
  } else if (argv[1][0] == L'-') {
    Usage();
    return 2;
  } else {
    // Legacy: <pid|javaw.exe> [dll]
    DWORD pid = 0;
    if (DWORD v = static_cast<DWORD>(_wtoi(argv[1])); v != 0) {
      pid = v;
    } else {
      pid = FindPidByName(argv[1]);
    }
    if (!pid) {
      std::fwprintf(stderr, L"[!] process not found: %s\n", argv[1]);
      return 1;
    }
    targets.push_back(pid);
    dll = (argc >= 3) ? argv[2] : DefaultDllPath();
  }

  if (listOnly) {
    PrintList(DetectGames());
    return 0;
  }

  if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
    std::fwprintf(stderr, L"[!] dll not found: %s\n", dll.c_str());
    return 1;
  }

  int ok = 0;
  for (DWORD pid : targets) {
    if (InjectOne(pid, dll)) ok++;
  }
  if (ok == 0) return 1;
  std::wprintf(L"[*] verifying panel http://127.0.0.1:8080/ ...\n");
  if (PanelUp(15000)) {
    std::wprintf(L"[+] panel is up. Open http://127.0.0.1:8080/ to tune.\n");
    return 0;
  }
  std::fwprintf(stderr,
                L"[!] injected but the panel is not answering yet.\n"
                L"    Game still starting? Wait 10s and refresh the page.\n"
                L"    Still dead? Run --eject, then inject again.\n");
  return 4;
}
