# a05777Client 维护 / 交接文档

> 适用对象：接手继续开发的人，或三个月后失忆的自己。
> 约定：本文所有路径均为相对路径，以 `raven-dll/` 为根（`../` 表示其父目录）。
> 现状快照：32 个模块，Release 编译零警告，`tools/smoke.cpp` 自测全绿。
> 2026-09-30 player-lite 批：AntiAFK / Auto Tool / Fast Place / Auto Jump（轮询子集，包/事件/列表相关省略项见 §7），同上全绿。
> 2026-09-30 搭路批：Bridge Assist-lite / Auto Swap-lite（旋转/放置引擎与名单设置省略，见 §7），同上全绿。
> 2026-10-01 用户真机验证：overlay 跟随/ESP 成框/搭路基础行为正常；BridgeAssist 按用户要求改成边缘锁 + 右键门（预测版手感差已回退）。
> 2026-09-30 Overlay 诊断批：窗口迟到自愈 + DPI 感知 + 全屏判定 + `/api/debug/esp.overlay`，同上全绿。
> 2026-09-30 HUD 不显示攻坚：看门狗（`iters/stage`）+ 红屏测试（`POST /api/debug/overlay/flash`）+ 重注/计数/字体/stale 窗口修复；loadonly 干净，全量 smoke 待游戏卸注后跑。
> 2026-09-30 死锁根因修复：窗口/字体改由绘制线程建销（`CreateOwnedWindow/DestroyOwnedWindow`），Release 零警告 + 全量 smoke 全绿 + `flash` 端点实测 OK；待游戏内验证水印。
> 2026-09-30 ULW87 双链路：显式 `BLENDFUNCTION` + 去多余 `SLWA` + 5 连败切 PAINT；本机探针 `iters:46 stage:6 ulwFailed:false`（ULW 在此环境已通），待游戏内复验。
> 2026-10-01 HUD 美化批（极简静态款）：颜色/背景块/Side-Full 描边/左右对齐/XY 位置/阴影/小写开关 + `Show module info` 占位（C++ 模块暂无 info 源，后缀为空）；`HudStyle` 由 Hud 每帧推送、绘制线程缓存；Release 零警告 + smoke 全绿（含新 HUD PUT 断言）；待游戏内截图确认。
> 2026-10-01 配置导入导出（纯前端）：网页工具栏 `Export config`（GET 全量 → PUT 形 JSON 下载）/ `Import config`（读文件 → 逐模块 PUT，缺模块跳过计数）；零 C++ 改动；Release 零警告 + smoke 全绿；自动恢复仍无（见 §8.3.1）。
> 2026-10-01 网页浅色简约换肤（纯 CSS，无功能改动）：亮底白卡 + 绿 accent 开关 + 卡片投影；全 ASCII；Release 零警告 + smoke 全绿。
> 2026-10-01 版权头：`tools/add-license.py` 给自有 92 个 .h/.cpp 统一加 GPL 头（幂等，third_party 排除）；Release 零警告 + smoke 全绿。
> 2026-10-01 构建归位：`build/` 是从 `D:/zm1/H4` 整目录拷过来的，`CMakeCache` 仍指旧源码路径且残留 `raven_client.*` 旧工程名文件 + `H5/` 根下空 `a05777Client.log`；已删 `build/` +  stray log，用正确路径重配（`Visual Studio 18 2026 -A x64`），Release 零警告 + smoke 全绿（31 模块基线）。
> 2026-10-01 AutoBlock（用户点名 Full 版）：全量 12 设置项（5 slider + 7 bool，`DescriptionSetting` 分组纯 UI 无行为已省）；`Mc` 映射零新增（`sKeyUse/kSetState/kOnTick/pIsUsingItem/lHurtTime/mcScreen` 全复用）；发包 Lag 因无包钩子只能做定时 block-pause 近似、`Mouse/RightClick/UseItem/SendPacket` 事件取消与 `Force block animation` 未实现（见模块头 HONEST GAPS）；Release 零警告 + smoke 全绿（含新 `Auto Block` PUT 断言）；待游戏内真机验证。
> 2026-10-10 AutoBlock 实现重写：保留 12 个设置项，改为独立 C++ 轮询状态机；目标选择加入准星实体优先、玩家眼睛到目标 AABB 最近点距离、死亡/旁观/队友过滤；`Damaged` 使用 `lMaxHurt`（`maxHurtTime`）映射；按键状态增加缓存释放与世界/界面切换兜底。仍保留无包钩子、无事件总线和无第一人称渲染注入的诚实差距；Release 零警告，smoke 覆盖 12 项设置回显，待游戏内格挡验证。
> 2026-10-11 AutoBlock 完全重写：改为 `Idle/Blocking/AttackPause/LagPause` 四态；物理左键优先释放格挡，KillAura 在攻击前通过 AutoBlock 接口发 C07、清除本地使用状态并释放本模块持有的 use 键，避免 `isUsingItem` 门控吞掉攻击；移除伪造 `setItemInUse` 动画，默认 `Require Left mouse=false` 以允许目标在攻击间隔自动格挡；12 个设置项保持，`Force block animation` 仅保留配置兼容，动画由原版 use-key 路径处理。Lag 仍是定时暂停近似，不拦包。
> 2026-10-11 AutoBlock 改为攻击后短脉冲：移除“附近有目标就自动起格挡”，空闲时不再按住 use 键；手动攻击输入边沿和 KillAura 实际提交的攻击都会排入短延迟格挡，下一次攻击前 KillAura 先释放格挡并清理本地使用状态。默认最大格挡时长 50ms、Lag Chance 0%，避免常驻格挡压住攻击；保留原设置字段和状态机。已编译主 DLL，未运行 smoke，待用户游戏内验证伤害与格挡节奏。
> 2026-10-11 AutoBlock 再按参考客户端重写：依据 `../raven-src/keystrokesmod/module/impl/combat/Autoblock.java` 的右键总开关、手动格挡标记、目标/条件判断、受伤预测和 hold/lag 时序移植为 C++；新增游戏 WndProc 鼠标桥，持剑时接管右键事件，并在原版处理左键攻击前释放格挡。KillAura 仍在控制器攻击前释放 use 状态，Lag 攻击打断由攻击回调处理。没有复制 Java 源码；真实 Netty lag 仍不可用。
> 2026-10-11 用户真机确认 AutoBlock 当前功能正常；无包钩子造成的真实网络 hold 限制仍存在。
> 2026-10-11 HUD 调整：去掉品牌卡片和装饰，改为传统紧凑 ArrayList；白色 Arial 文字、长度排序、右侧对齐，阴影开、背景/描边默认关；HUD 自身不列出，移除常驻右下角水印；Release 与 smoke 通过，待游戏内视觉确认。
> 2026-10-11 HUD 常驻标识：HUD 启用时在左上角显示白字黑影 `a05777 Client v1.0.0`，即使没有其他启用模块也显示；保留传统紧凑 ArrayList，不增加卡片或计数。
> 2026-10-01 基础美化批（零新模块、零新映射）：HUD `Scale` 滑杆正式接入渲染（绘制线程按 scale 重建 Consolas 字号 + 等比行高/边距/侧栏，`HudStyle.scale` 由 Hud 每帧推送）；PlayerESP 新增 `Line width` 滑杆 1~3px（`EspBox.lineW`，同宽同色复用笔对象）；Release 零警告 + smoke 全绿（含 Line width PUT 断言）；待游戏内截图确认。
> 2026-10-01 网页换肤（用户设计）：`D:/zm1/web/index.html`（深色像素风）原样嵌入 `UI.h`（75KB，全 ASCII，`R"WEB()` 无冲突，smoke 标题断言通过）：分类 tab + 搜索/状态过滤 + 可折叠卡片 + 按键录制（click→按任意键，Esc 清除）+ Debug 检查面板（mc/esp 表 + 原始 JSON + 红屏 flash）+ toast + 导入汇总弹窗 + 离线预览 mock；API 零改动（PUT 形状/导入导出 v1 兼容）；Release 零警告 + smoke 全绿 + 服务端标记直验（module-grid/category-tabs/debug-section 等 9 标记全命中）。页面源码在仓外维护，改完贴回后用 `tools` 下拼接脚本重嵌（见 `UI.h` 头注）。
> 2026-10-01 键位系统：`Client::PollBinds` 每帧边沿检测各模块 bind（Win32 VK，0=无；鼠标键同样可用）；上升沿才过两道门——前台必须是游戏窗口、在游戏内无 GUI 界面（防浏览器/聊天框打字误触）；`Mc` 零新映射（复用 `mcScreen`），无 JVM 时 inert；网页录制/PUT 通路早已存在，零网页改动；Release 零警告 + smoke 全绿（含 bind PUT 回显断言，热键实按只能真机验证）。
> 真机状态：原版 1.8.9 + FPSMaster 1.8.9 Forge 均可注入；overlay 可视层在用户机器上仍在排查（见 §9.1，現已可按 `overlay` 段远程定界）。

---

## 1. 这是什么

`a05777Client` 是一个 **Minecraft 1.8.9 的 C++ DLL 注入式客户端**（内挂）：

* 把 `build/bin/Release/a05777Client.dll` 注入正在运行的游戏进程（`javaw.exe`），
* 通过 JNI 读写游戏内存/对象实现 25 个功能模块（PvP 战斗 + 移动 + 显示），
* **没有游戏内菜单**——所有调参走本机 `http://127.0.0.1:8080/` 的 REST + 网页。

参考原型是 `../raven-src/`（Raven / KeystrokesMod 的 Java 反编译源码，Forge 1.8.9 + Mixin，约 400 个 `.java`，136 个模块框架）。但本工程**不是移植 Forge 模组**，而是为 DLL 注入从零重写的 C++ 实现，只借用了它的模块设计、参数默认值和行为逻辑。

---

## 2. 设计理念演进（按时间顺序）

1. **起点：Forge 复刻计划。** 最初按 `../raven-src/` 的 `Module + Setting + ClickGui + Mixin事件 + Script/Profile` 五件套规划 Java 工程。
2. **转向 DLL 注入。** 用户要求：1.8.9、不走 Forge、C++ DLL、全功能重写。Forge/Mixin 路线作废，新建 `raven-dll/` 空工程。
3. **品牌定为 `a05777Client`**（DLL 名、日志前缀、HUD/网页标题统一）。
4. **砍掉游戏内菜单。** 不写 ImGui 游戏内 GUI，改 WinSock 自研 HTTP 服务 + 单文件网页调参：零第三方依赖、调参不用进游戏、手机/另一台电脑开网页也能调（仅回环地址，外部连不进来）。
5. **渲染零 hook。** 不碰 `wglSwapBuffers`/GL 上下文（风险高），ESP/HUD 走独立的 GDI 分层透传窗口（`WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST` + 色键），跟随游戏窗口。
6. **映射隔离。** 全部混淆名只允许出现在 `src/client/sdk/`（notch/searge 双命名，见 §4），业务模块只调干净 C++ API。
7. **无 JVM 也能跑。** 所有 JNI 入口判空：`tools/smoke.cpp` 把 DLL 当普通库加载，能完整自测 REST/网页/模块生命周期；进游戏才走 JNI 真路径。
8. **先保命后保真。** 和包拦截、事件 Mixin 强相关的功能一律不硬写（§8.2 有清单），能轮询实现的先轮询，能近似的注明近似，绝不编造“我实现了”。

---

## 3. 目录结构（相对路径）

```text
raven-dll/
  CMakeLists.txt                  # 构建定义：a05777Client(DLL) + injector + smoke
  MAINTENANCE.md                  # 本文档
  src/client/
    dllmain.cpp                   # DllMain：只起线程，loader-lock 下不干活
    core/
      Client.h/.cpp               # 总控：Start/Stop/100Hz Loop/OnFrame
      Module.h/.cpp               # 模块基类（enable/atomic/设置注册表）
      ModuleManager.h/.cpp        # 注册表 + TickAll/RenderAll（可重入锁）
      Setting.h                   # Bool/Slider/Key/Mode/Color 五种设置
    sdk/
      Jvm.h/.cpp                  # JVM 发现 + 本线程 JNIEnv（daemon attach）
      Mc.h/.cpp                   # notch/searge 双命名映射缓存 + 帧快照
      Raycast.h/.cpp              # 共享实体 raycast（Reach/HitBox/杀戮通用）
      Mappings.h                  # 早期 searge 名 archive（已被 Mc 取代，勿删，供对照）
    hooks/
      Hooks.h/.cpp                # 游戏窗口发现（打分制，类/标题压倒面积，启动器排除）+ opengl 预留
    render/
      Overlay.h/.cpp              # 帧计数 stub（历史遗留；HUD 不经此类绘制）
      OverlayWindow.h/.cpp        # GDI 分层 overlay：ESP 框/血条/HUD 面板与活动模块列表
    web/
      RestServer.h/.cpp           # WinSock HTTP + REST路由 + 手写 JSON
      UI.h                        # 单文件网页（内嵌，ASCII only 防 C4819）
    modules/
      Module.h 归 core；此处每个模块 h+cpp：
      Sprint/HUD/Velocity/NoSlow/AutoClicker/KeepSprint/Fullbright/PlayerESP/
      Reach/HitBox/KillAura/Fly/Speed/NoFall/SafeWalk/BHop/
      WTap/JumpReset/ClickAssist/Reduce/AimAssist/RodAimbot/Piercing/TPAura/RawInput/
       AntiAFK/AutoTool/FastPlace/AutoJump/BridgeAssist/AutoSwap/AutoBlock
      CombatUtil.h                # 战斗共享：按键/武器/瞄准数学/队友文本
      MoveUtil.h                  # 移动共享：setSpeed/药水/方块探针/边缘/虚空
    util/
      Logger.h/.cpp               # OutputDebugString + 文件日志（窄字符追加！）
  src/injector/
    main.cpp                      # 自动检测+LJGL识别+防重注+验活+--eject
  tools/
    smoke.cpp                     # 端到端自测：加载DLL→调REST→卸载，exit code 判定
    extract-mappings.ps1          # 从 joined.srg 提取所需映射（见 §5）
  third_party/
    jni/jni.h, jni_md.h           # 官方 OpenJDK 头（编译期 only，运行期用游戏自带 JVM）
    mcp/joined.srg                # MCP 1.8.9 notch<->searge 全量映射（2.7MB）
    mcp/needed.txt                # 已废弃的中间产物（保留供对照）
    mcp/mcp-1.8.9-srg.zip         # 下载原包
    mcp/fields.csv                # 下载失败的残留（404 页面，仅 1KB，可删）
  build/                          # CMake 生成目录（可删，命令重建）
    bin/Release/
      a05777Client.dll            # 注入本体
      injector.exe                # 注入器
      smoke.exe                   # 自测工具
```

`../backup/` 下是按批次的时间戳快照（`a05777Client-batch1-*`、`batch2`、`batch3`、`combat`、`pre*`、`aimfix`、`visual`…），每次大改前/后各一份，空间很小（每次约 10MB）。

---

## 4. 核心子系统详解

### 4.1 模块框架（`src/client/core/`）

* `Module`：`Name/Category/IsEnabled/Bind` + `OnEnable/OnDisable/OnUpdate/OnRender`。`enabled_` 是 `atomic<bool>`（游戏 tick 线程和 HTTP 线程都会读）。
* 设置注册表：派生类把成员 `BoolSetting/SliderSetting/ModeSetting/ColorSetting` 用 `RegisterXxx(&member)` 登记，网页自动生成控件——**加新设置零 UI 工作量**。
* `ModuleManager`：`RegisterDefaults()` 按批次 `Add`；`TickAll/RenderAll` 每 tick 调 enabled 模块；锁是 `recursive_mutex`（HUD 回调里会重入 `Snapshot()`，普通 mutex 会死锁）；`Find(name)` 供跨模块协作（WTap↔Sprint、KeepSprint↔Reduce、Reach→HitBoxes）。
* `Client::Loop` 保持 100Hz 调度：有有效游戏窗口时只向窗口线程投递合并后的 `WM_APP + 0x577` 帧消息，由窗口线程执行 JNI 模块；窗口不可用或投递失败时回退到 worker 线程。10ms 是采样率，不是 MC 本体的 20Hz tick。
* `Client::PollBinds` 跑在每帧 Tick 之后：先做无 JNI 的 `GetAsyncKeyState` 边沿检测，无边沿直接返回；有边沿才查前台窗口 + `mcScreen` 界面门，无 JVM 时 inert。

### 4.2 JVM 桥（`src/client/sdk/Jvm.*`）

* `Attach()`：`GetModuleHandleW(jvm.dll)` + `JNI_GetCreatedJavaVMs` 拿 `JavaVM*`。无 jvm.dll（smoke/单测）返回 false，调用方全部 inert。
* `EnvForCurrentThread()`：thread_local 缓存 JNIEnv，首次 `AttachCurrentThreadAsDaemon`（daemon 保证不卡 JVM 退出；退出时不 detach，进程级泄漏可接受）。

### 4.3 映射层（`src/client/sdk/Mc.*`，最关键的文件）

* **双命名**：`bool notch_`，启动时探测 `ave`（原版）还是 `net.minecraft.client.Minecraft`（Forge），之后全部字段/方法/签名二选一。`GET /api/debug/mc` 的 `naming` 字段直接暴露当前值。
* **类发现不用 `FindClass`**（血泪教训见 §6.3）：attach 的外部线程只能看见 bootstrap 类。做法是 `Thread.getAllStackTraces()` 枚举全部线程，取 `Client thread` 的 context loader（找不到就逐个试），再 `Class.forName(dottedName, true, loader)`。注意 forName 要**点分名**（`java.util.List` 不能写斜杠，§6.4）。
* **分两级**：`Ensure()` 解析开机就有类（core，失败则 5 秒重试）；`EnsureExtra()` 解析按需加载类（world/发包/GUI/vec3/方块，失败也 5 秒重试，core 不受影响）。`Mc::Frame` 每 tick 取新鲜 local ref，`BeginFrame/ReleaseFrame` 必须配对（防 local ref 表爆）。
* **extra 是“报全错”模式**：类/字段/方法逐个尝试、缺啥打全（`Mc::Ensure[extra]: ... missing: xxx`），最后汇总一行，不会只报第一个。core 保持 fail-fast。
* 重要字段勘误（都被坑过，见 §6.6）：`isFlying` 是 `wl/b`（field_75100_b），不是 `wl/e`；`theWorld` 声明类型是 `WorldClient(bdb)`；`field_75099_e` 按 adventure 门（allowEdit）理解使用，**未经证实**。
* SRG 里**没有构造函数**：C07/C09/C03/Vec3/MOP/BlockPos/AABB 的 `<init>` 描述符是按 MCP 签名手写的（`(I)V`、`(Lir$a;Lcj;Lcq;)V`、`(DDD)V`、`(Lpk;Laui;)V`、`(Z)V`、`(III)V`、`(DDDDDD)V`），**未经逐个证实**，出问题先查它们。
* 2026-09-30~10-01 新增映射（srg 对名）：core 层 `keyBindForward/Back/Left/Right`（avh Y/aa/Z/ab）、`rightClickDelayTimer`（ave/ap）、`mainInventory`（wm/a）、`getStrVsBlock`（zw/a）；extra 层 `syncCurrentPlayItem`（bda/n）、AABB 六界（aug/a~f）+ 构造、`getCollisionBoxes`（adm/a）、`getStackInSlot`（wm/a）、`stackSize`（zx/b）。core 保持 fail-fast：错一个全挂，smoke 先验。

### 4.4 共享 raycast（`src/client/sdk/Raycast.*`）

* 复刻原版 `EntityRenderer` 实体射线：视点眼睛 + look 向量 + AABB sweep + border 膨胀 + 骑乘处理 + 最近命中 + `dist<reach 且非生物非画框则丢弃` 的原版规则。
* `RayHit.entity` 是 global ref，调用方必须 `FreeRayHit`；`ApplyPointed` 写 `objectMouseOver + pointedEntity`；`LineOfSight` 用 `rayTraceBlocks` 判方块遮挡。
* Reach / HitBox / KillAura（实体遮挡门）共用。

### 4.5 战斗/移动共享头（`src/client/modules/CombatUtil.h` / `MoveUtil.h`）

* `CombatUtil.h`：左右键状态（Win32）、**前台判定（用真实窗口句柄，不用标题匹配，§6.8）**、武器判定（剑/斧）、vanilla 瞄准数学（`AnglesTo/LookDir/AngleBetween/WrapDiff`，和 ESP 投影同基）、队友文本（名字前 2 字符颜色码启发式）、怪物包名前缀判定。注意 KillAura 自带一份拷贝（历史原因，别动它）。
* `MoveUtil.h`：`HSpeed/SetStrafeSpeed`（Fly.setSpeed 原样移植）、`MovingInput`、`SpeedAmplifier`（真药水等级）、`BlockAt/IsAirBlock/EdgeOfBlock/OverVoid`（方块探针；`OverVoid` 整列扫描，调用方只在下坠时调）。

### 4.6 Hook 层（`src/client/hooks/`）

* `FindMinecraftWindow()` / `PickBestWindow()`：打分制（2026-09-30 改：之前纯比面积，大启动器赢小游戏是跟错窗口的根因）。`exact("Minecraft 1.8.9")+100万 > LWJGL类+50万 > FPSMaster+20万 > 含Minecraft+5万 > 面积/4096`；标题含 `launcher/mojang` 直接排除；客户区 <50x50 排除（隐藏 LWJGL 小窗口，§6.9）。选中打日志（标题+类+尺寸+分）。
* `RefreshWindow()`/`RevalidateWindow()`（2026-09-30 合并为智能路径，500ms 节流）：句柄无效必换；有效但出现严格更高分窗口也换（启动器先匹配、游戏后开的自愈）。`WindowDesc()` 给 `/api/debug/esp.overlay.win` 用。

### 4.7 渲染 overlay（`src/client/render/OverlayWindow.*`）

* 独立窗口 + 独立 30Hz 线程，跟随游戏客户区（`GetClientRect + ClientToScreen`），`UpdateLayeredWindow + ULW_COLORKEY（品红）` 呈现，`WS_EX_TRANSPARENT` 点透。**窗口与字体必须由绘制线程创建/销毁/操作**（2026-09-30 血泪：之前由 Client 线程 `Start` 里建，绘制线程首帧 `SetWindowPos` 跨线程死锁，`stage` 永远停 4）。
* 内容：ESP 框+血条、HUD 活动模块列表，以及 HUD 启用时左上角常驻的 `a05777 Client v1.0.0` 白字黑影标识（无其他活动模块时仍显示）；overlay 状态通过诊断 API 与红屏测试确认。
* HUD 和 ESP 共用，引用计数（`users_` + `startMutex_`），`Shutdown()` 专供 DLL 卸载（否则 eject 后线程跑飞必崩）。
* DPI（2026-09-30 新增）：`Start` 时动态启用 PMv2（`SetProcessDpiAwarenessContext`，回退 `SetProcessDPIAware`，全动态加载兼容 Win7）；`Loop` 每帧 `GetDpiForWindow`（取不到=96），首帧日志 `dpi=` + 缩放比；`OverlayDiag` 上报。
* 全屏判定（2026-09-30 新增）：客户区≈显示器尺寸（±4px）即判 `fullscreen=true`，首帧 warn“独占全屏盖不住，请用窗口/无边框”；心跳与 `/api/debug/esp` 均带该位。
* 字体回退：Arial 建失败则用 `DEFAULT_GUI_FONT`（只用不删，`fontOwned_` 标记归属，`Teardown` 不删系统 stock 对象——之前会误删）。HUD Scale 变化时在绘制线程重建字号（`fontScale_` 缓存，同线程建销，跨线程不动字体）。
* 重注存活（2026-09-30 修）：`RegisterClassEx` 遇 `ERROR_CLASS_ALREADY_EXISTS` 复用旧类（eject 不反注册，owner 是宿主 exe）；`Start` 失败路径回滚 `users_` 计数（之前失败一次涨一次）。
* 看门狗（2026-09-30 新增，HUD 不显示攻坚）：`Loop` 每帧写 `diag_.stage`（1=pump，2=窗口坏/最小化，3=客户区过小，4=rect 就绪，5=已绘制，6=已呈现）+ 成功呈现计数 `iters`；`POST /api/debug/overlay/flash` 触发 5 秒全屏红色测试图（`FlashTest`），红屏可见=窗口链路通、问题在内容/文字渲染，红屏不可见=分层窗口根本没上屏。
* 呈现双链路（2026-09-30 新增）：主 `ULW`（`UpdateLayeredWindow + ULW_COLORKEY`，显式 `BLENDFUNCTION`，创建时不再调多余的 `SetLayeredWindowAttributes`）；`ULW` 连败 5 次自动切 `PAINT` 回退（`SetLayeredWindowAttributes` 色键 + `WM_PAINT BitBlt`，`diag_.paintMode` 上报，`esp.overlay.paint` 可见）。`WM_PAINT` 由 `GWLP_USERDATA` 回调进 `OnPaint`，同属主线程。
* **已知限制**：独占全屏盖不住（原理限制，只能窗口/无边框）；DPI 已做感知+日志，但多屏混 DPI 下仍以游戏所在窗 DPI 为准。
* 诊断：第一帧打几何日志（含 dpi/全屏位）；ULW/DIB 失败打 error（ULW 错误码记入 `OverlayDiag`）；每 10 秒心跳（`beat visible=... boxes=N lines=M ... dpi=U`）；`GET /api/debug/esp` 可拉当前框+相机+`overlay` 诊断对象。

### 4.8 Web 层（`src/client/web/`）

* `RestServer.cpp`：WinSock 单线程 accept 循环（select 200ms），只绑 `127.0.0.1:8080`。手写 HTTP 解析 + JSON 拼装/极简提取（`FindField/BodyBool/BodyNum/BodyStr`）。路由见文件头注释：`/`、`api/status`、`api/modules`、`api/modules/{name}`（GET/PUT）、`.../toggle`（POST）、`api/debug/mc`、`api/debug/esp`（含 `overlay:{running,hasWindow,windowValid,minimized,fullscreen,rect[x,y,w,h],dpi,ulwFailed,ulwErr,surfFailed,beats,lines,iters,stage,flash,paint,win}`，只增字段不破坏 smoke）、`POST api/debug/overlay/flash`（红屏 5 秒）。
* `UI.h`：单文件网页（内嵌，ASCII only 防 C4819）。页面源码在仓外 `D:/zm1/web/index.html` 维护（用户设计的深色像素风：分类 tab/搜索过滤/可折叠卡片/按键录制/Debug 面板/toast/导入汇总/离线 mock），嵌入时原样拼进 `R"WEB()`（75KB，禁 `)WEB"` 与非 ASCII，smoke 只断言标题）。工具栏 `Export config` / `Import config` 做手动配置备份恢复（纯前端逐个 PUT，无服务端文件 IO，格式见本段 PUT 约定）。**全 ASCII**（MSVC C4819 警告教训）。
* PUT 约定：`{enabled, bind, bools:{名:bool}, sliders:{名:num}, modes:{名:index|选项串}, colors:{名:"#rrggbb"}}`；滑杆服务端钳制+步长吸附。
* 线程安全：handler 里拿 manager 递归锁；`ModuleDetailJson` 调用方必须持锁。

### 4.9 注入器（`src/injector/main.cpp`）

* 默认无参：等游戏（最多 90 秒）→ 按“窗口+LWJGL 双命中”打分选择 → 拒绝 32 位目标 → 注入 → **回验面板**（`PanelUp` 直连 8080 找 `a05777Client` 字样，15 秒）。
* 防重注：目标已加载同名 DLL 则跳过（解决 refcount 叠加导致卸不掉）。
* `--list / --all / --pid / --eject [pid]`；DLL 默认取注入器同目录。注意 eject 后等成功再重编/重注。

### 4.10 自测（`tools/smoke.cpp`）

* 独立进程：`LoadLibrary` → 等 8080 → 查 status/modules → toggle Sprint → PUT（slider/mode/color）→ 首页标题 → 恢复现场 → `FreeLibrary`，全过 exit 0。
* **覆盖上限**：无 JVM，只能验证框架/REST/生命周期，**验证不了任何 JNI 行为**。stdout 必须无缓冲（曾因缓冲吞掉崩溃前的输出，见 §6.7）。

---

## 5. 依赖与来源（构建/运行）

* 编译器：VS BuildTools（实测 18.x / MSVC 14.51， require ≥ VS2022 级别 + C++20），CMake ≥ 3.20（VS 自带 4.x 可直接用），Windows SDK 10+。`CMakeLists.txt` 里 `Visual Studio 18 2026 -A x64` 按实际安装改 generator 名。
* 运行：64 位 `javaw.exe`（原版 1.8.9 用 notch，Forge 1.8.9 用 searge，自动识别），8080 端口空闲。
* 第三方源码（已 vendoring 进仓库）：
  * `third_party/jni/jni.h`、`third_party/jni/jni_md.h`：官方 OpenJDK raw（`raw.githubusercontent.com/openjdk/jdk`，`src/java.base/...` 路径）。
  * `third_party/jni/jvmti.h`：**没拿到**（master 路径 404，jdk8u 路径 404，本地无残留）→ 决定绕开 JVMTI（§4.3 loader 方案）。后人若要走 JVMTI 重传路线，先装 Temurin JDK 取 `include/jvmti.h`。
  * `third_party/mcp/joined.srg`：Forge 官方 Maven（`maven.minecraftforge.net/de/oceanlabs/mcp/mcp/1.8.9/mcp-1.8.9-srg.zip`），notch↔searge 全量真理表。
  * `tools/extract-mappings.ps1`：从 `joined.srg` 抽取所需条目的脚本（注意 FD/MD 行是斜杠格式 `ave/h`，CL 行才是空格，直接 `Select-String` 会漏，脚本里已处理）。
* `tools/add-license.py`：给自有 `.h/.cpp` 批量加 GPL 版权头（幂等，已含跳过逻辑；`third_party` 排除）。新加文件后跑一遍即可。
* 无 vcpkg、无 NuGet、无其他库：`opengl32/user32/gdi32/ws2_32` 全是系统库。

---

## 6. 踩坑记录（按时间顺序，都有 ticket 级细节）

1. **无编译器**：PATH 里没有 cl/cmake/msbuild，但 advocacy：BuildTools 藏在 VS Installer 下（`.../18/BuildTools/...`），CMake/Ninja 在 `Common7/IDE/CommonExtensions/Microsoft/CMake/` 里。用 `-G "Visual Studio 18 2026" -A x64` + `cmake --build` 全程不用配环境变量。
2. **`ccs=UTF-8` fastfail**：日志 `fopen "a, ccs=UTF-8"` 后用窄字符 `fputs` 写 → UCRT invalid-parameter → Watson `0xC0000409`（WER 定位到 `ucrtbase.dll`）。修：纯 `"a"` 模式（内容本就是 ASCII，文件天然 UTF-8）。
3. **FindClass 看不见游戏类**：注入线程只能看见 bootstrap 类（游戏类在 LaunchClassLoader 下）。改 `Class.forName + Client thread 上下文 loader`，全线程兜底。这是早期“功能全没用”的根因。
4. **forName 点分名**：`Class.forName("java/util/List")` 必挂，要 `java.util.List`（`$` 内部类不动）。
5. **theWorld 声明类型**：不是 `World(adm)`，是 `WorldClient(bdb)`（`client/multiplayer` 包，之前按 `world/WorldClient` 查漏掉了）。
6. **isFlying 读错字段**：之前读 `wl/e`，SRG+Fly/NoFall 源码交叉确认飞行标志是 `wl/b`（field_75100_b）。`wl/e` 暂按 allowEdit 理解使用（**未经证实**，见 §9.3）。
7. **输出缓冲吞崩溃**：smoke 定向管道时 stdout 全缓冲，崩溃/超时杀进程导致零输出。修：`setvbuf(_IONBF)` + 缩短等待 + WER 查 faulting module。
8. **HTTP 丢最后一行头**：`nl >= hEnd` 把末行 header（恒为 Content-Length）跳过 → PUT body 恒空。修 `nl > hEnd`。
9. **标题匹配前台失效**：FPSMaster 标题无 "Minecraft"，`ForegroundIsGame` 恒假 → Require mouse 类全灭。改真窗口句柄比对。
10. **`DrawFrame` 提前 return**：`boxes.empty()` 直接返回，HUD 文字永不绘制。修：框/字/水印各自独立绘制。
11. **窗口抓错**：`FindWindow(L"LWJGL")` 可能命中隐藏小窗口 → overlay 永远跳过。改最大面积 + 打日志。
12. **LNK1104 文件锁**：游戏占着 DLL 时编不过。流程：`--eject`（成功才继续）→ 编 → 注。重复注入会叠 refcount（已加防重注），僵尸 8080 监听 + 残留模块多半是这么来的。
13. **winsock.h 冲突**：`windows.h` 先于 `winsock2.h` 且缺 `WIN32_LEAN_AND_MEAN` 必炸。所有网络目标统一定义。
14. **`Potion.jump` 查错类**：写到了 `ItemPotion(aai)` 上，正确是 `Potion(pe)`，曾导致整 tier 挂。
15. **eBorder 方法/字段混淆**：`func_70111_Y` 是方法（`CallFloatMethod`），一度当字段读。
16. **Raycast 距离平方/真值混用**：统一用真距离（`std::sqrt` 一次）， riders/inside 分支与 Java 语义对齐。
17. **AimAssist 抖动**：multipoint/random 每 tick 重掷 → 改目标切换时单掷 + 时间指数趋近（`1-exp(-λdt)`）。
18. **AimAssist.h 丢 `};`**：级联上百错，根子永远先看第一个错。
19. **Mc.h `potion` 重名**：ItemPotion 类与 Potion 类 id 撞名，拆成 `potion` / `potionCls`。
20. **UI 非 ASCII**：`·/…` 触发 C4819，全改 ASCII。
21. **整目录拷贝忘清 `build/`**：`H5/raven-dll/build/CMakeCache.txt` 里 `CMAKE_HOME_DIRECTORY` 仍指 `D:/zm1/H4/raven-dll`，且残留旧工程名 `raven_client.vcxproj/raven_dll.slnx/raven_client.dir` + `H5/` 根下空 `a05777Client.log`。修：删 `build/` + stray log，用正确源码路径重配（`Visual Studio 18 2026 -A x64`），`build/` 可删重建（见 §3）。教训：拷目录前先删 `build/`，或收工快照时排除 `build/`（本次 `preautoblock` 备份即排除 `build/`、只另存三件套）。

---

## 7. 模块现状（32 个，`src/client/modules/`）

| 模块 | 类别 | 状态与备注 |
|---|---|---|
| Sprint | movement | 前进疾跑键强制；后/侧/使用物品全向需 movement hook，未做 |
| HUD | render | 传统紧凑 ArrayList：白色文字、按名称长度排序、右侧对齐；默认无背景/描边，有阴影；左上角常驻 `a05777 Client v1.0.0`；颜色/缩放/位置/对齐/大小写可调；排除 HUD 自身，无品牌卡片或模块计数 |
| Velocity | combat | 保真：hurt 边沿 + H/V/Chance/准星/S 键门 |
| NoSlow | movement | 输入还原 + Beta 三连包；setJump 死代码省略 |
| Auto Clicker | combat | CPS 调度 + 背包 windowClick（**背包路径未真机验证**）；adventure 门按 allowEdit 理解 |
| Keep Sprint | movement | 准星目标 hurt 边沿近似（无攻击事件）；Reduce 开时让路 |
| Fullbright | render | 自创（Raven 无此模块）：gamma 钳制+恢复 |
| PlayerESP | render | 2D 框+血条+颜色/彩虹/距离/自身/隐身 + 线宽 1~3px；骨骼/描边/圆环/阴影/队友色/受击红/名牌 deferred；投影自算（FOV 近似、眼高 1.62/1.54），**对齐未视觉验证** |
| Reach | combat | 左键边沿 raycast 重写准星；用 HitBox 倍率（与 Java 一致） |
| Hitboxes | combat | 同上触发，倍率膨胀；GL 描框/好友检查省略 |
| Kill Aura | combat | **单目标子集**：4 排序/FOV/CPS/挥砍+直伤/Lock-view 平滑；Silent≈Lock；无多目标/切延迟/hitMap；队友=名字颜色前缀启发式；不导出 target 给他用 |
| Fly | movement | Vanilla/Fast/Fast2；Walk（碰撞钩子）砍掉；Fast 第二分支按意图用 sneak（Java 疑似笔误写了两次 jump） |
| Speed | movement | Vanilla 保真；Float-lite（本地 tick 计数+真药水等级，无跨模块联动、无雪/地毯特例） |
| NoFall | movement | 四档全走 C3 直发（Spoof/NoGround 无包钩子的等效实现）；adventure/回放门省略 |
| SafeWalk | movement | 边缘 sneak 键强制+延迟/倍率/方块/抬头门；GUI 同步省略 |
| BHop | movement | Strafe/Ground/9/8/7 tick 全有；伤害加速组在 Java 里就是死的，已砍；转向/跨模块检查省略 |
| WTap | combat | 命中边沿+延迟/冷却/概率状态机；经 `IsResetting()` 直连 Sprint（对标 Java stopSprint） |
| Jump Reset | combat | maxHurt 上跳边沿 + 鼠标/朝向/准星/概率/药水门；FOV 用准星门近似 |
| ClickAssist | combat | Win32 合成补刀 + 30ms 自回声屏蔽 + 5CPS 门 |
| Reduce | combat | 同 KeepSprint 触发路径，reduction 公式原样 |
| Aim Assist | combat | 目标选择 + 粘性瞄点 + 时间指数趋近；Silent≈Normal；keepMoveDirection 省略；实体遮挡用 Raycast 近似 |
| RodAimbot | combat | **纯瞄准**（取消右键+手动重发做不到无事件钩子）；鱼竿/鱼钩/预测 tick 全有 |
| Piercing | combat | 每 tick 重写准星；hurt/health 排序坍缩为最近命中（sortMode 设置已删） |
| TPAura | combat | 受伤重随偏移 + 传送拍人；好友/AntiBot 省略 |
| Raw Input | combat | 原生等效：开关 Windows 指针精确度（非 JInput 方案，已注明） |
| AntiAFK | player | **子集**：5 档移动 + 4 档转头 + 跳/碰墙跳/换物品 + 随机增量/延迟；Random clicks（要点鼠标，无 accessor）与开火屏蔽省略，无 GUI 时才动 |
| Auto Tool | player | **子集**：按住左键按 `getStrVsBlock` 切最优热键槽 + 按设置切回；白/黑名单与手持名单（无列表设置类型）、spoof 与滚轮取消（要事件钩子）省略 |
| Fast Place | player | 右键按住写 `rightClickDelayTimer`（0~3 档 + 方块/俯仰/激活门）；C08 撤包（要发包钩子）与黑名单省略 |
| Auto Jump | player | 落地 + 移动中 + 水平碰撞 → `jump()`；无设置（和 Java 一样）；只在站地时触发所以顶不满飞不起来 |
| Bridge Assist | player | **子集**：脚悬空量（真 AABB 碰撞盒，0=安全在内）超 `Edge offset` 才按潜行 + 右键放置门（不按右键不碰潜行键，原生 SHIFT 照蹲）+ `Edge lock`（默认开）+ 延迟松开/跳跃窗口 + 3 条件门；教训：读物理 SHIFT 防自锁、offset 是悬空量。预测版（Lookahead+Hysteresis）手感差已回退，见备份。模拟预测、Pre place 瞄准、C08 追踪省略 |
| Auto Swap | player | 搭路时手里方块用完，按轮询追踪的同种方块从热键栏补（300ms 内不重切）；白名单（无列表设置）、C08 追踪省略 |
| Auto Block | combat | 参考 `raven-src/.../Autoblock.java` 移植为纯 C++/JNI；右键总开关和 manual-block 状态；WndProc 接管持剑时右键，在左键消息交给游戏前发 C07 并释放 use-key；KillAura 攻击前同样释放；Damaged 预测与最大持有时间按 50ms tick；Lag 仅本地计时，不 hold 网络包；默认 `Require Left mouse=true`；用户 2026-10-11 确认当前功能正常 |

---

## 8. 明确没做的

### 8.1 包拦截类（现在架构下做不了，做就是骗人）

HitSelect（取消攻击）、AntiKnockback（收包改；motion 侧已有 Velocity）、Displace / KnockbackDelay / LagRange（lag 系统要暂存/延迟收发包）。AutoBlock 已通过 C++ 状态机和游戏 WndProc 接管本地鼠标时序；仍没有 C02 发送事件或 Netty pipeline hook，因此 Lag 只能本地计时，不能 hold 网络包。

### 8.2 没开动的模块（按 `../raven-src/keystrokesmod/module/impl/`）

* movement 剩 12：Boost, Dolphin, InstantStop, InvMove, LongJump（432 行火球助推，单独一批）, MovementFix, NullMove, Stasis, StopMotion, Teleport, Timer, VClip（+ Fly-Walk 已砍）
* player 剩 14 个：AntiFireball, BedAura, Blink, BlockIn, Clutch, DelayRemover, FakeLag, FastMine, Freecam, GhostHand, HideWindow, InvManager, NoRotate, WaterBucket（+ NoFall✓/SafeWalk✓/AntiAFK✓/AutoTool✓/FastPlace✓/AutoJump✓/BridgeAssist✓/AutoSwap✓已做；BlockIn/Clutch 完整版要旋转+放置引擎，另立项）
* render 剩 31 个（HUD/PlayerESP/Fullbright 已做 3）
* minigames 10 / fun 4 / other 6 / client 5（Gui/Settings/Relationships/Profile/ProfileManager 全套）/ world 2 / scripts 全套 / keystroke 按键显示：都没动

### 8.3 基础设施缺口（按痛排）

1. **配置持久化（半自动）**：网页已有 `Export/Import config` 手动备份恢复（2026-10-01，纯前端 JSON）；**重启自动恢复仍没有**，每次重注后需手动 Import。这是用户第一个会骂的（剩下一半）。
2. **键位系统（已做，2026-10-01）**：网页按键录制 + `PUT bind` 早已通；`Client::PollBinds` 每帧边沿检测、上升沿 + 前台门 + 无界面门才 `Toggle`；同键多模块会一起切；实按热键待真机验证。
3. 命令行/Profile/Script/Keystroke：`../raven-src` 里都有，C++ 侧零。
4. 反作弊对抗、Hypixel 专项行为：零（当前全是单机可测逻辑）。

---

## 9. 已知 open issues（2026-10-01 更新）

1. **overlay 跟窗**（已解决，用户真机确认）：打分制上线后水印贴游戏客户区、`win` 显示 FPSMaster。最小化隐藏逻辑随同恢复。
2. **PlayerESP 前后反**（已解决，用户真机确认）：`vz = d·look` 上线后正对成框、背对消失。左右未见镜像。
3. **ESP 投影视觉验证通过**（2026-10-01 用户真机：一切正常）。
4. **未真机验证**：NoSlow-Beta 三连包、C03 系列、背包 windowClick、TPAura 传送、AutoClicker 破方块分支、新 6 模块（AntiAFK/AutoTool/FastPlace/AutoJump/BridgeAssist/AutoSwap，仅框架自测过）、HUD 本轮新外观（Release 与 smoke 通过，待游戏内视觉确认）。AutoBlock 当前功能已由用户确认正常。
5. **未经证实的假设**：`field_75099_e`=allowEdit；7 个 `<init>` 描述符（SRG 不含构造器，按 MCP 签名手写，新增 AABB）；`isShiftKeyDown` 是 static。
6. **BHop Speed 默认值 2.0 超出 max 1.2**：Java 原样保留（疑似 Raven 数据 bug），网页滑杆首次拖动会钳制。

---

## 10. 日常迭代流程（交接重点）

```text
改代码 → cmake --build ... --config Release → smoke.exe（一票否决）
      → 老 esk：injector.exe --eject（等成功，文件锁是常态）
      → 双击 injector.exe（自动等游戏/识别/验活，看 panel is up）
      → 进存档 → /api/debug/mc（mcReady/mcExtra/naming/inGame）
      → 网页开模块试 → 有问题贴 Mc::Ensure 行 + debug 接口
      → 收工前 robocopy 快照一份到 ../backup/a05777Client-<主题>-<日期>/
```

* smoke 在 `build/bin/Release/` 下跑，DLL 传参给它；冒烟挂了先看 exit code，再看控制台前几行（已无缓冲）。
* 日志在 DLL 旁边 `a05777Client.log`（相对路径，游戏 CWD 不可写所以不放游戏目录）。
* 加新模块三步：`modules/` 加 h+cpp → `CMakeLists.txt` 加文件 → `ModuleManager.cpp` 注册（smoke 的 `want[]` 顺手加断言）。
* 加新设置：`Setting.h` 类型 → `Module.h` 注册表 → REST（`ModuleDetailJson` + PUT 解析）→ `UI.h` 控件，四处，缺一网页就看不见。
* 加新映射：先 `tools/extract-mappings.ps1` 查 `third_party/mcp/joined.srg` 定 notch 名 → `Mc.h` 加 id → `Mc.cpp` 对应 tier 表加条目（core 失败全挂，extra 缺啥报啥）。

---

## AutoBlock 当前实现（2026-10-11，参考逻辑移植）

`Auto Block` 是主 DLL 内的纯 C++/JNI 实现，不加载 Java bridge、`.class` 文件或 `bridge-classes/`。逻辑参考 `../raven-src/keystrokesmod/module/impl/combat/Autoblock.java`，不复制 Java 源码。保留 `Idle`、`Blocking`、`AttackPause`、`LagPause` 状态，但按参考实现的输入门控和手动格挡分支重写。

* 保留 5 个 slider + 7 个 bool，默认值恢复参考客户端：Maximum Hold 150ms、Lag Chance 100%、Require Left mouse=true、Require right mouse=false。右键是格挡总开关：按住右键且没有攻击意图时手动格挡，不要求目标；松开右键时停止并清理状态。
* 攻击意图采用参考规则：物理左键，或启用且不要求物理按键的 KillAura 在范围内有目标。手动左键期间保持 use 状态释放；右键事件由游戏 WndProc 接管，持剑且无 GUI 时不让原版右键重复进入使用状态。
* 原生 WndProc 在转交左键消息前调用 C++ AutoBlock 释放路径，排入 C07 `RELEASE_USE_ITEM`、清除本地 `isUsingItem` 并释放本模块的 use-key；KillAura 仍在 controller attack 前调用 `PrepareAttack`。这是针对参考客户端 `PrePlayerInteractEvent`/鼠标事件取消的 native 时序适配。
* `Damaged` 按参考实现以 `round(Maximum Hurt Time / 50ms)` 精确匹配预测 tick，并在再次受伤时停止格挡；Maximum Hold 按 50ms tick 向上取整。Lag Chance/Max Duration 复用参考的总时长关系，但没有 packet-send hook，只能本地暂停格挡，不能 hold outbound 包；`Prevent delaying attacks` 可在 KillAura 攻击时提前结束该暂停。
* 目标优先准星实体，回退到范围内最近玩家，并过滤死亡、旁观和可识别队友。`Force block animation` 保留配置兼容；没有第一人称渲染注入。
* 所有实现仍是 C++/JNI，编译进主 DLL。用户已确认当前版本游戏内功能正常；完整设置组合和卸载流程未逐项回归。

验证：2026-10-11 用户反馈当前版本游戏内功能正常。本轮 Release 编译已生成主 DLL，未运行 smoke；真实网络包 hold 仍受缺少 Netty/C02 hook 限制。
