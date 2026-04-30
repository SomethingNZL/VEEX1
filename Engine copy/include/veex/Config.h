#pragma once
#include <string>
#include <unordered_map>

// ── Platform detection ────────────────────────────────────────────────────────
#if defined(_WIN32) || defined(_WIN64)
    #define VEEX_PLATFORM_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
    #define VEEX_PLATFORM_MACOS 1
#elif defined(__linux__)
    #define VEEX_PLATFORM_LINUX 1
#else
    #error "Unsupported platform"
#endif

// ── OpenGL Platform Optimizations ─────────────────────────────────────────────
// Platform-specific GL hints and optimizations for macOS 11+ and Windows 10+

// macOS-specific optimizations
#if VEEX_PLATFORM_MACOS
    // macOS benefits from persistent mapped buffers
    #define VEEX_GL_USE_PERSISTENT_MAP 1
    
    // macOS has better performance with buffer storage
    #define VEEX_GL_USE_BUFFER_STORAGE 1
    
    // macOS GL is generally well-optimized, but we need to be careful about
    // texture compression formats (use ASTC when available)
    #define VEEX_GL_PREFER_ASTC 1
#endif

// Windows-specific optimizations
#if VEEX_PLATFORM_WINDOWS
    // Windows has excellent NVIDIA/AMD driver support
    // Enable NVIDIA-specific optimizations when available
    #define VEEX_GL_ENABLE_NV_EXTENSIONS 1
    
    // Windows benefits from explicit sync
    #define VEEX_GL_USE_EXPLICIT_SYNC 1
#endif

// Linux-specific optimizations
#if VEEX_PLATFORM_LINUX
    // Linux Mesa drivers benefit from certain optimizations
    #define VEEX_GL_USE_MESA_EXTENSIONS 1
#endif

namespace veex {

class Config {
public:
    Config() = default;
    bool LoadFromFile(const std::string& path);
    std::string Get(const std::string& key, const std::string& defaultValue = "") const;

    // The Linker is looking for this:
    static std::string GetExecutableDir();

private:
    std::unordered_map<std::string, std::string> m_entries;
};

} // namespace veex