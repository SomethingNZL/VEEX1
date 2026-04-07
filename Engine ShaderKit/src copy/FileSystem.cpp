#include "veex/FileSystem.h"
#include "veex/Logger.h"
#include "veex/Config.h" // Assuming this has GetExecutableDir()
#include <filesystem>

namespace veex {

std::string ResolveAssetPath(const std::string& localPath, const GameInfo& game) {
    // 1. Get the directory where VEEXEngine is actually running
    std::string exeDir = Config::GetExecutableDir();
    if (!exeDir.empty() && exeDir.back() != '/') exeDir += "/";

    // 2. Iterate through the SearchPaths from your gameinfo.txt
    // game.pakFiles should contain {"Game/", "platform/"}
    for (const auto& mountPoint : game.pakFiles) {
        std::string searchFolder = mountPoint;
        
        // Ensure no double slashes when joining
        if (!searchFolder.empty() && searchFolder.back() != '/') searchFolder += "/";
        
        // Construct the full absolute path
        std::string fullPath = exeDir + searchFolder + localPath;

        if (std::filesystem::exists(fullPath)) {
            Logger::Info("FS: Match! Found asset at: " + fullPath);
            return fullPath;
        } else {
            // Log the "Miss" so we can see exactly where the string is wrong
            Logger::Info("FS: Checked and missed: " + fullPath);
        }
    }

    // 3. Fallback: check the path directly relative to the EXE
    std::string fallback = exeDir + localPath;
    if (std::filesystem::exists(fallback)) return fallback;

    Logger::Error("FS: Failed to locate asset: " + localPath);
    return "";
}

} // namespace veex