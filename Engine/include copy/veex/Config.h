#pragma once
#include <string>
#include <unordered_map>

namespace veex {

class Config {
public:
    Config() = default;
    bool LoadFromFile(const std::string& path);
    std::string Get(const std::string& key, const std::string& defaultValue = "") const;

    // The Linker is looking for this:
    static std::string GetExecutableDir();

private:
    std::unordered_map<std::string, std::string> m_entries;
};

} // namespace veex