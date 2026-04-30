#include "veex/FileSystem.h"
#include "veex/Logger.h"
#include "veex/Config.h"
#include <filesystem>
#include <algorithm>

namespace veex {

// ─── VPKManager Implementation ─────────────────────────────────────────────────

VPKManager& VPKManager::GetInstance() {
    static VPKManager instance;
    return instance;
}

bool VPKManager::MountVPK(const std::string& vpkPath) {
    // Check if already mounted
    if (IsMounted(vpkPath)) {
        Logger::Warn("VPKManager: VPK already mounted: " + vpkPath);
        return true;
    }

    // Create and load the VPK
    auto vpk = std::make_unique<VPK>(vpkPath);
    if (!vpk->Load()) {
        Logger::Error("VPKManager: Failed to load VPK: " + vpkPath);
        return false;
    }

    // Add to mounted list
    MountedVPK mounted;
    mounted.path = vpkPath;
    mounted.vpk = std::move(vpk);
    mountedVPKs.push_back(std::move(mounted));

    Logger::Info("VPKManager: Mounted VPK: " + vpkPath);
    return true;
}

bool VPKManager::UnmountVPK(const std::string& vpkPath) {
    auto it = std::find_if(mountedVPKs.begin(), mountedVPKs.end(),
        [&vpkPath](const MountedVPK& m) { return m.path == vpkPath; });

    if (it == mountedVPKs.end()) {
        Logger::Warn("VPKManager: VPK not mounted: " + vpkPath);
        return false;
    }

    Logger::Info("VPKManager: Unmounted VPK: " + vpkPath);
    mountedVPKs.erase(it);
    return true;
}

bool VPKManager::FileExists(const std::string& filePath) const {
    // Check each mounted VPK in reverse order (last mounted has priority)
    for (auto it = mountedVPKs.rbegin(); it != mountedVPKs.rend(); ++it) {
        if (it->vpk->FileExists(filePath)) {
            return true;
        }
    }
    return false;
}

bool VPKManager::ReadFile(const std::string& filePath, std::vector<char>& buffer) const {
    // Check each mounted VPK in reverse order (last mounted has priority)
    for (auto it = mountedVPKs.rbegin(); it != mountedVPKs.rend(); ++it) {
        if (it->vpk->FileExists(filePath)) {
            return it->vpk->ReadFile(filePath, buffer);
        }
    }
    return false;
}

size_t VPKManager::GetFileSize(const std::string& filePath) const {
    // Check each mounted VPK in reverse order (last mounted has priority)
    for (auto it = mountedVPKs.rbegin(); it != mountedVPKs.rend(); ++it) {
        if (it->vpk->FileExists(filePath)) {
            return it->vpk->GetFileSize(filePath);
        }
    }
    return 0;
}

bool VPKManager::IsMounted(const std::string& vpkPath) const {
    return std::find_if(mountedVPKs.begin(), mountedVPKs.end(),
        [&vpkPath](const MountedVPK& m) { return m.path == vpkPath; }) != mountedVPKs.end();
}

void VPKManager::Clear() {
    mountedVPKs.clear();
    Logger::Info("VPKManager: All VPKs unmounted");
}

// ─── FileSystem Functions ──────────────────────────────────────────────────────

bool InitializeFileSystem(const GameInfo& game) {
    VPKManager& vpkManager = VPKManager::GetInstance();
    
    // Mount all VPK files from the gameinfo
    for (const auto& vpkPath : game.vpkFiles) {
        vpkManager.MountVPK(vpkPath);
    }
    
    return true;
}

void ShutdownFileSystem() {
    VPKManager::GetInstance().Clear();
}

std::string ResolveAssetPath(const std::string& localPath, const GameInfo& game) {
    std::filesystem::path exeDir = Config::GetExecutableDir();
    
    // If the path is already absolute, check directly
    std::filesystem::path absolutePath(localPath);
    if (absolutePath.is_absolute()) {
        // Direct path check
        if (std::filesystem::exists(absolutePath)) {
            return localPath;
        } else {
            return "";
        }
    }

    // Clean leading slashes to ensure the path is treated as relative to the mount points
    std::string cleanPath = localPath;
    if (!cleanPath.empty() && (cleanPath[0] == '/' || cleanPath[0] == '\\')) {
        cleanPath.erase(0, 1);
    }
    std::filesystem::path target(cleanPath);

    // First, check VPK files (in reverse order - last mounted has priority)
    VPKManager& vpkManager = VPKManager::GetInstance();
    if (vpkManager.FileExists(cleanPath)) {
        // Return a special VPK path marker
        return "vpk://" + cleanPath;
    }

    // Then check regular filesystem paths
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
    if (std::filesystem::exists(fallback)) {
        return fallback.string();
    }

    return "";
}

bool ReadFile(const std::string& localPath, const GameInfo& game, std::vector<char>& buffer) {
    // Clean leading slashes
    std::string cleanPath = localPath;
    if (!cleanPath.empty() && (cleanPath[0] == '/' || cleanPath[0] == '\\')) {
        cleanPath.erase(0, 1);
    }

    // First, check VPK files
    VPKManager& vpkManager = VPKManager::GetInstance();
    if (vpkManager.ReadFile(cleanPath, buffer)) {
        return true;
    }

    // Then check regular filesystem paths
    std::string resolvedPath = ResolveAssetPath(localPath, game);
    if (!resolvedPath.empty() && resolvedPath.find("vpk://") != 0) {
        std::ifstream file(resolvedPath, std::ios::binary);
        if (file.is_open()) {
            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);
            buffer.resize(size);
            file.read(buffer.data(), size);
            return true;
        }
    }

    return false;
}

bool FileExists(const std::string& localPath, const GameInfo& game) {
    // Clean leading slashes
    std::string cleanPath = localPath;
    if (!cleanPath.empty() && (cleanPath[0] == '/' || cleanPath[0] == '\\')) {
        cleanPath.erase(0, 1);
    }

    // First, check VPK files
    VPKManager& vpkManager = VPKManager::GetInstance();
    if (vpkManager.FileExists(cleanPath)) {
        return true;
    }

    // Then check regular filesystem paths
    std::string resolvedPath = ResolveAssetPath(localPath, game);
    if (!resolvedPath.empty() && resolvedPath.find("vpk://") != 0) {
        return std::filesystem::exists(resolvedPath);
    }

    return false;
}

} // namespace veex