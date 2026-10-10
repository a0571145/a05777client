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

// Dev-only end-to-end smoke test for a05777Client.
// Usage: smoke.exe <full-path-to-a05777Client.dll>
// Loads the DLL (runs DllMain -> REST on 127.0.0.1:8080), polls the port,
// then exercises the API with raw WinSock HTTP (no proxy, no deps).
// Exit 0 = all checks passed.
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdio>
#include <string>

namespace {

bool SendAll(SOCKET s, const char* p, size_t n) {
  size_t done = 0;
  while (done < n) {
    int r = ::send(s, p + done, static_cast<int>(n - done), 0);
    if (r <= 0) return false;
    done += static_cast<size_t>(r);
  }
  return true;
}

std::string Http(const std::string& method, const std::string& path,
                 const std::string& body = "") {
  SOCKET c = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (c == INVALID_SOCKET) return "";
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = htons(8080);
  if (::connect(c, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0) {
    ::closesocket(c);
    return "";
  }
  std::string req = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                    "Connection: close\r\n";
  if (!body.empty()) {
    req += "Content-Type: application/json\r\nContent-Length: " +
           std::to_string(body.size()) + "\r\n\r\n" + body;
  } else {
    req += "\r\n";
  }
  if (!SendAll(c, req.data(), req.size())) {
    ::closesocket(c);
    return "";
  }
  std::string resp;
  char buf[8192];
  for (;;) {
    int n = ::recv(c, buf, sizeof(buf), 0);
    if (n <= 0) break;
    resp.append(buf, static_cast<size_t>(n));
  }
  ::closesocket(c);
  size_t p = resp.find("\r\n\r\n");
  return p == std::string::npos ? resp : resp.substr(p + 4);
}

bool WaitPort(int tries = 50) {
  for (int i = 0; i < tries; ++i) {
    SOCKET c = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(8080);
    bool ok = ::connect(c, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0;
    ::closesocket(c);
    if (ok) return true;
    ::Sleep(200);
  }
  return false;
}

int Fail(const char* msg) {
  std::printf("[SMOKE-FAIL] %s\n", msg);
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  ::setvbuf(stdout, nullptr, _IONBF, 0);  // unbuffered: progress survives hangs/crashes
  if (argc < 2 || argc > 3) {
    std::printf("usage: smoke.exe <a05777Client.dll> [loadonly]\n");
    return 2;
  }
  WSADATA wd{};
  if (WSAStartup(MAKEWORD(2, 2), &wd) != 0) return Fail("WSAStartup");

  wchar_t wpath[MAX_PATH];
  if (MultiByteToWideChar(CP_ACP, 0, argv[1], -1, wpath, MAX_PATH) == 0) {
    return Fail("path convert");
  }
  HMODULE dll = LoadLibraryW(wpath);
  if (!dll) {
    std::printf("[SMOKE-FAIL] LoadLibrary err=%lu\n", GetLastError());
    return 1;
  }
  std::printf("[SMOKE] dll loaded, waiting for 127.0.0.1:8080 ...\n");
  if (argc == 3 && std::string(argv[2]) == "loadonly") {
    std::printf("[SMOKE] loadonly: sleeping 4s (no connections) ...\n");
    ::Sleep(4000);
    std::printf("[SMOKE] loadonly: unloading ...\n");
    FreeLibrary(dll);
    std::printf("[SMOKE] loadonly: unloaded clean\n");
    WSACleanup();
    return 0;
  }
  if (!WaitPort()) {
    FreeLibrary(dll);
    return Fail("port 8080 never opened");
  }

  std::string status = Http("GET", "/api/status");
  std::printf("[SMOKE] status: %s\n", status.c_str());
  if (status.find("a05777Client") == std::string::npos) {
    FreeLibrary(dll);
    return Fail("bad /api/status");
  }

  std::string mods = Http("GET", "/api/modules");
  std::printf("[SMOKE] modules: %s\n", mods.c_str());
  const char* want[] = {"\"Sprint\"",   "\"HUD\"",      "\"Velocity\"",
                         "\"NoSlow\"",   "\"Auto Block\"", "\"Auto Clicker\"", "\"Keep Sprint\"",
                        "\"Fullbright\"", "\"PlayerESP\"", "\"Reach\"",
                        "\"Hitboxes\"", "\"Kill Aura\"", "\"Fly\"", "\"Speed\"",
                        "\"NoFall\"", "\"SafeWalk\"", "\"BHop\"", "\"WTap\"",
                        "\"Jump Reset\"", "\"ClickAssist\"", "\"Reduce\"",
                        "\"Aim Assist\"", "\"RodAimbot\"", "\"Piercing\"",
                         "\"TPAura\"", "\"Raw Input\"", "\"AntiAFK\"",
                         "\"Auto Tool\"", "\"Fast Place\"", "\"Auto Jump\"",
                         "\"Bridge Assist\"", "\"Auto Swap\""};
  for (const char* w : want) {
    if (mods.find(w) == std::string::npos) {
      FreeLibrary(dll);
      return Fail("module list incomplete");
    }
  }

  std::string toggled = Http("POST", "/api/modules/Sprint/toggle");
  std::printf("[SMOKE] toggle: %s\n", toggled.c_str());
  if (toggled.find("\"enabled\":true") == std::string::npos) {
    FreeLibrary(dll);
    return Fail("toggle did not enable Sprint");
  }

  // Bind registry path (the in-game hotkey poll itself needs a real game
  // window + keypress, so only the PUT echo is asserted headless).
  std::string bindPut = Http("PUT", "/api/modules/Sprint", "{\"bind\":82}");
  std::printf("[SMOKE] bind put: %s\n", bindPut.c_str());
  if (bindPut.find("\"bind\":82") == std::string::npos) {
    FreeLibrary(dll);
    return Fail("bind PUT not applied");
  }
  Http("PUT", "/api/modules/Sprint", "{\"bind\":0}");

  std::string put = Http("PUT", "/api/modules/HUD",
                          "{\"sliders\":{\"Scale\":1.5,\"Pos X\":16},"
                          "\"bools\":{\"Draw background\":false},"
                          "\"modes\":{\"Outline\":2},"
                          "\"colors\":{\"Color\":\"#00ff00\"}}");
  std::printf("[SMOKE] put: %s\n", put.c_str());
  if (put.find("\"value\":1.5") == std::string::npos ||
      put.find("\"hex\":\"#00ff00\"") == std::string::npos ||
      put.find("\"index\":2") == std::string::npos) {
    FreeLibrary(dll);
    return Fail("HUD PUT not applied");
  }

  std::string modePut =
      Http("PUT", "/api/modules/NoSlow", "{\"modes\":{\"Mode\":1}}");
  std::printf("[SMOKE] mode put: %s\n", modePut.c_str());
  if (modePut.find("\"index\":1") == std::string::npos) {
    FreeLibrary(dll);
    return Fail("mode PUT not applied");
  }

  std::string colorPut = Http("PUT", "/api/modules/PlayerESP",
                              "{\"colors\":{\"Color\":\"#ff0000\"},"
                              "\"sliders\":{\"Line width\":2}}");
  std::printf("[SMOKE] color put: %s\n", colorPut.c_str());
  if (colorPut.find("\"hex\":\"#ff0000\"") == std::string::npos ||
      colorPut.find("\"value\":2") == std::string::npos) {
    FreeLibrary(dll);
    return Fail("color PUT not applied");
  }

  std::string blockPut = Http("PUT", "/api/modules/Auto%20Block",
                              "{\"sliders\":{\"Range\":5.0,"
                              "\"Maximum Hurt Time\":300,"
                              "\"Maximum Hold Time\":250,"
                              "\"Lag Chance\":75,"
                              "\"Lag Max Duration\":350},"
                              "\"bools\":{\"Prevent delaying attacks\":false,"
                              "\"Block again immediately\":false,"
                              "\"Force block animation\":false,"
                              "\"Require Left mouse\":false,"
                              "\"Require right mouse\":true,"
                              "\"Damaged\":true,"
                              "\"Ignore teammates\":false}}");
  std::printf("[SMOKE] autoblock put: %s\n", blockPut.c_str());
  const char* autoBlockSettings[] = {
      "\"name\":\"Range\",\"value\":5",
      "\"name\":\"Maximum Hurt Time\",\"value\":300",
      "\"name\":\"Maximum Hold Time\",\"value\":250",
      "\"name\":\"Lag Chance\",\"value\":75",
      "\"name\":\"Lag Max Duration\",\"value\":350",
      "\"name\":\"Prevent delaying attacks\",\"value\":false",
      "\"name\":\"Block again immediately\",\"value\":false",
      "\"name\":\"Force block animation\",\"value\":false",
      "\"name\":\"Require Left mouse\",\"value\":false",
      "\"name\":\"Require right mouse\",\"value\":true",
      "\"name\":\"Damaged\",\"value\":true",
      "\"name\":\"Ignore teammates\",\"value\":false"};
  for (const char* expected : autoBlockSettings) {
    if (blockPut.find(expected) == std::string::npos) {
      FreeLibrary(dll);
      return Fail("Auto Block settings PUT not applied");
    }
  }

  std::string home = Http("GET", "/");
  std::printf("[SMOKE] home bytes=%zu title=%s\n", home.size(),
              home.find("<title>a05777Client</title>") != std::string::npos ? "ok"
                                                                            : "MISSING");
  if (home.find("<title>a05777Client</title>") == std::string::npos) {
    FreeLibrary(dll);
    return Fail("home page missing");
  }

  // Toggle Sprint back off to leave clean state.
  Http("POST", "/api/modules/Sprint/toggle");
  FreeLibrary(dll);
  WSACleanup();
  std::printf("[SMOKE] ALL CHECKS PASSED\n");
  return 0;
}
