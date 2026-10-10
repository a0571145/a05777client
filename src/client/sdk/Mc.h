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
#include <jni.h>
#include <cstdint>
#include <functional>
#include <mutex>

// Vanilla 1.8.9 (notch) JNI mapping layer.
// Searge names from raven-src are translated via third_party/mcp/joined.srg;
// ONLY this module speaks notch (e.g. ave/h == Minecraft.thePlayer).
//
// Threading: Ids are cached once (global refs + IDs are thread-safe).
// Frame fetches fresh local refs per call; pair BeginFrame with ReleaseFrame.
namespace raven {
namespace sdk {

struct McIds {
  // classes (global refs)
  jclass minecraft = nullptr, playerSP = nullptr, entityPlayer = nullptr;
  jclass living = nullptr, entity = nullptr, world = nullptr, settings = nullptr;
  jclass keyBinding = nullptr, mop = nullptr, moveInput = nullptr;
  jclass itemStack = nullptr, item = nullptr, sword = nullptr, bow = nullptr;
  jclass potion = nullptr, axe = nullptr, caps = nullptr, inventory = nullptr;
  jclass renderMgr = nullptr, timer = nullptr, netHandler = nullptr, packet = nullptr;
  jclass c07 = nullptr, c07action = nullptr, c09 = nullptr, blockPos = nullptr;
  jclass facing = nullptr, blocks = nullptr, block = nullptr, liquid = nullptr;
  jclass ibs = nullptr, guiContainer = nullptr, slot = nullptr, container = nullptr;
  jclass guiScreen = nullptr, controller = nullptr, vec3 = nullptr, list = nullptr;
  jclass aabb = nullptr, itemFrame = nullptr, chatComp = nullptr;
  jclass potionFx = nullptr, collCls = nullptr, iterCls = nullptr, c03 = nullptr;
  jclass snow = nullptr, carpet = nullptr, itemblock = nullptr;
  jclass rod = nullptr, potionCls = nullptr;

  // Minecraft fields
  jfieldID mcPlayer = nullptr, mcWorld = nullptr, mcSettings = nullptr;
  jfieldID mcScreen = nullptr, mcHasFocus = nullptr, mcMouseOver = nullptr;
  jfieldID mcController = nullptr, mcTimer = nullptr;
  jfieldID mcDisplayW = nullptr, mcDisplayH = nullptr;
  jfieldID mcRightDelay = nullptr;  // rightClickDelayTimer (FastPlace)
  // EntityPlayerSP fields
  jfieldID spMoveInput = nullptr, spSendQueue = nullptr;
  // Entity fields
  jfieldID eMotionX = nullptr, eMotionY = nullptr, eMotionZ = nullptr;
  jfieldID ePosX = nullptr, ePosY = nullptr, ePosZ = nullptr;
  jfieldID eLastX = nullptr, eLastY = nullptr, eLastZ = nullptr;
  jfieldID eWidth = nullptr, eHeight = nullptr, eOnGround = nullptr;
  jfieldID eYaw = nullptr, ePitch = nullptr;
  jfieldID eDead = nullptr;
  jfieldID fallDist = nullptr, ticksEx = nullptr, collidedH = nullptr;
  // EntityLivingBase fields
  jfieldID lHurtTime = nullptr, lMaxHurt = nullptr;
  // EntityPlayer fields
  jfieldID pInventory = nullptr, pCaps = nullptr;
  // InventoryPlayer fields
  jfieldID invCurrent = nullptr, invMain = nullptr;
  // PlayerCapabilities fields
  jfieldID cCreative = nullptr, cFlying = nullptr;
  jfieldID cAllowFly = nullptr;  // allowFlying (wl/c, proven by Fly.onDisable)
  jfieldID cCanEdit = nullptr;   // adventure gate (wl/e, inferred allowEdit)
  // MovementInput fields
  jfieldID inForward = nullptr, inStrafe = nullptr, inJump = nullptr, inSneak = nullptr;
  // GameSettings fields
  jfieldID sKeyAttack = nullptr, sKeyUse = nullptr, sKeySprint = nullptr;
  jfieldID sKeyForward = nullptr, sKeyBack = nullptr;
  jfieldID sKeyLeft = nullptr, sKeyRight = nullptr;
  jfieldID sGamma = nullptr, sFov = nullptr, sneakKey = nullptr;
  jfieldID jumpKey = nullptr;
  // Potion statics + fish hook
  jfieldID potJump = nullptr, potPoison = nullptr, potWither = nullptr;
  jfieldID fishHook = nullptr;
  // KeyBinding fields
  jfieldID kPressed = nullptr;
  // MovingObjectPosition fields
  jfieldID mopEntity = nullptr;
  // World fields
  jfieldID wEntities = nullptr;
  // RenderManager fields
  jfieldID rViewX = nullptr, rViewY = nullptr, rViewZ = nullptr;
  // Timer fields
  jfieldID tPartial = nullptr, timerSpeed = nullptr;
  // GuiContainer / Slot / Container fields
  jfieldID gSlot = nullptr, gContainer = nullptr;
  jfieldID slotNum = nullptr, contWindow = nullptr;
  // Raycast / interact fields
  jfieldID mcPointed = nullptr;  // Minecraft.pointedEntity
  jfieldID eRiding = nullptr;    // Entity.ridingEntity
  jfieldID mopHitVec = nullptr;  // MovingObjectPosition.hitVec
  jfieldID vecX = nullptr, vecY = nullptr, vecZ = nullptr;
  // AxisAlignedBB bounds (BridgeAssist edge offset)
  jfieldID boxMinX = nullptr, boxMinY = nullptr, boxMinZ = nullptr;
  jfieldID boxMaxX = nullptr, boxMaxY = nullptr, boxMaxZ = nullptr;
  // ItemStack fields
  jfieldID stSize = nullptr;  // stackSize (AutoSwap depletion check)
  // static singleton fields
  jfieldID blocksAir = nullptr, posOrigin = nullptr, faceDown = nullptr;
  jfieldID actRelease = nullptr;

  // methods
  jmethodID mcGetMc = nullptr, mcGetRenderMgr = nullptr;
  jmethodID pIsUsingItem = nullptr, pSetItemInUse = nullptr, pClearItemInUse = nullptr;
  jmethodID lGetHealth = nullptr, lGetHeld = nullptr;
  jmethodID eInvisible = nullptr, eSprinting = nullptr, eGetEyes = nullptr;
  jmethodID stGetItem = nullptr, stGetMeta = nullptr, potSplash = nullptr;
  jmethodID itStrVs = nullptr;  // Item.getStrVsBlock (AutoTool)
  jmethodID kGetCode = nullptr, kOnTick = nullptr, kSetState = nullptr;
  jmethodID nhSend = nullptr, mopBlockPos = nullptr, wBlockState = nullptr;
  jmethodID ibsGetBlock = nullptr, ctlWindowClick = nullptr, scrShift = nullptr;
  jmethodID ctlSyncItem = nullptr;  // PlayerControllerMP.syncCurrentPlayItem
  jmethodID vDist = nullptr, listToArray = nullptr;
  jmethodID c09Ctor = nullptr, c07Ctor = nullptr;
  // Raycast / interact methods
  jmethodID mcRenderView = nullptr, eGetLook = nullptr, vecAdd = nullptr;
  jmethodID eGetBox = nullptr, boxExpand = nullptr, boxGrow = nullptr;
  jmethodID boxIsVec = nullptr, boxIntercept = nullptr, wGetEntities = nullptr;
  jmethodID eCanCollide = nullptr, eBorder = nullptr, ctlExtReach = nullptr;
  jmethodID pIsSpectator = nullptr, eCanRide = nullptr, ctlAttack = nullptr;
  jmethodID pSwing = nullptr, wRayTrace = nullptr, eGetName = nullptr;
  jmethodID ccText = nullptr, vecCtor = nullptr, mopCtor = nullptr;
  // Movement batch methods
  jmethodID capsFlySpeed = nullptr, jumpM = nullptr, setSprintM = nullptr;
  jmethodID inWaterM = nullptr, inLavaM = nullptr, isSneakingM = nullptr;
  jmethodID setPosM = nullptr, activeFx = nullptr, fxAmp = nullptr, fxName = nullptr;
  jmethodID collIter = nullptr, collHas = nullptr, collNext = nullptr;
  jmethodID c03Ctor = nullptr, posCtor = nullptr;
  jmethodID boxCtor = nullptr;  // AABB.<init>(DDDDDD) (BridgeAssist)
  jmethodID wCollide = nullptr;  // World.getCollisionBoxes (BridgeAssist)
  jmethodID invSlot = nullptr;   // InventoryPlayer.getStackInSlot (AutoSwap)
  jmethodID isPotionActive = nullptr, reachDist = nullptr;

  // immutable singletons (global refs, resolved at Ensure)
  jobject airBlock = nullptr, originPos = nullptr, downFace = nullptr;
  jobject releaseAction = nullptr;
};

class Mc {
public:
  static Mc& Instance();

  // Caches all classes/IDs via env. Retries at most every 5s on failure.
  // False => not in a game JVM or version mismatch; modules must stay inert.
  bool Ensure(JNIEnv* env);
  bool Ready() const { return ready_; }
  // On-demand classes (world/player packets/gui/vec3/blocks): resolved once
  // the game has loaded them (i.e. in-world). Best-effort, never fails core.
  bool EnsureExtra(JNIEnv* env);
  bool ExtraReady() const { return extraReady_; }
  // Naming in use (decided at Ensure): notch (vanilla) vs searge (Forge).
  bool IsNotch() const { return notch_; }
  McIds& Ids() { return ids_; }

  struct Frame {
    JNIEnv* env = nullptr;
    jobject mc = nullptr, player = nullptr, world = nullptr, settings = nullptr;
    jobject attackKey = nullptr, useKey = nullptr, sprintKey = nullptr;
    jobject renderMgr = nullptr, screen = nullptr, mouseOver = nullptr;
    float partialTicks = 0.f;
    int displayW = 0, displayH = 0;
    float fov = 90.f, gamma = 1.f;
    bool hasFocus = false;
  };

  // Fills frame with fresh local refs; false when no Minecraft instance.
  // Must pair every true return with ReleaseFrame on the same thread.
  bool BeginFrame(JNIEnv* env, Frame& f);
  void ReleaseFrame(Frame& f);

  // Calls fn(env, entity) for every EntityPlayer in the world (playersOnly),
  // or every EntityLivingBase when playersOnly=false (KillAura mobs).
  // Entity ref is a frame-local temporary: do not retain.
  void EachPlayer(Frame& f, const std::function<void(jobject)>& fn,
                  bool playersOnly = true);

  // ---- field helpers (null-safe, defaults on null) ----
  static jdouble D(JNIEnv* e, jobject o, jfieldID id) {
    return (e && o && id) ? e->GetDoubleField(o, id) : 0.0;
  }
  static jfloat Fl(JNIEnv* e, jobject o, jfieldID id) {
    return (e && o && id) ? e->GetFloatField(o, id) : 0.f;
  }
  static jint In(JNIEnv* e, jobject o, jfieldID id) {
    return (e && o && id) ? e->GetIntField(o, id) : 0;
  }
  static bool Bo(JNIEnv* e, jobject o, jfieldID id) {
    return e && o && id && e->GetBooleanField(o, id);
  }
  static void SetD(JNIEnv* e, jobject o, jfieldID id, jdouble v) {
    if (e && o && id) e->SetDoubleField(o, id, v);
  }
  static void SetF(JNIEnv* e, jobject o, jfieldID id, jfloat v) {
    if (e && o && id) e->SetFloatField(o, id, v);
  }
  static void SetI(JNIEnv* e, jobject o, jfieldID id, jint v) {
    if (e && o && id) e->SetIntField(o, id, v);
  }
  static void SetB(JNIEnv* e, jobject o, jfieldID id, bool v) {
    if (e && o && id) e->SetBooleanField(o, id, v ? JNI_TRUE : JNI_FALSE);
  }
  // Returns a local ref (may be null); caller must DeleteLocalRef.
  static jobject Obj(JNIEnv* e, jobject o, jfieldID id) {
    return (e && o && id) ? e->GetObjectField(o, id) : nullptr;
  }
  static bool IsA(JNIEnv* e, jobject o, jclass cls) {
    return e && o && cls && e->IsInstanceOf(o, cls);
  }
  static bool Same(JNIEnv* e, jobject a, jobject b) {
    return e && a && b && e->IsSameObject(a, b);
  }

private:
  Mc() = default;
  bool ResolveCore(JNIEnv* env);
  bool ResolveExtra(JNIEnv* env);

  McIds ids_;
  std::mutex mutex_;
  bool ready_ = false;
  bool extraReady_ = false;
  bool notch_ = true;  // vanilla notch vs Forge searge (decided in ResolveCore)
  long long lastTryMs_ = 0;
  long long lastExtraTryMs_ = 0;
  jobject loader_ = nullptr;  // game class loader (global ref, cached)
};

}  // namespace sdk
}  // namespace raven
