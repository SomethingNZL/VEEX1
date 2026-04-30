#pragma once

#include "veex/KeyValues.h"
#include <string>
#include <vector>
#include <memory>

namespace veex {

/**
 * @brief Represents a search path entry in the gameinfo.txt FileSystem block.
 * 
 * Search paths can be:
 * - Directory paths on the filesystem
 * - VPK archive files
 * - Paths with macros like |gameinfo_path|
 */
struct SearchPath {
    std::string path;           ///< The actual path (after macro expansion)
    std::string originalPath;   ///< Original path as specified in gameinfo.txt
    bool isVPK;                 ///< True if this is a .vpk file
    bool isDirectory;           ///< True if this is a directory path
    
    SearchPath() : isVPK(false), isDirectory(false) {}
    SearchPath(const std::string& p, bool vpk, bool dir) 
        : path(p), originalPath(p), isVPK(vpk), isDirectory(dir) {}
};

/**
 * @brief Main GameInfo structure for parsing Source Engine style gameinfo.txt files.
 * 
 * This structure supports the full Source Engine gameinfo.txt format including:
 * - Game information (name, title, developer, etc.)
 * - FileSystem block with SearchPaths
 * - Steam integration (SteamAppId, ToolsAppId)
 * - VPK file mounting
 * - Platform-specific paths
 */
struct GameInfo {
    // Game Information
    std::string game;                   ///< Game identifier (e.g., "hl2")
    std::string gameName;               ///< Full game name (e.g., "Half-Life 2")
    std::string title;                  ///< Window title (primary)
    std::string title2;                 ///< Window title (secondary line)
    std::string developer;              ///< Developer name
    std::string developerURL;           ///< Developer website URL
    std::string manual;                 ///< URL or path to manual
    std::string icon;                   ///< Path to game icon
    std::string gameLogo;               ///< Path to game logo
    std::string gameLogoMinimal;        ///< Path to minimal game logo
    std::string type;                   ///< Game type (e.g., "singleplayer_only")
    std::string version;                ///< Game version
    std::string gameURL;                ///< Game website URL
    std::string supportURL;             ///< Support website URL
    std::string updateURL;              ///< Update website URL
    
    // Steam Integration
    std::string steamAppId;             ///< Steam App ID
    std::string toolsAppId;             ///< Tools App ID
    std::string gameAppId;              ///< Game App ID
    
    // File System
    std::vector<SearchPath> searchPaths; ///< List of search paths (directories and VPKs)
    std::vector<std::string> pakFiles;   ///< Legacy: kept for backward compatibility
    std::string enginePath;              ///< Path to the gameinfo.txt directory
    std::string gameDir;                 ///< Game directory (usually "Game" or game name)
    std::string startMap;                ///< Starting map name
    
    // Configuration
    bool secure;                        ///< Whether the game is secure (VAC)
    bool nodefaultgamedir;              ///< Don't use default game directory
    bool nohunters;                     ///< Disable hunters
    bool nomodels;                      ///< Disable models
    bool nohimodels;                    ///< Disable hi-res models
    
    // VPK Support
    std::vector<std::string> vpkFiles;  ///< List of VPK files to mount
    
    bool valid = false;
    
    /**
     * @brief Load gameinfo from a file path.
     * @param path Path to the gameinfo.txt file.
     * @return True if loading was successful.
     */
    bool LoadFromFile(const std::string& path);
    
    /**
     * @brief Load gameinfo from a KeyValues string.
     * @param content The KeyValues content as a string.
     * @param basePath Base path for resolving relative paths.
     * @return True if loading was successful.
     */
    bool LoadFromString(const std::string& content, const std::string& basePath);
    
    /**
     * @brief Add a search path to the list.
     * @param path The path to add.
     * @param isVPK Whether the path is a VPK file.
     */
    void AddSearchPath(const std::string& path, bool isVPK = false);
    
    /**
     * @brief Resolve a macro in a path string.
     * @param path The path containing macros.
     * @return The resolved path.
     */
    std::string ResolveMacro(const std::string& path) const;
    
    /**
     * @brief Check if a path is a VPK file.
     * @param path The path to check.
     * @return True if the path ends with .vpk.
     */
    static bool IsVPKPath(const std::string& path);
};

/**
 * @brief Binary gameinfo structure (gameinfobin.txt).
 * 
 * This is used for engine configuration separate from game configuration.
 */
struct GameInfoBin {
    std::string engineVersion;
    std::string gamePath;
    std::string cachePath;
    
    bool LoadFromFile(const std::string& path);
    
    bool valid = false;
};

} // namespace veex