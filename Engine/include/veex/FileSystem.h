#pragma once
#include <string>
#include "veex/GameInfo.h"

namespace veex {
    /**
     * Resolves a local asset path (e.g., "textures/wall.png") into an absolute 
     * disk path by checking the search paths defined in gameinfo.txt.
     */
    std::string ResolveAssetPath(const std::string& localPath, const GameInfo& game);
}