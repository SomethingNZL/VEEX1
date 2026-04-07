#pragma once
// BinfoParser.h
// Reads Game/entities.binfo and builds a BinfoTable.
// Mirrors Source's MapEntity_ParseAllEntities but for the binfo format.

#include "veex/BinfoTypes.h"
#include "veex/GameInfo.h"
#include <string>

namespace veex {

class BinfoParser {
public:
    // Load and parse a .binfo file.
    // Returns true on success, false if the file can't be opened.
    // Update: Added GameInfo parameter to resolve asset paths.
    static bool LoadFromFile(const std::string& path, const GameInfo& game, BinfoTable& outTable);

private:
    // Strip leading/trailing whitespace.
    static std::string Trim(const std::string& s);

    // Parse "InputName(param)" → writes inputName and param.
    // If no parens are present, param is left empty.
    static void ParseInput(const std::string& token,
                           std::string& inputName,
                           std::string& param);
};

} // namespace veex