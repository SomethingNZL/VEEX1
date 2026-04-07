#pragma once

#include <iostream>
#include <string>

namespace veex {

class Logger {
public:
    static void Info(const std::string& msg) {
        std::cout << "[INFO] " << msg << "\n";
    }
    static void Warn(const std::string& msg) {
        std::cout << "[WARN] " << msg << "\n";
    }
    static void Error(const std::string& msg) {
        std::cerr << "[ERROR] " << msg << "\n";
    }
};

} // namespace veex
