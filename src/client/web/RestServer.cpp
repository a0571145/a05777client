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

#include "web/RestServer.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include "core/ModuleManager.h"
#include "hooks/Hooks.h"
#include "render/Overlay.h"
#include "render/OverlayWindow.h"
#include "sdk/Jvm.h"
#include "sdk/Mc.h"
#include "util/Logger.h"
#include "web/UI.h"

namespace raven {
namespace web {

namespace {

constexpr const char* kName = "a05777Client";
constexpr const char* kGame = "1.8.9";
constexpr size_t kMaxHead = 64 * 1024;
constexpr size_t kMaxBody = 256 * 1024;

std::string UrlDecode(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (c == '+') {
      out.push_back(' ');
    } else if (c == '%' && i + 2 < s.size()) {
      unsigned v = 0;
      if (std::sscanf(s.c_str() + i + 1, "%2x", &v) == 1) {
        out.push_back(static_cast<char>(v));
        i += 2;
      } else {
        out.push_back(c);
      }
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::string JsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 2);
  for (char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char b[8];
          std::snprintf(b, sizeof(b), "\\u%04x", c);
          out += b;
        } else {
          out.push_back(c);
        }
    }
  }
  return out;
}

std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

bool SendAll(SOCKET s, const char* data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    int n = ::send(s, data + sent, static_cast<int>(len - sent), 0);
    if (n <= 0) return false;
    sent += static_cast<size_t>(n);
  }
  return true;
}

std::string Resp(int code, const char* codeText, const std::string& contentType,
                 const std::string& body, bool cors = true) {
  char head[512];
  std::snprintf(head, sizeof(head),
                "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                "Connection: close\r\n%s\r\n",
                code, codeText, contentType.c_str(), body.size(),
                cors ? "Access-Control-Allow-Origin: *\r\n" : "");
  return std::string(head) + body;
}

// --- module JSON (caller must hold modules->Mutex()) ---

bool ParseHexColor(const std::string& s, int& r, int& g, int& b);
std::string ToHex(int r, int g, int b);

std::string ModuleDetailJson(core::Module* m) {
  std::string j = "{\"name\":\"" + JsonEscape(m->Name()) + "\",\"category\":\"" +
                  core::CategoryName(m->GetCategory()) + "\",\"enabled\":" +
                  (m->IsEnabled() ? "true" : "false") + ",\"bind\":" +
                  std::to_string(m->Bind());
  j += ",\"bools\":[";
  bool first = true;
  for (auto* b : m->Bools()) {
    if (!first) j += ",";
    first = false;
    j += "{\"name\":\"" + JsonEscape(b->name) + "\",\"value\":" +
         (b->value ? "true" : "false") + "}";
  }
  j += "],\"sliders\":[";
  first = true;
  for (auto* s : m->Sliders()) {
    if (!first) j += ",";
    first = false;
    char nums[160];
    std::snprintf(nums, sizeof(nums), "\"value\":%g,\"min\":%g,\"max\":%g,\"step\":%g",
                  s->value, s->min, s->max, s->step);
    j += "{\"name\":\"" + JsonEscape(s->name) + "\"," + nums + "}";
  }
  j += "],\"modes\":[";
  first = true;
  for (auto* md : m->Modes()) {
    if (!first) j += ",";
    first = false;
    j += "{\"name\":\"" + JsonEscape(md->name) + "\",\"index\":" +
         std::to_string(md->index) + ",\"current\":\"" +
         JsonEscape(md->Current()) + "\",\"options\":[";
    for (size_t i = 0; i < md->options.size(); ++i) {
      if (i) j += ",";
      j += "\"" + JsonEscape(md->options[i]) + "\"";
    }
    j += "]}";
  }
  j += "],\"colors\":[";
  first = true;
  for (auto* cl : m->Colors()) {
    if (!first) j += ",";
    first = false;
    j += "{\"name\":\"" + JsonEscape(cl->name) + "\",\"r\":" + std::to_string(cl->r) +
         ",\"g\":" + std::to_string(cl->g) + ",\"b\":" + std::to_string(cl->b) +
         ",\"hex\":\"" + ToHex(cl->r, cl->g, cl->b) + "\"}";
  }
  return j + "]}";
}

core::Module* FindLocked(core::ModuleManager* mgr, const std::string& name) {
  for (auto& m : mgr->All()) {
    if (m->Name() == name) return m.get();
  }
  return nullptr;
}

// --- minimal JSON body field readers ("key" : value) ---

const char* FindField(const std::string& body, const std::string& key) {
  std::string pat = "\"" + key + "\"";
  size_t p = body.find(pat);
  if (p == std::string::npos) return nullptr;
  const char* c = body.c_str() + p + pat.size();
  while (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n') ++c;
  if (*c != ':') return nullptr;
  ++c;
  while (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n') ++c;
  return c;
}

bool BodyBool(const std::string& body, const std::string& key, bool& out) {
  const char* c = FindField(body, key);
  if (!c) return false;
  if (std::strncmp(c, "true", 4) == 0) { out = true; return true; }
  if (std::strncmp(c, "false", 5) == 0) { out = false; return true; }
  return false;
}

bool BodyNum(const std::string& body, const std::string& key, double& out) {
  const char* c = FindField(body, key);
  if (!c) return false;
  char* end = nullptr;
  double v = std::strtod(c, &end);
  if (end == c) return false;
  out = v;
  return true;
}

bool BodyStr(const std::string& body, const std::string& key, std::string& out) {  const char* c = FindField(body, key);
  if (!c || *c != '"') return false;
  ++c;
  std::string s;
  while (*c && *c != '"') {
    if (*c == '\\' && c[1]) {
      ++c;
      s.push_back(*c == 'n' ? '\n' : *c);
    } else {
      s.push_back(*c);
    }
    ++c;
  }
  out = s;
  return true;
}

bool ParseHexColor(const std::string& s, int& r, int& g, int& b) {
  if (s.size() != 7 || s[0] != '#') return false;
  unsigned v = 0;
  if (std::sscanf(s.c_str() + 1, "%06x", &v) != 1) return false;
  r = static_cast<int>((v >> 16) & 255);
  g = static_cast<int>((v >> 8) & 255);
  b = static_cast<int>(v & 255);
  return true;
}

std::string ToHex(int r, int g, int b) {
  r = std::clamp(r, 0, 255);
  g = std::clamp(g, 0, 255);
  b = std::clamp(b, 0, 255);
  char h[8];
  std::snprintf(h, sizeof(h), "#%02x%02x%02x", r, g, b);
  return h;
}

}  // namespace

RestServer& RestServer::Instance() {
  static RestServer inst;
  return inst;
}

bool RestServer::Start(core::ModuleManager* modules, uint16_t port) {
  if (running_.load()) return true;
  modules_ = modules;
  port_ = port;

  WSADATA wd{};
  if (WSAStartup(MAKEWORD(2, 2), &wd) != 0) {
    util::Logger::Error("RestServer: WSAStartup failed");
    return false;
  }
  wsa_ = true;

  SOCKET ls = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (ls == INVALID_SOCKET) {
    util::Logger::Error("RestServer: socket() failed");
    WSACleanup();
    wsa_ = false;
    return false;
  }
  BOOL yes = TRUE;
  ::setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes),
               sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // local only by design
  addr.sin_port = htons(port_);
  if (::bind(ls, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    util::Logger::Error("RestServer: bind 127.0.0.1:" + std::to_string(port_) +
                        " failed (port busy?) - web panel disabled, client keeps running");
    ::closesocket(ls);
    WSACleanup();
    wsa_ = false;
    return false;
  }
  if (::listen(ls, 16) != 0) {
    util::Logger::Error("RestServer: listen() failed");
    ::closesocket(ls);
    WSACleanup();
    wsa_ = false;
    return false;
  }

  listen_ = static_cast<uintptr_t>(ls);
  running_ = true;
  thread_ = std::thread(&RestServer::Loop, this);
  util::Logger::Info(std::string(kName) + " web panel: http://127.0.0.1:" +
                     std::to_string(port_) + "/");
  return true;
}

void RestServer::Stop() {
  if (!running_.exchange(false)) return;
  if (thread_.joinable()) thread_.join();
  if (listen_) {
    ::closesocket(static_cast<SOCKET>(listen_));
    listen_ = 0;
  }
  if (wsa_) {
    WSACleanup();
    wsa_ = false;
  }
}

void RestServer::Loop() {
  SOCKET ls = static_cast<SOCKET>(listen_);
  while (running_.load()) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(ls, &rfds);
    timeval tv{0, 200 * 1000};
    int n = ::select(0, &rfds, nullptr, nullptr, &tv);
    if (n <= 0) continue;
    SOCKET c = ::accept(ls, nullptr, nullptr);
    if (c != INVALID_SOCKET) {
      HandleConn(static_cast<uintptr_t>(c));
      ::closesocket(c);
    }
  }
}

void RestServer::HandleConn(uintptr_t sock) {
  SOCKET c = static_cast<SOCKET>(sock);
  std::string head;
  head.reserve(4096);
  char buf[4096];
  // Read until end of headers (or limit).
  while (head.size() < kMaxHead) {
    int n = ::recv(c, buf, sizeof(buf), 0);
    if (n <= 0) return;
    head.append(buf, static_cast<size_t>(n));
    if (head.find("\r\n\r\n") != std::string::npos) break;
  }
  size_t hEnd = head.find("\r\n\r\n");
  if (hEnd == std::string::npos) {
    std::string r = Resp(431, "Too Large", "text/plain", "headers too large");
    SendAll(c, r.data(), r.size());
    return;
  }

  // Request line: METHOD SP TARGET SP VERSION
  size_t eol = head.find("\r\n");
  std::string reqLine = head.substr(0, eol);
  size_t sp1 = reqLine.find(' ');
  size_t sp2 = reqLine.find(' ', sp1 + 1);
  if (sp1 == std::string::npos || sp2 == std::string::npos) return;
  std::string method = reqLine.substr(0, sp1);
  std::string target = reqLine.substr(sp1 + 1, sp2 - sp1 - 1);

  // Headers (only Content-Length matters).
  std::map<std::string, std::string> headers;
  size_t pos = eol + 2;
  while (pos < hEnd) {
    size_t nl = head.find("\r\n", pos);
    // NOTE: the last header's terminating CRLF starts exactly at hEnd
    // (it doubles as the first half of "\r\n\r\n"), so only break past it.
    if (nl == std::string::npos || nl > hEnd) break;
    std::string line = head.substr(pos, nl - pos);
    pos = nl + 2;
    size_t col = line.find(':');
    if (col == std::string::npos) continue;
    std::string k = Lower(line.substr(0, col));
    std::string v = line.substr(col + 1);
    size_t s = v.find_first_not_of(" \t");
    if (s != std::string::npos) v = v.substr(s);
    headers[k] = v;
  }

  size_t contentLen = 0;
  auto it = headers.find("content-length");
  if (it != headers.end()) {
    contentLen = static_cast<size_t>(std::strtoul(it->second.c_str(), nullptr, 10));
    if (contentLen > kMaxBody) {
      std::string r = Resp(413, "Too Large", "text/plain", "body too large");
      SendAll(c, r.data(), r.size());
      return;
    }
  }
  std::string body = head.substr(hEnd + 4);
  while (body.size() < contentLen) {
    int n = ::recv(c, buf, sizeof(buf), 0);
    if (n <= 0) break;
    body.append(buf, static_cast<size_t>(n));
  }
  if (body.size() > contentLen) body.resize(contentLen);

  std::string out = Dispatch(method, target, body);
  SendAll(c, out.data(), out.size());
}

std::string RestServer::Dispatch(const std::string& method, const std::string& target,
                                 const std::string& body) {
  std::string path = target;
  size_t q = path.find('?');
  if (q != std::string::npos) path = path.substr(0, q);

  if (method == "OPTIONS") {
    return "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\n"
           "Access-Control-Allow-Methods: GET, POST, PUT, OPTIONS\r\n"
           "Access-Control-Allow-Headers: Content-Type\r\n"
           "Content-Length: 0\r\nConnection: close\r\n\r\n";
  }

  if (method == "GET" && (path == "/" || path == "/index.html")) {
    return Resp(200, "OK", "text/html; charset=utf-8", kIndexHtml, false);
  }

  const std::string api = "/api/";
  if (path.compare(0, api.size(), api) != 0) {
    return Resp(404, "Not Found", "application/json", "{\"error\":\"not found\"}");
  }
  std::string rest = path.substr(api.size());  // e.g. "status", "modules", "modules/Sprint"

  // GET /api/status
  if (method == "GET" && rest == "status") {
    size_t total = 0, enabled = 0;
    if (modules_) {
      std::lock_guard<std::recursive_mutex> lock(modules_->Mutex());
      total = modules_->All().size();
      for (auto& m : modules_->All()) {
        if (m->IsEnabled()) ++enabled;
      }
    }
    uint64_t frames = render::Overlay::Instance().Frames();
    char b[256];
    std::snprintf(b, sizeof(b),
                  "{\"name\":\"%s\",\"game\":\"%s\",\"running\":true,"
                  "\"modules\":%zu,\"enabled\":%zu,\"frames\":%llu}",
                  kName, kGame, total, enabled,
                  static_cast<unsigned long long>(frames));
    return Resp(200, "OK", "application/json", b);
  }

  // GET /api/debug/mc (live JVM snapshot: mapping status + world state)
  if (method == "GET" && rest == "debug/mc") {
    JNIEnv* env = sdk::Jvm::Instance().EnvForCurrentThread();
    bool hasJvm = env != nullptr;
    bool ready = hasJvm && sdk::Mc::Instance().Ensure(env);
    bool extra = ready && sdk::Mc::Instance().EnsureExtra(env);
    bool inGame = false, hasWorld = false, screenOpen = false;
    double px = 0, py = 0, pz = 0;
    int entities = -1, dw = 0, dh = 0;
    float gamma = 0;
    if (ready) {
      sdk::Mc::Frame f;
      if (sdk::Mc::Instance().BeginFrame(env, f)) {
        auto& id = sdk::Mc::Instance().Ids();
        inGame = f.player != nullptr;
        hasWorld = f.world != nullptr;
        screenOpen = f.screen != nullptr;
        dw = f.displayW;
        dh = f.displayH;
        gamma = f.gamma;
        if (inGame) {
          px = sdk::Mc::D(env, f.player, id.ePosX);
          py = sdk::Mc::D(env, f.player, id.ePosY);
          pz = sdk::Mc::D(env, f.player, id.ePosZ);
        }
        if (hasWorld) {
          int n = 0;
          sdk::Mc::Instance().EachPlayer(f, [&](jobject) { ++n; });
          entities = n;
        }
        sdk::Mc::Instance().ReleaseFrame(f);
      }
    }
    char b[420];
    std::snprintf(b, sizeof(b),
                  "{\"jni\":%s,\"mcReady\":%s,\"mcExtra\":%s,\"naming\":\"%s\","
                  "\"inGame\":%s,\"hasWorld\":%s,"
                  "\"screenOpen\":%s,\"entities\":%d,\"player\":[%g,%g,%g],"
                  "\"display\":[%d,%d],\"gamma\":%g}",
                  hasJvm ? "true" : "false", ready ? "true" : "false",
                  extra ? "true" : "false",
                  sdk::Mc::Instance().IsNotch() ? "notch" : "searge",
                  inGame ? "true" : "false", hasWorld ? "true" : "false",
                  screenOpen ? "true" : "false", entities, px, py, pz, dw, dh,
                  static_cast<double>(gamma));
    return Resp(200, "OK", "application/json", b);
  }

  // GET /api/debug/esp (ESP boxes + camera + overlay diag for remote diagnosis)
  if (method == "GET" && rest == "debug/esp") {
    render::EspSnapshot snap = render::OverlayWindow::Instance().GetSnapshot();
    render::OverlayDiag diag = render::OverlayWindow::Instance().GetDiag();
    char cb[192];
    std::snprintf(cb, sizeof(cb),
                  "{\"eye\":[%g,%g,%g],\"yaw\":%g,\"pitch\":%g,\"fov\":%g,\"wh\":[%d,%d]}",
                  snap.cam.ex, snap.cam.ey, snap.cam.ez, snap.cam.yaw, snap.cam.pitch,
                  snap.cam.fov, snap.cam.w, snap.cam.h);
    std::string j = std::string("{\"camera\":") + cb + ",\"boxes\":[";
    bool first = true;
    for (const auto& b : snap.boxes) {
      if (!first) j += ",";
      first = false;
      char bb[160];
      std::snprintf(bb, sizeof(bb), "{\"x\":%g,\"y\":%g,\"w\":%g,\"h\":%g,\"hp\":%g}",
                    static_cast<double>(b.x), static_cast<double>(b.y),
                    static_cast<double>(b.w), static_cast<double>(b.h),
                    static_cast<double>(b.hp));
      j += bb;
    }
    j += "],\"overlay\":{";
    char ob[420];
    std::snprintf(ob, sizeof(ob),
                  "\"running\":%s,\"hasWindow\":%s,\"windowValid\":%s,"
                  "\"minimized\":%s,\"fullscreen\":%s,"
                  "\"rect\":[%d,%d,%d,%d],\"dpi\":%u,"
                  "\"ulwFailed\":%s,\"ulwErr\":%lu,\"surfFailed\":%s,"
                  "\"beats\":%llu,\"lines\":%zu,"
                  "\"iters\":%llu,\"stage\":%d,\"flash\":%s,\"paint\":%s",
                  diag.running ? "true" : "false", diag.hasWindow ? "true" : "false",
                  diag.windowValid ? "true" : "false", diag.minimized ? "true" : "false",
                  diag.fullscreen ? "true" : "false", diag.x, diag.y, diag.w, diag.h,
                  diag.dpi, diag.ulwFailed ? "true" : "false",
                  static_cast<unsigned long>(diag.lastUwlErr),
                  diag.surfFailed ? "true" : "false",
                  static_cast<unsigned long long>(diag.beats), snap.lines.size(),
                  static_cast<unsigned long long>(diag.iters), diag.stage,
                  diag.flash ? "true" : "false",
                  diag.paintMode ? "true" : "false");
    j += ob;
    char wb[256];
    std::snprintf(wb, sizeof(wb), ",\"win\":\"%s\"",
                  JsonEscape(hooks::Hooks::Instance().WindowDesc()).c_str());
    j += wb;
    j += "}}";
    return Resp(200, "OK", "application/json", j);
  }

  // POST /api/debug/overlay/flash -> fullscreen red pattern for 5s.
  // If the user sees red, the window path works (content/text is the problem);
  // if not, the layered window itself never reaches the screen.
  if (method == "POST" && rest == "debug/overlay/flash") {
    render::OverlayWindow::Instance().FlashTest(5000);
    return Resp(200, "OK", "application/json", "{\"flash\":true}");
  }

  // GET /api/modules
  if (method == "GET" && rest == "modules") {    std::string j = "[";
    if (modules_) {
      std::lock_guard<std::recursive_mutex> lock(modules_->Mutex());
      bool first = true;
      for (auto& m : modules_->All()) {
        if (!first) j += ",";
        first = false;
        j += ModuleDetailJson(m.get());
      }
    }
    return Resp(200, "OK", "application/json", j + "]");
  }

  // /api/modules/{name}[/toggle]
  const std::string mpre = "modules/";
  if (rest.compare(0, mpre.size(), mpre) == 0) {
    std::string tail = UrlDecode(rest.substr(mpre.size()));
    bool wantToggle = false;
    const std::string tog = "/toggle";
    if (tail.size() > tog.size() &&
        tail.compare(tail.size() - tog.size(), tog.size(), tog) == 0) {
      wantToggle = true;
      tail = tail.substr(0, tail.size() - tog.size());
    }
    if (tail.empty() || tail.find('/') != std::string::npos) {
      return Resp(404, "Not Found", "application/json", "{\"error\":\"bad module path\"}");
    }
    if (!modules_) {
      return Resp(503, "Unavailable", "application/json", "{\"error\":\"no manager\"}");
    }

    std::lock_guard<std::recursive_mutex> lock(modules_->Mutex());
    core::Module* m = FindLocked(modules_, tail);
    if (!m) {
      return Resp(404, "Not Found", "application/json", "{\"error\":\"unknown module\"}");
    }

    if (wantToggle) {
      if (method != "POST") {
        return Resp(405, "Method Not Allowed", "application/json",
                    "{\"error\":\"use POST toggle\"}");
      }
      m->Toggle();
      return Resp(200, "OK", "application/json", ModuleDetailJson(m));
    }

    if (method == "GET") {
      return Resp(200, "OK", "application/json", ModuleDetailJson(m));
    }

    if (method == "PUT") {
      bool en = false;
      if (BodyBool(body, "enabled", en)) m->SetEnabled(en);
      double bind = 0;
      if (BodyNum(body, "bind", bind)) m->BindSetting().vk = static_cast<int>(bind);
      for (auto* b : m->Bools()) {
        bool v = false;
        if (BodyBool(body, b->name, v)) b->value = v;
      }
      for (auto* s : m->Sliders()) {
        double v = 0;
        if (BodyNum(body, s->name, v)) {
          v = std::clamp(v, s->min, s->max);
          if (s->step > 0) v = s->min + std::round((v - s->min) / s->step) * s->step;
          s->value = std::clamp(v, s->min, s->max);
        }
      }
      for (auto* md : m->Modes()) {
        double idx = 0;
        if (BodyNum(body, md->name, idx)) {
          int i = static_cast<int>(idx);
          if (i >= 0 && static_cast<size_t>(i) < md->options.size()) md->index = i;
        } else {
          std::string opt;
          if (BodyStr(body, md->name, opt)) {
            for (size_t i = 0; i < md->options.size(); ++i) {
              if (md->options[i] == opt) {
                md->index = static_cast<int>(i);
                break;
              }
            }
          }
        }
      }
      for (auto* cl : m->Colors()) {
        std::string hex;
        if (BodyStr(body, cl->name, hex)) {
          int r = 0, g = 0, b = 0;
          if (ParseHexColor(hex, r, g, b)) {
            cl->r = r;
            cl->g = g;
            cl->b = b;
          }
        }
      }
      return Resp(200, "OK", "application/json", ModuleDetailJson(m));
    }

    return Resp(405, "Method Not Allowed", "application/json", "{\"error\":\"bad method\"}");
  }

  return Resp(404, "Not Found", "application/json", "{\"error\":\"unknown api\"}");
}

}  // namespace web
}  // namespace raven
