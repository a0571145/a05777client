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

#include "sdk/Mc.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "util/Logger.h"

namespace raven {
namespace sdk {

Mc& Mc::Instance() {
  static Mc inst;
  return inst;
}

namespace {

long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

struct FieldEnt {
  jclass cls;
  jfieldID* out;
  const char* nName;
  const char* sName;
  const char* nSig;
  const char* sSig;
  bool isStatic;
  const char* label;
};

struct MethodEnt {
  jclass cls;
  jmethodID* out;
  const char* nName;
  const char* sName;
  const char* nSig;
  const char* sSig;
  bool isStatic;
  const char* label;
};

bool ResolveFields(JNIEnv* env, const FieldEnt* ents, size_t n, const char* tier,
                   bool useNotch, bool strict) {
  bool ok = true;
  for (size_t i = 0; i < n; ++i) {
    const FieldEnt& f = ents[i];
    if (!f.cls) {
      util::Logger::Warn(std::string("Mc::Ensure[") + tier + "]: field skipped (no class): " +
                         f.label);
      if (strict) return false;
      ok = false;
      continue;
    }
    const char* name = useNotch ? f.nName : f.sName;
    const char* sig = useNotch ? f.nSig : f.sSig;
    jfieldID fid = f.isStatic ? env->GetStaticFieldID(f.cls, name, sig)
                              : env->GetFieldID(f.cls, name, sig);
    if (!fid || env->ExceptionCheck()) {
      env->ExceptionClear();
      util::Logger::Warn(std::string("Mc::Ensure[") + tier + "]: field missing: " + f.label);
      if (strict) return false;
      ok = false;
      continue;
    }
    *f.out = fid;
  }
  return ok;
}

bool ResolveMethods(JNIEnv* env, const MethodEnt* ents, size_t n, const char* tier,
                    bool useNotch, bool strict) {
  bool ok = true;
  for (size_t i = 0; i < n; ++i) {
    const MethodEnt& m = ents[i];
    if (!m.cls) {
      util::Logger::Warn(std::string("Mc::Ensure[") + tier + "]: method skipped (no class): " +
                         m.label);
      if (strict) return false;
      ok = false;
      continue;
    }
    const char* name = useNotch ? m.nName : m.sName;
    const char* sig = useNotch ? m.nSig : m.sSig;
    jmethodID mid = m.isStatic ? env->GetStaticMethodID(m.cls, name, sig)
                               : env->GetMethodID(m.cls, name, sig);
    if (!mid || env->ExceptionCheck()) {
      env->ExceptionClear();
      util::Logger::Warn(std::string("Mc::Ensure[") + tier + "]: method missing: " + m.label);
      if (strict) return false;
      ok = false;
      continue;
    }
    *m.out = mid;
  }
  return ok;
}

// ---- game class-loader discovery (no JVMTI, no FindClass guessing) ----
// Attached native threads see only bootstrap classes, so game classes are
// resolved via Class.forName(name, true, loader) with the loader taken from
// the "Client thread" (vanilla MC main thread) context, with fallback to
// any thread whose loader can see the Minecraft class.

struct BootRefs {
  jclass thread = nullptr, cls = nullptr, map = nullptr, set = nullptr;
  jclass iter = nullptr, entry = nullptr;
  jmethodID allStacks = nullptr, forName = nullptr, entrySet = nullptr;
  jmethodID iterator = nullptr, hasNext = nullptr, next = nullptr;
  jmethodID getKey = nullptr, getName = nullptr, getLoader = nullptr;
};

bool BootInit(JNIEnv* env, BootRefs& b) {
  b.thread = env->FindClass("java/lang/Thread");
  b.cls = env->FindClass("java/lang/Class");
  b.map = env->FindClass("java/util/Map");
  b.set = env->FindClass("java/util/Set");
  b.iter = env->FindClass("java/util/Iterator");
  b.entry = env->FindClass("java/util/Map$Entry");
  if (env->ExceptionCheck() || !b.thread || !b.cls || !b.map || !b.set || !b.iter ||
      !b.entry) {
    env->ExceptionClear();
    return false;
  }
  b.allStacks =
      env->GetStaticMethodID(b.thread, "getAllStackTraces", "()Ljava/util/Map;");
  b.forName = env->GetStaticMethodID(b.cls, "forName",
                                     "(Ljava/lang/String;ZLjava/lang/ClassLoader;)"
                                     "Ljava/lang/Class;");
  b.entrySet = env->GetMethodID(b.map, "entrySet", "()Ljava/util/Set;");
  b.iterator = env->GetMethodID(b.set, "iterator", "()Ljava/util/Iterator;");
  b.hasNext = env->GetMethodID(b.iter, "hasNext", "()Z");
  b.next = env->GetMethodID(b.iter, "next", "()Ljava/lang/Object;");
  b.getKey = env->GetMethodID(b.entry, "getKey", "()Ljava/lang/Object;");
  b.getName = env->GetMethodID(b.thread, "getName", "()Ljava/lang/String;");
  b.getLoader = env->GetMethodID(b.thread, "getContextClassLoader",
                                 "()Ljava/lang/ClassLoader;");
  if (env->ExceptionCheck() || !b.allStacks || !b.forName || !b.entrySet ||
      !b.iterator || !b.hasNext || !b.next || !b.getKey || !b.getName || !b.getLoader) {
    env->ExceptionClear();
    return false;
  }
  return true;
}

void BootCleanup(JNIEnv* env, BootRefs& b) {
  jobject refs[] = {b.thread, b.cls, b.map, b.set, b.iter, b.entry};
  for (jobject r : refs) {
    if (r) env->DeleteLocalRef(r);
  }
  if (env->ExceptionCheck()) env->ExceptionClear();
}

// True when the loader can see the Minecraft class, vanilla (notch "ave")
// or Forge (searge "net.minecraft.client.Minecraft").
bool LoaderSeesGame(JNIEnv* env, BootRefs& b, jobject loader) {
  const char* probes[] = {"ave", "net.minecraft.client.Minecraft"};
  for (const char* p : probes) {
    jstring probe = env->NewStringUTF(p);
    jclass c = probe ? static_cast<jclass>(env->CallStaticObjectMethod(
                                  b.cls, b.forName, probe, JNI_TRUE, loader))
                     : nullptr;
    bool ok = c && !env->ExceptionCheck();
    if (c) env->DeleteLocalRef(c);
    if (probe) env->DeleteLocalRef(probe);
    env->ExceptionClear();
    if (ok) return true;
  }
  return false;
}

// Returns a NEW GLOBAL REF to a loader that can see game classes.
jobject PickGameLoader(JNIEnv* env) {
  BootRefs b;
  if (!BootInit(env, b)) {
    BootCleanup(env, b);
    return nullptr;
  }
  jobject chosen = nullptr;
  int scanned = 0, sawClient = 0;
  jobject map = env->CallStaticObjectMethod(b.thread, b.allStacks);
  if (map && !env->ExceptionCheck()) {
    jobject set = env->CallObjectMethod(map, b.entrySet);
    jobject it = set ? env->CallObjectMethod(set, b.iterator) : nullptr;
    if (it) {
      std::vector<jobject> loaders;  // global refs
      jobject preferred = nullptr;   // "Client thread" loader
      while (env->CallBooleanMethod(it, b.hasNext)) {
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          break;
        }
        jobject entry = env->CallObjectMethod(it, b.next);
        if (!entry || env->ExceptionCheck()) {
          env->ExceptionClear();
          break;
        }
        jobject thread = env->CallObjectMethod(entry, b.getKey);
        env->DeleteLocalRef(entry);
        if (!thread || env->ExceptionCheck()) {
          env->ExceptionClear();
          continue;
        }
        scanned++;
        jstring name = static_cast<jstring>(env->CallObjectMethod(thread, b.getName));
        jobject loader = env->CallObjectMethod(thread, b.getLoader);
        bool clientThread = false;
        if (name && !env->ExceptionCheck()) {
          const char* c = env->GetStringUTFChars(name, nullptr);
          clientThread = c && std::strcmp(c, "Client thread") == 0;
          if (c) env->ReleaseStringUTFChars(name, c);
        } else {
          env->ExceptionClear();
        }
        if (name) env->DeleteLocalRef(name);
        if (clientThread) sawClient = 1;
        if (loader && !env->ExceptionCheck()) {
          bool dup = false;
          for (jobject l : loaders) {
            if (env->IsSameObject(l, loader)) {
              dup = true;
              break;
            }
          }
          if (!dup) {
            jobject g = env->NewGlobalRef(loader);
            if (g) {
              loaders.push_back(g);
              if (clientThread && !preferred) preferred = g;
            }
          }
        } else {
          env->ExceptionClear();
        }
        if (loader) env->DeleteLocalRef(loader);
        env->DeleteLocalRef(thread);
      }
      if (preferred && LoaderSeesGame(env, b, preferred)) {
        chosen = env->NewGlobalRef(preferred);
      }
      if (!chosen) {
        for (jobject l : loaders) {
          if (LoaderSeesGame(env, b, l)) {
            chosen = env->NewGlobalRef(l);
            break;
          }
        }
      }
      for (jobject l : loaders) env->DeleteGlobalRef(l);
    }
    if (set) env->DeleteLocalRef(set);
    if (it) env->DeleteLocalRef(it);
    env->DeleteLocalRef(map);
  } else {
    env->ExceptionClear();
  }
  if (chosen) {
    // Diagnostic: which loader did we pick? (helps联调 when mappings fail)
    std::string lname = "?";
    jclass objCls = env->FindClass("java/lang/Object");
    jclass clsCls = env->FindClass("java/lang/Class");
    if (objCls && clsCls && !env->ExceptionCheck()) {
      jmethodID getClass = env->GetMethodID(objCls, "getClass", "()Ljava/lang/Class;");
      jmethodID getName = env->GetMethodID(clsCls, "getName", "()Ljava/lang/String;");
      if (getClass && getName) {
        jobject lc = env->CallObjectMethod(chosen, getClass);
        jstring ln = (lc && !env->ExceptionCheck())
                         ? static_cast<jstring>(env->CallObjectMethod(lc, getName))
                         : nullptr;
        if (ln && !env->ExceptionCheck()) {
          const char* c = env->GetStringUTFChars(ln, nullptr);
          if (c) lname = c;
          if (c) env->ReleaseStringUTFChars(ln, c);
          env->DeleteLocalRef(ln);
        } else {
          env->ExceptionClear();
        }
        if (lc) env->DeleteLocalRef(lc);
      }
    } else {
      env->ExceptionClear();
    }
    if (objCls) env->DeleteLocalRef(objCls);
    if (clsCls) env->DeleteLocalRef(clsCls);
    char msg[256];
    std::snprintf(msg, sizeof(msg),
                  "Mc::loader: %s (threads=%d, clientThread=%s)", lname.c_str(), scanned,
                  sawClient ? "yes" : "no");
    util::Logger::Info(msg);
  }
  BootCleanup(env, b);
  return chosen;
}

// forName method on java/lang/Class (bootstrap: re-resolvable anywhere).
jmethodID ForNameId(JNIEnv* env) {
  jclass cls = env->FindClass("java/lang/Class");
  if (!cls || env->ExceptionCheck()) {
    env->ExceptionClear();
    return nullptr;
  }
  jmethodID mid = env->GetStaticMethodID(cls, "forName",
                                         "(Ljava/lang/String;ZLjava/lang/ClassLoader;)"
                                         "Ljava/lang/Class;");
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    mid = nullptr;
  }
  env->DeleteLocalRef(cls);
  return mid;
}

// True when forName(dotted) resolves through the game loader.
bool SeesClass(JNIEnv* env, jobject loader, jmethodID forName, const char* dotted) {
  jclass classCls = env->FindClass("java/lang/Class");
  jstring name =
      (classCls && !env->ExceptionCheck()) ? env->NewStringUTF(dotted) : nullptr;
  jclass c = (classCls && name && !env->ExceptionCheck())
                 ? static_cast<jclass>(
                       env->CallStaticObjectMethod(classCls, forName, name, JNI_TRUE, loader))
                 : nullptr;
  bool ok = c && !env->ExceptionCheck();
  if (c) env->DeleteLocalRef(c);
  if (name) env->DeleteLocalRef(name);
  if (classCls) env->DeleteLocalRef(classCls);
  env->ExceptionClear();
  return ok;
}

// Loads one game class via the cached loader. Returns global ref or nullptr.
// NOTE: Class.forName wants dotted binary names ("java.util.List"), while
// notch/searge names use slashes ("java/util/List") and $ for inner classes
// ("ir$a", already correct) -- convert here.
jclass LoadGameClass(JNIEnv* env, jobject loader, jmethodID forName, const char* notch,
                     const char* searge, bool useNotch, const char* tier) {
  std::string dotted = useNotch ? notch : searge;
  for (char& c : dotted) {
    if (c == '/') c = '.';
  }
  jclass classCls = env->FindClass("java/lang/Class");  // bootstrap: always visible
  jstring name = env->NewStringUTF(dotted.c_str());
  jclass local = nullptr;
  if (classCls && name && !env->ExceptionCheck()) {
    local = static_cast<jclass>(
        env->CallStaticObjectMethod(classCls, forName, name, JNI_TRUE, loader));
  }
  if (name) env->DeleteLocalRef(name);
  if (classCls) env->DeleteLocalRef(classCls);
  if (!local || env->ExceptionCheck()) {
    env->ExceptionClear();
    util::Logger::Warn(std::string("Mc::Ensure[") + tier + "]: class missing: " + dotted);
    return nullptr;
  }
  jclass global = static_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  return global;
}

}  // namespace

bool Mc::Ensure(JNIEnv* env) {
  if (!env) return false;
  if (ready_) return true;
  std::lock_guard<std::mutex> lock(mutex_);
  if (ready_) return true;
  long long now = NowMs();
  if (now - lastTryMs_ < 5000) return false;
  lastTryMs_ = now;
  ready_ = ResolveCore(env);
  if (ready_) {
    util::Logger::Info("Mc::Ensure[core]: vanilla 1.8.9 mappings resolved");
  }
  return ready_;
}

bool Mc::EnsureExtra(JNIEnv* env) {
  if (!env || !ready_) return false;
  if (extraReady_) return true;
  std::lock_guard<std::mutex> lock(mutex_);
  if (extraReady_) return true;
  long long now = NowMs();
  if (now - lastExtraTryMs_ < 5000) return false;
  lastExtraTryMs_ = now;
  extraReady_ = ResolveExtra(env);
  if (extraReady_) {
    util::Logger::Info("Mc::Ensure[extra]: world/packet/gui mappings resolved");
  }
  return extraReady_;
}

bool Mc::ResolveCore(JNIEnv* env) {
  McIds& id = ids_;
  if (!loader_) {
    loader_ = PickGameLoader(env);
    if (!loader_) {
      util::Logger::Warn("Mc::Ensure[core]: no game class loader found");
      return false;
    }
  }
  jmethodID forName = ForNameId(env);
  if (!forName) return false;

  // Environment probe: vanilla uses notch names ("ave"), Forge runtime uses
  // searge names ("net.minecraft.client.Minecraft"). Everything below follows.
  notch_ = SeesClass(env, loader_, forName, "ave");
  if (!notch_ && !SeesClass(env, loader_, forName, "net.minecraft.client.Minecraft")) {
    util::Logger::Warn("Mc::Ensure[core]: neither notch (ave) nor searge Minecraft found");
    return false;
  }
  util::Logger::Info(std::string("Mc::Ensure[core]: naming=") +
                     (notch_ ? "notch (vanilla)" : "searge (Forge)"));

  struct CoreClass {
    jclass* out;
    const char* notch;
    const char* searge;
  };
  const CoreClass classes[] = {
      {&id.minecraft, "ave", "net/minecraft/client/Minecraft"},
      {&id.settings, "avh", "net/minecraft/client/settings/GameSettings"},
      {&id.keyBinding, "avb", "net/minecraft/client/settings/KeyBinding"},
      {&id.entity, "pk", "net/minecraft/entity/Entity"},
      {&id.living, "pr", "net/minecraft/entity/EntityLivingBase"},
      {&id.entityPlayer, "wn", "net/minecraft/entity/player/EntityPlayer"},
      {&id.itemStack, "zx", "net/minecraft/item/ItemStack"},
      {&id.item, "zw", "net/minecraft/item/Item"},
      {&id.sword, "aay", "net/minecraft/item/ItemSword"},
      {&id.bow, "yt", "net/minecraft/item/ItemBow"},
      {&id.potion, "aai", "net/minecraft/item/ItemPotion"},
      {&id.axe, "yl", "net/minecraft/item/ItemAxe"},
      {&id.caps, "wl", "net/minecraft/entity/player/PlayerCapabilities"},
      {&id.inventory, "wm", "net/minecraft/entity/player/InventoryPlayer"},
      {&id.timer, "avl", "net/minecraft/util/Timer"},
      {&id.renderMgr, "biu", "net/minecraft/client/renderer/entity/RenderManager"},
      {&id.mop, "auh", "net/minecraft/util/MovingObjectPosition"},
      {&id.netHandler, "bcy", "net/minecraft/client/network/NetHandlerPlayClient"},
      {&id.packet, "ff", "net/minecraft/network/Packet"},
      {&id.list, "java/util/List", "java/util/List"},
  };
  for (const auto& c : classes) {
    *c.out = LoadGameClass(env, loader_, forName, c.notch, c.searge, notch_, "core");
    if (!*c.out) return false;
  }

  const FieldEnt fields[] = {
      {id.minecraft, &id.mcPlayer, "h", "field_71439_g", "Lbew;",
       "Lnet/minecraft/client/entity/EntityPlayerSP;", false, "thePlayer"},
      {id.minecraft, &id.mcWorld, "f", "field_71441_e", "Lbdb;",
       "Lnet/minecraft/client/multiplayer/WorldClient;", false, "theWorld (WorldClient)"},
      {id.minecraft, &id.mcSettings, "t", "field_71474_y", "Lavh;",
       "Lnet/minecraft/client/settings/GameSettings;", false, "gameSettings"},
      {id.minecraft, &id.mcScreen, "m", "field_71462_r", "Laxu;",
       "Lnet/minecraft/client/gui/GuiScreen;", false, "currentScreen"},
      {id.minecraft, &id.mcHasFocus, "w", "field_71415_G", "Z", "Z", false, "hasFocus"},
      {id.minecraft, &id.mcMouseOver, "s", "field_71476_x", "Lauh;",
       "Lnet/minecraft/util/MovingObjectPosition;", false, "objectMouseOver"},
      {id.minecraft, &id.mcController, "c", "field_71442_b", "Lbda;",
       "Lnet/minecraft/client/multiplayer/PlayerControllerMP;", false, "playerController"},
      {id.minecraft, &id.mcTimer, "Y", "field_71428_T", "Lavl;",
       "Lnet/minecraft/util/Timer;", false, "timer"},
      {id.minecraft, &id.mcDisplayW, "d", "field_71443_c", "I", "I", false, "displayWidth"},
      {id.minecraft, &id.mcDisplayH, "e", "field_71440_d", "I", "I", false, "displayHeight"},
      {id.entity, &id.eMotionX, "v", "field_70159_w", "D", "D", false, "motionX"},
      {id.entity, &id.eMotionY, "w", "field_70181_x", "D", "D", false, "motionY"},
      {id.entity, &id.eMotionZ, "x", "field_70179_y", "D", "D", false, "motionZ"},
      {id.entity, &id.ePosX, "s", "field_70165_t", "D", "D", false, "posX"},
      {id.entity, &id.ePosY, "t", "field_70163_u", "D", "D", false, "posY"},
      {id.entity, &id.ePosZ, "u", "field_70161_v", "D", "D", false, "posZ"},
      {id.entity, &id.eLastX, "P", "field_70142_S", "D", "D", false, "lastTickPosX"},
      {id.entity, &id.eLastY, "Q", "field_70137_T", "D", "D", false, "lastTickPosY"},
      {id.entity, &id.eLastZ, "R", "field_70136_U", "D", "D", false, "lastTickPosZ"},
      {id.entity, &id.eWidth, "J", "field_70130_N", "F", "F", false, "width"},
      {id.entity, &id.eHeight, "K", "field_70131_O", "F", "F", false, "height"},
      {id.entity, &id.eOnGround, "C", "field_70122_E", "Z", "Z", false, "onGround"},
      {id.entity, &id.eYaw, "y", "field_70177_z", "F", "F", false, "rotationYaw"},
      {id.entity, &id.ePitch, "z", "field_70125_A", "F", "F", false, "rotationPitch"},
      {id.entity, &id.eDead, "I", "field_70128_L", "Z", "Z", false, "isDead"},
      {id.living, &id.lHurtTime, "av", "field_70738_aO", "I", "I", false, "hurtTime"},
      {id.living, &id.lMaxHurt, "au", "field_70737_aN", "I", "I", false, "maxHurtTime"},
      {id.entityPlayer, &id.pInventory, "bi", "field_71071_by", "Lwm;",
       "Lnet/minecraft/entity/player/InventoryPlayer;", false, "inventory"},
      {id.entityPlayer, &id.pCaps, "bA", "field_71075_bZ", "Lwl;",
       "Lnet/minecraft/entity/player/PlayerCapabilities;", false, "capabilities"},
      {id.inventory, &id.invCurrent, "c", "field_70461_c", "I", "I", false, "currentItem"},
      {id.caps, &id.cCreative, "d", "field_75098_d", "Z", "Z", false, "isCreativeMode"},
      {id.caps, &id.cFlying, "b", "field_75100_b", "Z", "Z", false, "isFlying"},
      {id.caps, &id.cAllowFly, "c", "field_75101_c", "Z", "Z", false, "allowFlying"},
      {id.caps, &id.cCanEdit, "e", "field_75099_e", "Z", "Z", false,
       "allowEdit? (adventure gate)"},
      {id.settings, &id.sKeyAttack, "ai", "field_74312_F", "Lavb;",
       "Lnet/minecraft/client/settings/KeyBinding;", false, "keyBindAttack"},
      {id.settings, &id.sKeyUse, "ag", "field_74313_G", "Lavb;",
       "Lnet/minecraft/client/settings/KeyBinding;", false, "keyBindUseItem"},
      {id.settings, &id.sKeySprint, "ae", "field_151444_V", "Lavb;",
       "Lnet/minecraft/client/settings/KeyBinding;", false, "keyBindSprint"},
      {id.settings, &id.sKeyForward, "Y", "field_74351_w", "Lavb;",
       "Lnet/minecraft/client/settings/KeyBinding;", false, "keyBindForward"},
      {id.settings, &id.sKeyBack, "aa", "field_74368_y", "Lavb;",
       "Lnet/minecraft/client/settings/KeyBinding;", false, "keyBindBack"},
      {id.settings, &id.sKeyLeft, "Z", "field_74370_x", "Lavb;",
       "Lnet/minecraft/client/settings/KeyBinding;", false, "keyBindLeft"},
      {id.settings, &id.sKeyRight, "ab", "field_74366_z", "Lavb;",
       "Lnet/minecraft/client/settings/KeyBinding;", false, "keyBindRight"},
      {id.minecraft, &id.mcRightDelay, "ap", "field_71467_ac", "I", "I", false,
       "rightClickDelayTimer"},
      {id.inventory, &id.invMain, "a", "field_70462_a", "[Lzx;",
       "[Lnet/minecraft/item/ItemStack;", false, "mainInventory"},
      {id.settings, &id.sGamma, "aJ", "field_74333_Y", "F", "F", false, "gammaSetting"},
      {id.settings, &id.sFov, "aI", "field_74334_X", "F", "F", false, "fovSetting"},
      {id.keyBinding, &id.kPressed, "h", "field_74513_e", "Z", "Z", false, "pressed"},
      {id.mop, &id.mopEntity, "d", "field_72308_g", "Lpk;",
       "Lnet/minecraft/entity/Entity;", false, "entityHit"},
      {id.renderMgr, &id.rViewX, "h", "field_78730_l", "D", "D", false, "viewerPosX"},
      {id.renderMgr, &id.rViewY, "i", "field_78731_m", "D", "D", false, "viewerPosY"},
      {id.renderMgr, &id.rViewZ, "j", "field_78728_n", "D", "D", false, "viewerPosZ"},
      {id.timer, &id.tPartial, "c", "field_74281_c", "F", "F", false, "elapsedPartialTicks"},
  };
  if (!ResolveFields(env, fields, sizeof(fields) / sizeof(fields[0]), "core", notch_,
                     true)) {
    return false;
  }

  const MethodEnt methods[] = {
      {id.minecraft, &id.mcGetMc, "A", "func_71410_x", "()Lave;",
       "()Lnet/minecraft/client/Minecraft;", true, "getMinecraft"},
      {id.minecraft, &id.mcGetRenderMgr, "af", "func_175598_ae", "()Lbiu;",
       "()Lnet/minecraft/client/renderer/entity/RenderManager;", false, "getRenderManager"},
      {id.entityPlayer, &id.pIsUsingItem, "bS", "func_71039_bw", "()Z", "()Z", false,
       "isUsingItem"},
      {id.entityPlayer, &id.pSetItemInUse, "a", "func_71008_a", "(Lzx;I)V",
       "(Lnet/minecraft/item/ItemStack;I)V", false, "setItemInUse"},
      {id.entityPlayer, &id.pClearItemInUse, "bU", "func_71034_by", "()V", "()V", false,
       "clearItemInUse"},
      {id.living, &id.lGetHealth, "bn", "func_110143_aJ", "()F", "()F", false, "getHealth"},
      {id.living, &id.lGetHeld, "bA", "func_70694_bm", "()Lzx;",
       "()Lnet/minecraft/item/ItemStack;", false, "getHeldItem"},
      {id.entity, &id.eInvisible, "ax", "func_82150_aj", "()Z", "()Z", false, "isInvisible"},
      {id.entity, &id.eSprinting, "aw", "func_70051_ag", "()Z", "()Z", false, "isSprinting"},
      {id.itemStack, &id.stGetItem, "b", "func_77973_b", "()Lzw;",
       "()Lnet/minecraft/item/Item;", false, "getItem"},
      {id.itemStack, &id.stGetMeta, "h", "func_77952_i", "()I", "()I", false, "getMetadata"},
      {id.item, &id.itStrVs, "a", "func_150893_a", "(Lzx;Lafh;)F",
       "(Lnet/minecraft/item/ItemStack;Lnet/minecraft/block/Block;)F", false,
       "getStrVsBlock"},
      {id.potion, &id.potSplash, "f", "func_77831_g", "(I)Z", "(I)Z", true, "isSplash"},
      {id.keyBinding, &id.kGetCode, "i", "func_151463_i", "()I", "()I", false, "getKeyCode"},
      {id.keyBinding, &id.kOnTick, "a", "func_74507_a", "(I)V", "(I)V", true,
       "KeyBinding.onTick"},
      {id.keyBinding, &id.kSetState, "a", "func_74510_a", "(IZ)V", "(IZ)V", true,
       "setKeyBindState"},
      {id.netHandler, &id.nhSend, "a", "func_147297_a", "(Lff;)V",
       "(Lnet/minecraft/network/Packet;)V", false, "addToSendQueue"},
      {id.list, &id.listToArray, "toArray", "toArray", "()[Ljava/lang/Object;",
       "()[Ljava/lang/Object;", false, "toArray"},
  };
  if (!ResolveMethods(env, methods, sizeof(methods) / sizeof(methods[0]), "core", notch_,
                      true)) {
    return false;
  }
  return true;
}

bool Mc::ResolveExtra(JNIEnv* env) {
  if (!loader_) return false;
  McIds& id = ids_;
  jmethodID forName = ForNameId(env);
  if (!forName) return false;

  struct ExtraClass {
    jclass* out;
    const char* notch;
    const char* searge;
  };
  const ExtraClass classes[] = {
      {&id.playerSP, "bew", "net/minecraft/client/entity/EntityPlayerSP"},
      {&id.world, "adm", "net/minecraft/world/World"},
      {&id.moveInput, "beu", "net/minecraft/util/MovementInput"},
      {&id.c07, "ir", "net/minecraft/network/play/client/C07PacketPlayerDigging"},
      {&id.c07action, "ir$a",
       "net/minecraft/network/play/client/C07PacketPlayerDigging$Action"},
      {&id.c09, "iv", "net/minecraft/network/play/client/C09PacketHeldItemChange"},
      {&id.blockPos, "cj", "net/minecraft/util/BlockPos"},
      {&id.facing, "cq", "net/minecraft/util/EnumFacing"},
      {&id.blocks, "afi", "net/minecraft/init/Blocks"},
      {&id.block, "afh", "net/minecraft/block/Block"},
      {&id.liquid, "ahv", "net/minecraft/block/BlockLiquid"},
      {&id.ibs, "alz", "net/minecraft/block/state/IBlockState"},
      {&id.guiContainer, "ayl", "net/minecraft/client/gui/inventory/GuiContainer"},
      {&id.slot, "yg", "net/minecraft/inventory/Slot"},
      {&id.container, "xi", "net/minecraft/inventory/Container"},
      {&id.guiScreen, "axu", "net/minecraft/client/gui/GuiScreen"},
      {&id.controller, "bda", "net/minecraft/client/multiplayer/PlayerControllerMP"},
      {&id.vec3, "aui", "net/minecraft/util/Vec3"},
      {&id.aabb, "aug", "net/minecraft/util/AxisAlignedBB"},
      {&id.itemFrame, "uo", "net/minecraft/entity/item/EntityItemFrame"},
      {&id.chatComp, "eu", "net/minecraft/util/IChatComponent"},
      {&id.potionFx, "pf", "net/minecraft/potion/PotionEffect"},
      {&id.collCls, "java/util/Collection", "java/util/Collection"},
      {&id.iterCls, "java/util/Iterator", "java/util/Iterator"},
      {&id.c03, "ip", "net/minecraft/network/play/client/C03PacketPlayer"},
      {&id.snow, "ajp", "net/minecraft/block/BlockSnow"},
      {&id.carpet, "aks", "net/minecraft/block/BlockCarpet"},
      {&id.itemblock, "yo", "net/minecraft/item/ItemBlock"},
      {&id.rod, "zq", "net/minecraft/item/ItemFishingRod"},
      {&id.potionCls, "pe", "net/minecraft/potion/Potion"},
  };
  bool classesOk = true;
  for (const auto& c : classes) {
    *c.out = LoadGameClass(env, loader_, forName, c.notch, c.searge, notch_, "extra");
    if (!*c.out) classesOk = false;
  }

  const FieldEnt fields[] = {
      {id.playerSP, &id.spMoveInput, "b", "field_71158_b", "Lbeu;",
       "Lnet/minecraft/util/MovementInput;", false, "movementInput"},
      {id.playerSP, &id.spSendQueue, "a", "field_71174_a", "Lbcy;",
       "Lnet/minecraft/client/network/NetHandlerPlayClient;", false, "sendQueue"},
      {id.world, &id.wEntities, "f", "field_72996_f", "Ljava/util/List;",
       "Ljava/util/List;", false, "loadedEntityList"},
      {id.moveInput, &id.inForward, "b", "field_78900_b", "F", "F", false, "moveForward"},
      {id.moveInput, &id.inStrafe, "a", "field_78902_a", "F", "F", false, "moveStrafe"},
      {id.moveInput, &id.inJump, "c", "field_78901_c", "Z", "Z", false, "jump"},
      {id.moveInput, &id.inSneak, "d", "field_78899_d", "Z", "Z", false, "sneak"},
      {id.blocks, &id.blocksAir, "a", "field_150350_a", "Lafh;",
       "Lnet/minecraft/block/Block;", true, "Blocks.air"},
      {id.blockPos, &id.posOrigin, "a", "field_177992_a", "Lcj;",
       "Lnet/minecraft/util/BlockPos;", true, "BlockPos.ORIGIN"},
      {id.facing, &id.faceDown, "a", "DOWN", "Lcq;", "Lnet/minecraft/util/EnumFacing;", true,
       "EnumFacing.DOWN"},
      {id.c07action, &id.actRelease, "f", "RELEASE_USE_ITEM", "Lir$a;",
       "Lnet/minecraft/network/play/client/C07PacketPlayerDigging$Action;", true,
       "RELEASE_USE_ITEM"},
      {id.guiContainer, &id.gSlot, "u", "field_147006_u", "Lyg;",
       "Lnet/minecraft/inventory/Slot;", false, "theSlot"},
      {id.guiContainer, &id.gContainer, "h", "field_147002_h", "Lxi;",
       "Lnet/minecraft/inventory/Container;", false, "inventorySlots"},
      {id.slot, &id.slotNum, "e", "field_75222_d", "I", "I", false, "slotNumber"},
      {id.container, &id.contWindow, "d", "field_75152_c", "I", "I", false, "windowId"},
      {id.settings, &id.sneakKey, "ad", "field_74311_E", "Lavb;",
       "Lnet/minecraft/client/settings/KeyBinding;", false, "keyBindSneak"},
      {id.entity, &id.fallDist, "O", "field_70143_R", "F", "F", false, "fallDistance"},
      {id.entity, &id.ticksEx, "W", "field_70173_aa", "I", "I", false, "ticksExisted"},
      {id.entity, &id.collidedH, "D", "field_70123_F", "Z", "Z", false,
       "isCollidedHorizontally"},
      {id.timer, &id.timerSpeed, "d", "field_74278_d", "F", "F", false, "timerSpeed"},
      {id.settings, &id.jumpKey, "ac", "field_74314_A", "Lavb;",
       "Lnet/minecraft/client/settings/KeyBinding;", false, "keyBindJump"},
      {id.potionCls, &id.potJump, "j", "field_76430_j", "Lpe;",
       "Lnet/minecraft/potion/Potion;", true, "Potion.jump"},
      {id.potionCls, &id.potPoison, "u", "field_76436_u", "Lpe;",
       "Lnet/minecraft/potion/Potion;", true, "Potion.poison"},
      {id.potionCls, &id.potWither, "v", "field_82731_v", "Lpe;",
       "Lnet/minecraft/potion/Potion;", true, "Potion.wither"},
      {id.entityPlayer, &id.fishHook, "bG", "field_71104_cf", "Lur;",
       "Lnet/minecraft/entity/projectile/EntityFishHook;", false, "fishEntity"},
      {id.minecraft, &id.mcPointed, "i", "field_147125_j", "Lpk;",
       "Lnet/minecraft/entity/Entity;", false, "pointedEntity"},
      {id.entity, &id.eRiding, "m", "field_70154_o", "Lpk;",
       "Lnet/minecraft/entity/Entity;", false, "ridingEntity"},
      {id.mop, &id.mopHitVec, "c", "field_72307_f", "Laui;",
       "Lnet/minecraft/util/Vec3;", false, "hitVec"},
      {id.vec3, &id.vecX, "a", "field_72450_a", "D", "D", false, "vec.x"},
      {id.vec3, &id.vecY, "b", "field_72448_b", "D", "D", false, "vec.y"},
      {id.vec3, &id.vecZ, "c", "field_72449_c", "D", "D", false, "vec.z"},
      {id.aabb, &id.boxMinX, "a", "field_72340_a", "D", "D", false, "AABB.minX"},
      {id.aabb, &id.boxMinY, "b", "field_72338_b", "D", "D", false, "AABB.minY"},
      {id.aabb, &id.boxMinZ, "c", "field_72339_c", "D", "D", false, "AABB.minZ"},
      {id.aabb, &id.boxMaxX, "d", "field_72336_d", "D", "D", false, "AABB.maxX"},
      {id.aabb, &id.boxMaxY, "e", "field_72337_e", "D", "D", false, "AABB.maxY"},
      {id.aabb, &id.boxMaxZ, "f", "field_72334_f", "D", "D", false, "AABB.maxZ"},
      {id.itemStack, &id.stSize, "b", "field_77994_a", "I", "I", false, "stackSize"},
  };
  bool fieldsOk = ResolveFields(env, fields, sizeof(fields) / sizeof(fields[0]),
                                "extra", notch_, false);

  const MethodEnt methods[] = {
      {id.entity, &id.eGetEyes, "e", "func_174824_e", "(F)Laui;",
       "(F)Lnet/minecraft/util/Vec3;", false, "getPositionEyes"},
      {id.mop, &id.mopBlockPos, "a", "func_178782_a", "()Lcj;",
       "()Lnet/minecraft/util/BlockPos;", false, "getBlockPos"},
      {id.world, &id.wBlockState, "p", "func_180495_p", "(Lcj;)Lalz;",
       "(Lnet/minecraft/util/BlockPos;)Lnet/minecraft/block/state/IBlockState;", false,
       "getBlockState"},
      {id.ibs, &id.ibsGetBlock, "c", "func_177230_c", "()Lafh;",
       "()Lnet/minecraft/block/Block;", false, "getBlock"},
      {id.controller, &id.ctlWindowClick, "a", "func_78753_a", "(IIIILwn;)Lzx;",
       "(IIIILnet/minecraft/entity/player/EntityPlayer;)Lnet/minecraft/item/ItemStack;",
       false, "windowClick"},
      {id.controller, &id.ctlSyncItem, "n", "func_78750_j", "()V", "()V", false,
       "syncCurrentPlayItem"},
      {id.guiScreen, &id.scrShift, "r", "func_146272_n", "()Z", "()Z", true,
       "isShiftKeyDown"},
      {id.vec3, &id.vDist, "f", "func_72438_d", "(Laui;)D",
       "(Lnet/minecraft/util/Vec3;)D", false, "distanceTo"},
      {id.c09, &id.c09Ctor, "<init>", "<init>", "(I)V", "(I)V", false, "C09.<init>"},
      {id.c07, &id.c07Ctor, "<init>", "<init>", "(Lir$a;Lcj;Lcq;)V",
       "(Lnet/minecraft/network/play/client/C07PacketPlayerDigging$Action;"
       "Lnet/minecraft/util/BlockPos;Lnet/minecraft/util/EnumFacing;)V",
       false, "C07.<init>"},
      {id.minecraft, &id.mcRenderView, "ac", "func_175606_aa", "()Lpk;",
       "()Lnet/minecraft/entity/Entity;", false, "getRenderViewEntity"},
      {id.entity, &id.eGetLook, "d", "func_70676_i", "(F)Laui;",
       "(F)Lnet/minecraft/util/Vec3;", false, "getLook"},
      {id.vec3, &id.vecAdd, "b", "func_72441_c", "(DDD)Laui;",
       "(DDD)Lnet/minecraft/util/Vec3;", false, "addVector"},
      {id.entity, &id.eGetBox, "aR", "func_174813_aQ", "()Laug;",
       "()Lnet/minecraft/util/AxisAlignedBB;", false, "getEntityBoundingBox"},
      {id.aabb, &id.boxExpand, "a", "func_72321_a", "(DDD)Laug;",
       "(DDD)Lnet/minecraft/util/AxisAlignedBB;", false, "AABB.expand"},
      {id.aabb, &id.boxGrow, "b", "func_72314_b", "(DDD)Laug;",
       "(DDD)Lnet/minecraft/util/AxisAlignedBB;", false, "AABB.expand2"},
      {id.aabb, &id.boxIsVec, "a", "func_72318_a", "(Laui;)Z",
       "(Lnet/minecraft/util/Vec3;)Z", false, "AABB.isVecInside"},
      {id.aabb, &id.boxIntercept, "a", "func_72327_a", "(Laui;Laui;)Lauh;",
       "(Lnet/minecraft/util/Vec3;Lnet/minecraft/util/Vec3;)"
       "Lnet/minecraft/util/MovingObjectPosition;",
       false, "AABB.calculateIntercept"},
      {id.world, &id.wGetEntities, "b", "func_72839_b", "(Lpk;Laug;)Ljava/util/List;",
       "(Lnet/minecraft/entity/Entity;Lnet/minecraft/util/AxisAlignedBB;)Ljava/util/List;",
       false, "getEntitiesWithinAABB"},
      {id.entity, &id.eCanCollide, "ad", "func_70067_L", "()Z", "()Z", false,
       "canBeCollidedWith"},
      {id.entity, &id.eBorder, "ao", "func_70111_Y", "()F", "()F", false,
       "getCollisionBorderSize"},
      {id.controller, &id.ctlExtReach, "i", "func_78749_i", "()Z", "()Z", false,
       "extendedReach"},
      {id.entityPlayer, &id.pIsSpectator, "bW", "func_70632_aY", "()Z", "()Z", false,
       "isSpectator"},
      {id.entity, &id.eCanRide, "a", "func_174825_a", "(Lwn;Laui;)Z",
       "(Lnet/minecraft/entity/player/EntityPlayer;Lnet/minecraft/util/Vec3;)Z", false,
       "canRiderInteract"},
      {id.controller, &id.ctlAttack, "a", "func_78764_a", "(Lwn;Lpk;)V",
       "(Lnet/minecraft/entity/player/EntityPlayer;Lnet/minecraft/entity/Entity;)V", false,
       "attackEntity"},
      {id.living, &id.pSwing, "bw", "func_71038_i", "()V", "()V", false, "swingItem"},
      {id.world, &id.wRayTrace, "a", "func_147447_a", "(Laui;Laui;ZZZ)Lauh;",
       "(Lnet/minecraft/util/Vec3;Lnet/minecraft/util/Vec3;ZZZ)"
       "Lnet/minecraft/util/MovingObjectPosition;",
       false, "rayTraceBlocks"},
      {id.entityPlayer, &id.eGetName, "f_", "func_145748_c_", "()Leu;",
       "()Lnet/minecraft/util/IChatComponent;", false, "getDisplayName"},
      {id.chatComp, &id.ccText, "c", "func_150260_c", "()Ljava/lang/String;",
       "()Ljava/lang/String;", false, "getUnformattedText"},
      {id.vec3, &id.vecCtor, "<init>", "<init>", "(DDD)V", "(DDD)V", false, "Vec3.<init>"},
      {id.mop, &id.mopCtor, "<init>", "<init>", "(Lpk;Laui;)V",
       "(Lnet/minecraft/entity/Entity;Lnet/minecraft/util/Vec3;)V", false, "MOP.<init>"},
      {id.caps, &id.capsFlySpeed, "a", "func_75092_a", "(F)V", "(F)V", false, "setFlySpeed"},
      {id.living, &id.jumpM, "bF", "func_70664_aZ", "()V", "()V", false, "jump"},
      {id.entity, &id.setSprintM, "d", "func_70031_b", "(Z)V", "(Z)V", false, "setSprinting"},
      {id.entity, &id.inWaterM, "V", "func_70090_H", "()Z", "()Z", false, "isInWater"},
      {id.entity, &id.inLavaM, "ab", "func_180799_ab", "()Z", "()Z", false, "isInLava"},
      {id.entity, &id.isSneakingM, "av", "func_70093_af", "()Z", "()Z", false, "isSneaking"},
      {id.entity, &id.setPosM, "b", "func_70107_b", "(DDD)V", "(DDD)V", false,
       "setPositionAndUpdate"},
      {id.living, &id.activeFx, "bl", "func_70651_bq", "()Ljava/util/Collection;",
       "()Ljava/util/Collection;", false, "getActivePotionEffects"},
      {id.potionFx, &id.fxAmp, "c", "func_76458_c", "()I", "()I", false, "getAmplifier"},
      {id.potionFx, &id.fxName, "g", "func_76453_d", "()Ljava/lang/String;",
       "()Ljava/lang/String;", false, "getPotionName"},
      {id.collCls, &id.collIter, "iterator", "iterator", "()Ljava/util/Iterator;",
       "()Ljava/util/Iterator;", false, "iterator"},
      {id.iterCls, &id.collHas, "hasNext", "hasNext", "()Z", "()Z", false, "hasNext"},
      {id.iterCls, &id.collNext, "next", "next", "()Ljava/lang/Object;",
       "()Ljava/lang/Object;", false, "next"},
      {id.c03, &id.c03Ctor, "<init>", "<init>", "(Z)V", "(Z)V", false, "C03.<init>"},
      {id.blockPos, &id.posCtor, "<init>", "<init>", "(III)V", "(III)V", false,
       "BlockPos.<init>"},
      {id.living, &id.isPotionActive, "b", "func_70660_b", "(Lpe;)Lpf;",
       "(Lnet/minecraft/potion/Potion;)Lnet/minecraft/potion/PotionEffect;", false,
       "isPotionActive"},
      {id.controller, &id.reachDist, "d", "func_78757_d", "()F", "()F", false,
       "getBlockReachDistance"},
      {id.aabb, &id.boxCtor, "<init>", "<init>", "(DDDDDD)V", "(DDDDDD)V", false,
       "AABB.<init> (MCP signature, SRG has no ctors)"},
      {id.world, &id.wCollide, "a", "func_72945_a", "(Lpk;Laug;)Ljava/util/List;",
       "(Lnet/minecraft/entity/Entity;Lnet/minecraft/util/AxisAlignedBB;)"
       "Ljava/util/List;",
       false, "getCollisionBoxes"},
      {id.inventory, &id.invSlot, "a", "func_70301_a", "(I)Lzx;",
       "(I)Lnet/minecraft/item/ItemStack;", false, "getStackInSlot"},
  };
  bool methodsOk = ResolveMethods(env, methods, sizeof(methods) / sizeof(methods[0]),
                                  "extra", notch_, false);

  bool singletonsOk = false;
  if (classesOk && fieldsOk && methodsOk && id.blocks && id.blocksAir && id.blockPos &&
      id.posOrigin && id.facing && id.faceDown && id.c07action && id.actRelease) {
    id.airBlock = env->NewGlobalRef(env->GetStaticObjectField(id.blocks, id.blocksAir));
    id.originPos = env->NewGlobalRef(env->GetStaticObjectField(id.blockPos, id.posOrigin));
    id.downFace = env->NewGlobalRef(env->GetStaticObjectField(id.facing, id.faceDown));
    id.releaseAction =
        env->NewGlobalRef(env->GetStaticObjectField(id.c07action, id.actRelease));
    singletonsOk = id.airBlock && id.originPos && id.downFace && id.releaseAction &&
                   !env->ExceptionCheck();
    if (!singletonsOk) {
      env->ExceptionClear();
      util::Logger::Warn("Mc::Ensure[extra]: singleton resolve failed");
    }
  }
  if (!(classesOk && fieldsOk && methodsOk && singletonsOk)) {
    util::Logger::Warn("Mc::Ensure[extra]: incomplete mapping set, will retry");
    return false;
  }
  return true;
}

bool Mc::BeginFrame(JNIEnv* env, Frame& f) {
  if (!env || !ready_) return false;
  McIds& id = ids_;
  f = Frame();
  f.env = env;
  f.mc = env->CallStaticObjectMethod(id.minecraft, id.mcGetMc);
  if (!f.mc || env->ExceptionCheck()) {
    env->ExceptionClear();
    return false;
  }
  f.player = env->GetObjectField(f.mc, id.mcPlayer);
  f.world = env->GetObjectField(f.mc, id.mcWorld);
  f.settings = env->GetObjectField(f.mc, id.mcSettings);
  f.screen = env->GetObjectField(f.mc, id.mcScreen);
  f.mouseOver = env->GetObjectField(f.mc, id.mcMouseOver);
  f.hasFocus = env->GetBooleanField(f.mc, id.mcHasFocus);
  if (f.settings) {
    f.attackKey = env->GetObjectField(f.settings, id.sKeyAttack);
    f.useKey = env->GetObjectField(f.settings, id.sKeyUse);
    f.sprintKey = env->GetObjectField(f.settings, id.sKeySprint);
    f.fov = env->GetFloatField(f.settings, id.sFov);
    f.gamma = env->GetFloatField(f.settings, id.sGamma);
  }
  f.renderMgr = env->CallObjectMethod(f.mc, id.mcGetRenderMgr);
  jobject timer = env->GetObjectField(f.mc, id.mcTimer);
  if (timer) {
    f.partialTicks = env->GetFloatField(timer, id.tPartial);
    env->DeleteLocalRef(timer);
  }
  f.displayW = env->GetIntField(f.mc, id.mcDisplayW);
  f.displayH = env->GetIntField(f.mc, id.mcDisplayH);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    ReleaseFrame(f);
    return false;
  }
  return true;
}

void Mc::ReleaseFrame(Frame& f) {
  JNIEnv* env = f.env;
  if (!env) return;
  jobject refs[] = {f.mc,      f.player, f.world,     f.settings, f.attackKey,
                    f.useKey,  f.sprintKey, f.renderMgr, f.screen, f.mouseOver};
  for (jobject r : refs) {
    if (r) env->DeleteLocalRef(r);
  }
  f = Frame();
}

void Mc::EachPlayer(Frame& f, const std::function<void(jobject)>& fn,
                      bool playersOnly) {
  JNIEnv* e = f.env;
  if (!e || !f.world || !fn || !extraReady_) return;
  McIds& id = ids_;
  jobject list = e->GetObjectField(f.world, id.wEntities);
  if (!list || e->ExceptionCheck()) {
    e->ExceptionClear();
    return;
  }
  jobjectArray arr =
      static_cast<jobjectArray>(e->CallObjectMethod(list, id.listToArray));
  e->DeleteLocalRef(list);
  if (!arr || e->ExceptionCheck()) {
    e->ExceptionClear();
    return;
  }
  jsize n = e->GetArrayLength(arr);
  for (jsize i = 0; i < n; ++i) {
    jobject ent = e->GetObjectArrayElement(arr, i);
    if (!ent) continue;
    bool want = playersOnly ? e->IsInstanceOf(ent, id.entityPlayer)
                            : e->IsInstanceOf(ent, id.living);
    if (want) {
      fn(ent);
    }
    e->DeleteLocalRef(ent);
    if (e->ExceptionCheck()) {
      e->ExceptionClear();
      break;
    }
  }
  e->DeleteLocalRef(arr);
}

}  // namespace sdk
}  // namespace raven
