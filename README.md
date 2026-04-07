# VEEX Engine

Minimal project setup for VEEX game engine.

## Build

1. Install dependencies:
   - macOS: `brew install cmake glfw`
   - Linux: `sudo apt install cmake libglfw3-dev` (or distro equivalent)

2. Configure + build:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

3. Run:

```bash
./Engine/VEEXEngine [path/to/Game/gameinfo.txt]
```

## Project structure

- `Engine`:
  - core engine executable
  - `src/` with Application/Client/Server
  - `include/veex` with public engine headers
- `Game`:
  - config files: `gameinfo.txt`
  - placeholder game runtime data

## Next features

- BSP loader / `.veexbsp` importer
- PBR renderer and material pipeline
- Lua scripting host
- custom tools: `VEEXBSP`, `VEEXRAD`
