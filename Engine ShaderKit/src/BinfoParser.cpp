#include "veex/BinfoParser.h"
#include "veex/Logger.h"
#include "veex/FileSystem.h"
#include <fstream>
#include <sstream>

namespace veex {

std::string BinfoParser::Trim(const std::string& s)
{
    const char* ws = " \t\r\n";
    size_t start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

void BinfoParser::ParseInput(const std::string& token, std::string& inputName, std::string& param)
{
    param.clear();
    size_t lp = token.find('(');
    if (lp == std::string::npos) {
        inputName = Trim(token);
        return;
    }

    inputName = Trim(token.substr(0, lp));
    size_t rp = token.rfind(')');
    if (rp == std::string::npos || rp <= lp) return;

    std::string inner = Trim(token.substr(lp + 1, rp - lp - 1));
    if (inner.size() >= 2 &&
        ((inner.front() == '"' && inner.back() == '"') ||
         (inner.front() == '\'' && inner.back() == '\'')))
    {
        inner = inner.substr(1, inner.size() - 2);
    }
    param = inner;
}

bool BinfoParser::LoadFromFile(const std::string& path, const GameInfo& game, BinfoTable& outTable)
{
    std::string physicalPath = ResolveAssetPath(path, game);

    if (physicalPath.empty()) {
        Logger::Warn("BinfoParser: FileSystem could not locate '" + path + "'.");
        return false;
    }

    std::ifstream file(physicalPath);
    if (!file.is_open()) {
        Logger::Warn("BinfoParser: Could not open '" + physicalPath + "' — no binfo wiring loaded.");
        return false;
    }

    outTable.clear();
    std::string currentClass;
    std::string line;
    int lineNum = 0;

    while (std::getline(file, line)) {
        ++lineNum;
        std::string t = Trim(line);
        if (t.empty() || t[0] == '#') continue;

        if (t == "RETURN") {
            currentClass.clear();
            continue;
        }

        if (t.back() == ':' && t.find('>') == std::string::npos) {
            currentClass = Trim(t.substr(0, t.size() - 1));
            outTable.emplace(currentClass, ConnectionList{});
            continue;
        }

        if (!currentClass.empty()) {
            std::vector<std::string> parts;
            std::istringstream ss(t);
            std::string seg;
            while (std::getline(ss, seg, '>')) parts.push_back(Trim(seg));

            if (parts.size() != 3) {
                Logger::Warn("BinfoParser: Malformed connection at line " + std::to_string(lineNum));
                continue;
            }

            EntityConnection conn;
            conn.outputName = parts[0];
            conn.targetName = parts[1];
            ParseInput(parts[2], conn.inputName, conn.param);
            outTable[currentClass].push_back(std::move(conn));
        }
    }

    Logger::Info("BinfoParser: Loaded " + std::to_string(outTable.size()) + " entity definitions.");
    return true;
}

} // namespace veex