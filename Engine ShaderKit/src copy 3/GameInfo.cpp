#include "veex/GameInfo.h"
#include "veex/Config.h"
#include "veex/Logger.h"
#include "veex/KeyValues.h"
#include <algorithm>
#include <filesystem>

namespace veex {

bool GameInfo::LoadFromFile(const std::string& path) {
    std::filesystem::path fsPath = std::filesystem::absolute(path);
    std::string root = fsPath.parent_path().string();
    
    // Normalize root path with the OS preferred separator (\ for Win, / for Mac)
    if (!root.empty() && root.back() != std::filesystem::path::preferred_separator) {
        root += std::filesystem::path::preferred_separator;
    }
    enginePath = root;

    // --- Defaults ---
    gameName  = "VEEX Project";
    developer = "Bradley";
    version   = "1.0";
    pakFiles.clear();

    auto kv = KeyValues::LoadFromFile(path);
    if (!kv) {
        Logger::Error("GameInfo: Could not parse '" + path + "', using default search path.");
        pakFiles.push_back("Game" + std::string(1, std::filesystem::path::preferred_separator)); 
        valid = false;
        return false;
    }

    KVNode* gameInfoNode = kv->GetChild("GameInfo");
    if (!gameInfoNode) {
        Logger::Warn("GameInfo: No 'GameInfo' block found, using defaults.");
        valid = false;
        return false;
    }

    gameName  = gameInfoNode->Get("game",      gameName);
    developer = gameInfoNode->Get("developer", developer);
    version   = gameInfoNode->Get("version",   version);

    KVNode* fs = gameInfoNode->GetChild("FileSystem");
    if (fs) {
        KVNode* sp = fs->GetChild("SearchPaths");
        if (sp) {
            pakFiles.clear();
            for (const auto& child : sp->children) {
                std::string mountPath = child->value;
                const std::string macro = "|gameinfo_path|";
                auto pos = mountPath.find(macro);
                if (pos != std::string::npos)
                    mountPath.replace(pos, macro.size(), root);

                if (!mountPath.empty()) {
                    // Convert slashes to the OS preferred format automatically
                    std::filesystem::path p(mountPath);
                    std::string normalized = p.make_preferred().string();
                    
                    if (normalized.back() != std::filesystem::path::preferred_separator) 
                        normalized += std::filesystem::path::preferred_separator;
                    
                    pakFiles.push_back(normalized);
                    Logger::Info("GameInfo: SearchPath -> " + normalized);
                }
            }
        }
    }

    if (pakFiles.empty()) pakFiles.push_back(root);

    valid = true;
    Logger::Info("GameInfo: Loaded '" + gameName + "' from " + path);
    return true;
}

bool GameInfoBin::LoadFromFile(const std::string& path) {
    engineVersion = "0.1.0";
    gamePath      = "./Game/";
    cachePath     = Config::GetExecutableDir() + "cache/";

    auto kv = KeyValues::LoadFromFile(path);
    if (!kv) {
        Logger::Warn("GameInfoBin: Could not parse '" + path + "', using defaults.");
        valid = false;
        return false;
    }

    KVNode* root = kv->GetChild("GameInfoBin");
    if (root) {
        engineVersion = root->Get("engineVersion", engineVersion);
        gamePath      = root->Get("gamePath",      gamePath);
        cachePath     = root->Get("cachePath",     cachePath);
    }

    valid = true;
    return true;
}

} // namespace veex