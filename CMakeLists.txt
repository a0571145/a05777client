cmake_minimum_required(VERSION 3.20)
project(a05777Client CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# x64 only: target is javaw.exe (64-bit) running MC 1.8.9
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(WARNING "a05777Client targets 64-bit javaw; configure with -A x64")
endif()

add_library(a05777Client SHARED
  src/client/dllmain.cpp
  src/client/util/Logger.cpp
  src/client/core/Module.cpp
  src/client/core/ModuleManager.cpp
  src/client/core/Client.cpp
  src/client/sdk/Jvm.cpp
  src/client/sdk/Mc.cpp
  src/client/sdk/Raycast.cpp
  src/client/hooks/Hooks.cpp
  src/client/render/Overlay.cpp
  src/client/render/OverlayWindow.cpp
  src/client/web/RestServer.cpp
  src/client/modules/Sprint.cpp
  src/client/modules/Hud.cpp
  src/client/modules/Fullbright.cpp
  src/client/modules/AutoClicker.cpp
  src/client/modules/AutoBlock.cpp
  src/client/modules/AntiAFK.cpp
  src/client/modules/AutoJump.cpp
  src/client/modules/AutoTool.cpp
  src/client/modules/AutoSwap.cpp
  src/client/modules/BridgeAssist.cpp
  src/client/modules/FastPlace.cpp
  src/client/modules/KeepSprint.cpp
  src/client/modules/Velocity.cpp
  src/client/modules/NoSlow.cpp
  src/client/modules/PlayerESP.cpp
  src/client/modules/Reach.cpp
  src/client/modules/HitBox.cpp
  src/client/modules/KillAura.cpp
  src/client/modules/Fly.cpp
  src/client/modules/Speed.cpp
  src/client/modules/NoFall.cpp
  src/client/modules/SafeWalk.cpp
  src/client/modules/BHop.cpp
  src/client/modules/WTap.cpp
  src/client/modules/JumpReset.cpp
  src/client/modules/ClickAssist.cpp
  src/client/modules/Reduce.cpp
  src/client/modules/AimAssist.cpp
  src/client/modules/RodAimbot.cpp
  src/client/modules/Piercing.cpp
  src/client/modules/TPAura.cpp
  src/client/modules/RawInput.cpp
)

target_include_directories(a05777Client PRIVATE src/client third_party/jni)
target_link_libraries(a05777Client PRIVATE opengl32 user32 gdi32 ws2_32)
target_compile_definitions(a05777Client PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX A05777_CLIENT_EXPORTS)

add_executable(injector
  src/injector/main.cpp
)
target_link_libraries(injector PRIVATE user32 ws2_32)
target_compile_definitions(injector PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

# Dev-only end-to-end smoke test (loads the DLL, exercises :8080, unloads).
add_executable(smoke
  tools/smoke.cpp
)
target_link_libraries(smoke PRIVATE ws2_32)
target_compile_definitions(smoke PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

set_target_properties(a05777Client injector smoke PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
