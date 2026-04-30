#include "veex/Config.h"
#include "veex/Logger.h"
#include <fstream>
#include <sstream>

#ifdef _WIN32
    #include <windows.h>
#elif __APPLE__
    #include <mach-o/dyld.h>
    #include <libgen.h>
    #include <climits>
#elif __linux__
    #include <unistd.h>
    #include <limits.h>
#endif

namespace veex {

bool Config::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::Warn("Config: Could not open file: " + path);
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Strip inline comments
        auto commentPos = line.find("//");
        if (commentPos != std::string::npos)
            line = line.substr(0, commentPos);

        std::istringstream ss(line);
        std::string key, value;
        if (ss >> key >> value)
            m_entries[key] = value;
    }

    Logger::Info("Config: Loaded " + std::to_string(m_entries.size()) + " entries from " + path);
    return true;
}

std::string Config::Get(const std::string& key, const std::string& defaultValue) const {
    auto it = m_entries.find(key);
    if (it == m_entries.end()) return defaultValue;
    return it->second;
}

std::string Config::GetExecutableDir() {
    char path[1024] = {};

#ifdef _WIN32
    DWORD size = GetModuleFileNameA(NULL, path, sizeof(path));
    if (size == 0) return "./";
    std::string s(path);
    return s.substr(0, s.find_last_of("\\/")) + "/";

#elif __APPLE__
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        char real_path[PATH_MAX];
        if (realpath(path, real_path)) {
            char* dir = dirname(real_path);
            return std::string(dir) + "/";
        }
    }

#elif __linux__
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        std::string s(path);
        return s.substr(0, s.find_last_of('/')) + "/";
    }
#endif

    return "./";
}

} // namespace veex
