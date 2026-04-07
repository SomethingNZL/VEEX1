# VEEX Engine Development Plan

## Goal
Create a modular cross-platform single-player engine with separate server/client modules and a minimal startup that matches ARCH.txt architecture.

## Milestones

1. Project scaffolding (done)
   - CMake root + Engine/Game targets
   - gameinfo files

2. Core application and loop
   - `Application` class: init GLFW, main loop, server tick + client render.
   - Fixed timestep server logic + uncapped render.

3. Basic server and client modules
   - Server: AI/physics/nav placeholders.
   - Client: window, GL state, rendering pipeline stub.

4. Config and asset metadata
   - `gameinfo.txt` parser
   - `gameinfobin.txt` parser

5. Platform abstraction
   - Window/input + OpenGL flags for macOS/Linux/Windows.

6. Future stubs
   - Renderer subsystem (PBR path).
   - BSP loader and procedural navmesh.
   - Scripting host (Lua 5.4 integration). 

## Execution steps (conducted)

1. Create `Engine/include/veex` headers:
   - `Application.h`, `Server.h`, `Client.h`, `Config.h`, `Logger.h`
2. Create implementation files in `Engine/src`.
3. Update `Engine/src/main.cpp` to use `veex::Application::Run()`.
4. Update `Engine/CMakeLists.txt` to compile all new sources and link dependencies.
5. Add short `README.md` describing next commands to build and run.
