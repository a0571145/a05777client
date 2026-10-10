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
#include <cstdint>
#include <string>
#include <thread>

// Embedded REST + web panel for a05777Client. Listens on 127.0.0.1:8080.
// No third-party deps: plain WinSock HTTP/1.1, hand-rolled JSON.
//
//   GET  /                        -> web panel (UI.h)
//   GET  /api/status              -> {name, game, running, modules, enabled, frames}
//   GET  /api/modules             -> full detail list (name/category/enabled/bind/bools/sliders/modes)
//   GET  /api/modules/{name}      -> one module detail
//   POST /api/modules/{name}/toggle
//   PUT  /api/modules/{name}      -> JSON body:
//        {enabled?, bind?, bools?{name:bool}, sliders?{name:num}, modes?{name:index|option}}
//   GET  /api/debug/mc            -> live JVM snapshot (Ensure status,
//                                     in-game flags, player pos, entity count)
//   GET  /api/debug/esp            -> boxes + camera + overlay diag
//   POST /api/debug/overlay/flash  -> red test pattern for 5s (window-path check)
namespace raven { namespace core { class ModuleManager; } }

namespace raven {
namespace web {

class RestServer {
public:
  static RestServer& Instance();

  bool Start(core::ModuleManager* modules, uint16_t port = 8080);
  void Stop();
  bool IsRunning() const { return running_.load(); }
  uint16_t Port() const { return port_; }

private:
  RestServer() = default;
  void Loop();
  void HandleConn(uintptr_t sock);
  std::string Dispatch(const std::string& method, const std::string& target,
                       const std::string& body);

  std::atomic<bool> running_{false};
  core::ModuleManager* modules_ = nullptr;
  uint16_t port_ = 8080;
  uintptr_t listen_ = 0;
  bool wsa_ = false;
  std::thread thread_;
};

}  // namespace web
}  // namespace raven
