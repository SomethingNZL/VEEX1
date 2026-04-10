#include "veex/GameInfo.h"
#include "veex/Config.h"
#include "veex/Logger.h"
#include "veex/KeyValues.h"
#include <algorithm>
#include <filesystem>
#include <sstream>

namespace veex {

bool GameInfo::LoadFromFile(const std::string& path) {
    std::filesystem::path fsPath = std::filesystem::absolute(path);
    std::string root = fsPath.parent_path().string();
    
    // Normalize root path with the OS preferred separator (\ for Win, / for Mac)
    if (!root.empty() && root.back() != std::filesystem::path::preferred_separator) {
        root += std::filesystem::path::preferred_separator;
    }
    enginePath = root;

    // --- Initialize defaults ---
    game = "veex_game";
    gameName = "VEEX Project";
    developer = "Bradley";
    version = "1.0";
    type = "singleplayer_only";
    secure = false;
    nodefaultgamedir = false;
    nohunters = false;
    nomodels = false;
    searchPaths.clear();
    pakFiles.clear();
    vpkFiles.clear();

    auto kv = KeyValues::LoadFromFile(path);
    if (!kv) {
        Logger::Error("GameInfo: Could not parse '" + path + "', using default search path.");
        // Add default search path
        AddSearchPath("Game" + std::string(1, std::filesystem::path::preferred_separator), false);
        valid = false;
        return false;
    }

    // Parse the root node - in Source Engine format, the root contains the game info
    KVNode* rootNode = kv.get();
    
    // Extract game information from root level
    game = rootNode->Get("game", game);
    gameName = rootNode->Get("game", gameName); // Use "game" field for gameName if title not present
    title = rootNode->Get("title", gameName);
    title2 = rootNode->Get("title2", "");
    developer = rootNode->Get("developer", developer);
    developerURL = rootNode->Get("developer_url", "");
    manual = rootNode->Get("manual", "");
    icon = rootNode->Get("icon", "");
    gameLogo = rootNode->Get("gamelogo", "");
    gameLogoMinimal = rootNode->Get("gamelogo_minimal", "");
    type = rootNode->Get("type", type);
    version = rootNode->Get("version", version);
    gameURL = rootNode->Get("game_url", "");
    supportURL = rootNode->Get("support_url", "");
    updateURL = rootNode->Get("update_url", "");
    
    // Steam integration
    steamAppId = rootNode->Get("SteamAppId", "");
    toolsAppId = rootNode->Get("ToolsAppId", "");
    gameAppId = rootNode->Get("GameAppId", "");
    
    // Configuration flags
    secure = (rootNode->Get("secure", "0") == "1");
    nodefaultgamedir = (rootNode->Get("nodefaultgamedir", "0") == "1");
    nohunters = (rootNode->Get("nohunters", "0") == "1");
    nomodels = (rootNode->Get("nomodels", "0") == "1");
    
    // Game directory and start map
    gameDir = rootNode->Get("gameDir", "Game");
    startMap = rootNode->Get("startmap", "");

    // Parse FileSystem block
    KVNode* fs = rootNode->GetChild("FileSystem");
    if (fs) {
        KVNode* sp = fs->GetChild("SearchPaths");
        if (sp) {
            // Clear existing search paths
            searchPaths.clear();
            
            // Process each search path entry
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
                    
                    // Check if this is a VPK file
                    bool isVPK = IsVPKPath(normalized);
                    bool isDirectory = !isVPK;
                    
                    // Ensure proper trailing separator for directories
                    if (isDirectory && normalized.back() != std::filesystem::path::preferred_separator) 
                        normalized += std::filesystem::path::preferred_separator;
                    
                    AddSearchPath(normalized, isVPK);
                    Logger::Info("GameInfo: SearchPath -> " + normalized + (isVPK ? " (VPK)" : " (Directory)"));
                }
            }
        }
    }

    // If no search paths were found, add default
    if (searchPaths.empty()) {
        AddSearchPath(root, false);
    }

    // Build VPK files list for backward compatibility
    vpkFiles.clear();
    for (const auto& sp : searchPaths) {
        if (sp.isVPK) {
            vpkFiles.push_back(sp.path);
        }
    }
    
    // Build legacy pakFiles list for backward compatibility
    pakFiles.clear();
    for (const auto& sp : searchPaths) {
        if (sp.isDirectory) {
            pakFiles.push_back(sp.path);
        }
    }

    valid = true;
    Logger::Info("GameInfo: Loaded '" + gameName + "' from " + path);
    return true;
}

bool GameInfo::LoadFromString(const std::string& content, const std::string& basePath) {
    // Set engine path from base path
    std::filesystem::path fsPath = std::filesystem::absolute(basePath);
    std::string enginePathStr = fsPath.parent_path().string();
    
    if (!enginePathStr.empty() && enginePathStr.back() != std::filesystem::path::preferred_separator) {
        enginePathStr += std::filesystem::path::preferred_separator;
    }
    enginePath = enginePathStr;

    // Initialize defaults
    game = "veex_game";
    gameName = "VEEX Project";
    developer = "Bradley";
    version = "1.0";
    type = "singleplayer_only";
    secure = false;
    nodefaultgamedir = false;
    searchPaths.clear();
    pakFiles.clear();
    vpkFiles.clear();

    auto kv = KeyValues::LoadFromString(content);
    if (!kv) {
        Logger::Error("GameInfo: Could not parse gameinfo content, using defaults.");
        AddSearchPath("Game" + std::string(1, std::filesystem::path::preferred_separator), false);
        valid = false;
        return false;
    }

    // Parse the root node
    KVNode* rootNode = kv.get();
    
    // Extract game information
    game = rootNode->Get("game", game);
    gameName = rootNode->Get("game", gameName);
    title = rootNode->Get("title", gameName);
    title2 = rootNode->Get("title2", "");
    developer = rootNode->Get("developer", developer);
    type = rootNode->Get("type", type);
    version = rootNode->Get("version", version);
    
    steamAppId = rootNode->Get("SteamAppId", "");
    toolsAppId = rootNode->Get("ToolsAppId", "");
    gameAppId = rootNode->Get("GameAppId", "");
    
    secure = (rootNode->Get("secure", "0") == "1");
    nodefaultgamedir = (rootNode->Get("nodefaultgamedir", "0") == "1");
    
    gameDir = rootNode->Get("gameDir", "Game");
    startMap = rootNode->Get("startmap", "");

    // Parse FileSystem block
    KVNode* fs = rootNode->GetChild("FileSystem");
    if (fs) {
        KVNode* sp = fs->GetChild("SearchPaths");
        if (sp) {
            searchPaths.clear();
            
            for (const auto& child : sp->children) {
                std::string mountPath = child->value;
                const std::string macro = "|gameinfo_path|";
                auto pos = mountPath.find(macro);
                if (pos != std::string::npos)
                    mountPath.replace(pos, macro.size(), enginePathStr);

                if (!mountPath.empty()) {
                    std::filesystem::path p(mountPath);
                    std::string normalized = p.make_preferred().string();
                    
                    bool isVPK = IsVPKPath(normalized);
                    bool isDirectory = !isVPK;
                    
                    if (isDirectory && normalized.back() != std::filesystem::path::preferred_separator) 
                        normalized += std::filesystem::path::preferred_separator;
                    
                    AddSearchPath(normalized, isVPK);
                    Logger::Info("GameInfo: SearchPath -> " + normalized + (isVPK ? " (VPK)" : " (Directory)"));
                }
            }
        }
    }

    if (searchPaths.empty()) {
        AddSearchPath(enginePathStr, false);
    }

    // Build VPK and pak files lists
    vpkFiles.clear();
    pakFiles.clear();
    for (const auto& sp : searchPaths) {
        if (sp.isVPK) {
            vpkFiles.push_back(sp.path);
        } else {
            pakFiles.push_back(sp.path);
        }
    }

    valid = true;
    Logger::Info("GameInfo: Loaded '" + gameName + "' from string");
    return true;
}

void GameInfo::AddSearchPath(const std::string& path, bool isVPK) {
    SearchPath sp;
    sp.originalPath = path;
    sp.path = ResolveMacro(path);
    sp.isVPK = isVPK;
    sp.isDirectory = !isVPK;
    
    searchPaths.push_back(sp);
}

std::string GameInfo::ResolveMacro(const std::string& path) const {
    std::string result = path;
    
    // Replace |gameinfo_path| macro
    const std::string gameinfoMacro = "|gameinfo_path|";
    auto pos = result.find(gameinfoMacro);
    if (pos != std::string::npos) {
        result.replace(pos, gameinfoMacro.size(), enginePath);
    }
    
    // Replace |game| macro
    const std::string gameMacro = "|game|";
    pos = result.find(gameMacro);
    if (pos != std::string::npos) {
        result.replace(pos, gameMacro.size(), game);
    }
    
    // Replace |platform| macro (simplified - would need platform detection)
    const std::string platformMacro = "|platform|";
    pos = result.find(platformMacro);
    if (pos != std::string::npos) {
        result.replace(pos, platformMacro.size(), "win32"); // Default to win32
    }
    
    return result;
}

bool GameInfo::IsVPKPath(const std::string& path) {
    std::string lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
    
    // Check for .vpk extension
    if (lowerPath.length() >= 4) {
        return lowerPath.substr(lowerPath.length() - 4) == ".vpk";
    }
    return false;
}

bool GameInfoBin::LoadFromFile(const std::string& path) {
    engineVersion = "0.1.0";
    gamePath = "./Game/";
    cachePath = Config::GetExecutableDir() + "cache/";

    auto kv = KeyValues::LoadFromFile(path);
    if (!kv) {
        Logger::Warn("GameInfoBin: Could not parse '" + path + "', using defaults.");
        valid = false;
        return false;
    }

    KVNode* root = kv->GetChild("GameInfoBin");
    if (root) {
        engineVersion = root->Get("engineVersion", engineVersion);
        gamePath = root->Get("gamePath", gamePath);
        cachePath = root->Get("cachePath", cachePath);
    }

    valid = true;
    return true;
}

} // namespace veex