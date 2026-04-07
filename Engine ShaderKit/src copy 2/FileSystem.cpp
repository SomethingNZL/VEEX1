#include "veex/FileSystem.h"
#include "veex/Logger.h"
#include "veex/Config.h"
#include <filesystem>

namespace veex {

std::string ResolveAssetPath(const std::string& localPath, const GameInfo& game) {
    std::filesystem::path exeDir = Config::GetExecutableDir();
    
    // Clean leading slashes to ensure the path is treated as relative to the mount points
    std::string cleanPath = localPath;
    if (!cleanPath.empty() && (cleanPath[0] == '/' || cleanPath[0] == '\\')) {
        cleanPath.erase(0, 1);
    }
    std::filesystem::path target(cleanPath);

    for (const auto& mountPoint : game.pakFiles) {
        std::filesystem::path base(mountPoint);
        std::filesystem::path fullPath;
        
        // join paths using the / operator which handles OS slashes correctly
        if (base.is_absolute()) {
            fullPath = base / target;
        } else {
            fullPath = exeDir / base / target;
        }

        if (std::filesystem::exists(fullPath)) {
            return fullPath.string();
        }
    }

    // Fallback directly to EXE folder
    std::filesystem::path fallback = exeDir / target;
    if (std::filesystem::exists(fallback)) return fallback.string();

    Logger::Error("FS: Failed to locate asset: " + localPath);
    return "";
}

} // namespace veex