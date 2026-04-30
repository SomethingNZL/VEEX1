# VEEX Engine

Minimal project setup for VEEX game engine.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

### Third-Party Licenses

#### Other Third-Party Libraries
- **GLFW** (zlib/libpng License) - Window management
- **GLAD** (Public Domain) - OpenGL loader
- **GLM** (MIT License) - Mathematics library
- **miniaudio** (Public Domain/MIT) - Audio playback
- **stb_image** (Public Domain) - Image loading
- **stb_truetype** (Public Domain) - Font rendering
- **VTFParser** (MIT License) - VTF texture format parsing
- **MikkTSpace** (zlib/libpng License) - Tangent space generation

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

