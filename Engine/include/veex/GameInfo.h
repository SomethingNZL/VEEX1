#pragma once

#include "veex/KeyValues.h"
#include <string>
#include <vector>

namespace veex {

struct GameInfo {
    std::string gameName;
    std::string developer;
    std::string version;
    std::string enginePath;
    std::vector<std::string> pakFiles;

    // Configuration from gameinfo.txt
    std::string mapDir;
    std::string startMap;

    bool LoadFromFile(const std::string& path);

    bool valid = false;
};

struct GameInfoBin {
    std::string engineVersion;
    std::string gamePath;
    std::string cachePath;

    bool LoadFromFile(const std::string& path);

    bool valid = false;
};

} // namespace veex