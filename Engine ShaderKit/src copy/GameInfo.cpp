#include "veex/GameInfo.h"
#include "veex/Config.h"
#include "veex/Logger.h"
#include "veex/KeyValues.h"
#include <algorithm>

namespace veex {

bool GameInfo::LoadFromFile(const std::string& path) {
    // Always derive enginePath from the file's directory
    std::string root = path.substr(0, path.find_last_of("\\/"));
    if (!root.empty()) root += "/";
    enginePath = root;

    // --- Defaults (used if parsing fails) ---
    gameName  = "VEEX Project";
    developer = "Unknown";
    version   = "1.0";
    pakFiles.clear();
    pakFiles.push_back(root); // "|gameinfo_path|." equivalent

    auto kv = KeyValues::LoadFromFile(path);
    if (!kv) {
        Logger::Warn("GameInfo: Could not parse '" + path + "', using defaults.");
        valid = false;
        return false;
    }

    // gameinfo.txt structure:
    //   "GameInfo"
    //   {
    //       "game"      "My Game"
    //       "developer" "Bradley"
    //       "version"   "1.0"
    //       "FileSystem"
    //       {
    //           "SearchPaths"
    //           {
    //               "Game"  "Game/"
    //               "Game"  "platform/"
    //           }
    //       }
    //   }
    KVNode* gameInfoNode = kv->GetChild("GameInfo");
    if (!gameInfoNode) {
        Logger::Warn("GameInfo: No 'GameInfo' block found, using defaults.");
        valid = false;
        return false;
    }

    gameName  = gameInfoNode->Get("game",      gameName);
    developer = gameInfoNode->Get("developer", developer);
    version   = gameInfoNode->Get("version",   version);

    // Parse search paths
    KVNode* fs = gameInfoNode->GetChild("FileSystem");
    if (fs) {
        KVNode* sp = fs->GetChild("SearchPaths");
        if (sp) {
            pakFiles.clear();
            for (const auto& child : sp->children) {
                std::string mountPath = child->value;
                // Resolve "|gameinfo_path|" macro
                const std::string macro = "|gameinfo_path|";
                auto pos = mountPath.find(macro);
                if (pos != std::string::npos)
                    mountPath.replace(pos, macro.size(), root);

                if (!mountPath.empty()) {
                    if (mountPath.back() != '/') mountPath += "/";
                    pakFiles.push_back(mountPath);
                    Logger::Info("GameInfo: SearchPath -> " + mountPath);
                }
            }
        }
    }

    // Always ensure the game root itself is in the search list
    if (pakFiles.empty()) pakFiles.push_back(root);

    valid = true;
    Logger::Info("GameInfo: Loaded '" + gameName + "' from " + path);
    return true;
}

bool GameInfoBin::LoadFromFile(const std::string& path) {
    // Defaults
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
    Logger::Info("GameInfoBin: Engine " + engineVersion + ", game at " + gamePath);
    return true;
}

} // namespace veex
