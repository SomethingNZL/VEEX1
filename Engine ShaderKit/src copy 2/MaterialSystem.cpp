#include "veex/MaterialSystem.h"
#include "veex/FileSystem.h"
#include "veex/GameInfo.h"
#include "veex/Logger.h"
#include "../third_party/stb/stb_image.h"

#include <glad/gl.h>
#include <algorithm> 
#include <cctype>    

namespace veex {

// ─── Helpers ─────────────────────────────────────────────────────────────────

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

/**
 * Strips Source Engine map-baked suffixes.
 * Handles single suffixes (_patch) and coordinate suffixes (_-696_236_-299).
 */
static std::string StripSourceSuffix(const std::string& name) {
    std::string result = name;
    
    // Iteratively strip suffixes that look like coordinates or IDs
    while (true) {
        size_t lastUnderscore = result.find_last_of('_');
        if (lastUnderscore == std::string::npos || lastUnderscore == 0) break;

        std::string suffix = result.substr(lastUnderscore + 1);
        
        // If the suffix is numeric, a negative number, or "patch", it's a baked suffix
        bool isCoord = !suffix.empty() && (std::isdigit(suffix[0]) || suffix[0] == '-');
        bool isPatch = (suffix == "patch");

        if (isCoord || isPatch) {
            result = result.substr(0, lastUnderscore);
        } else {
            break; 
        }
    }
    return result;
}

// ─── Initialize / Shutdown ───────────────────────────────────────────────────

void MaterialSystem::Initialize(const GameInfo& game) {
    m_gameInfo = &game;
    m_fallbackTexture = CreateDummyTexture();
    Logger::Info("MaterialSystem: Initialized. Fallback texture created.");
}

void MaterialSystem::Shutdown() {
    for (auto& pair : m_textureCache) {
        uint32_t id = pair.second;
        if (id != 0 && id != m_fallbackTexture) {
            glDeleteTextures(1, &id);
        }
    }
    m_textureCache.clear();

    if (m_fallbackTexture != 0) {
        glDeleteTextures(1, &m_fallbackTexture);
        m_fallbackTexture = 0;
    }

    m_gameInfo = nullptr;
    Logger::Info("MaterialSystem: Shutdown complete.");
}

// ─── GetMaterial ─────────────────────────────────────────────────────────────

Material MaterialSystem::GetMaterial(const std::string& name) {
    // Standardize path separators and casing
    std::string key = ToLower(name);
    std::replace(key.begin(), key.end(), '\\', '/');

    // Check Cache
    auto it = m_textureCache.find(key);
    if (it != m_textureCache.end()) {
        return { it->second, m_defaultShader };
    }

    // Skip tool textures/nodraw
    if (key.rfind("tools/", 0) == 0 || key == "trigger" || key == "nodraw" || key == "sky") {
        m_textureCache[key] = m_fallbackTexture;
        return { m_fallbackTexture, m_defaultShader };
    }

    uint32_t texID = 0;
    const char* extensions[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", nullptr };

    if (m_gameInfo) {
        // 1. Try Exact Match
        for (int i = 0; extensions[i] && texID == 0; ++i) {
            std::string localPath = "materials/" + key + extensions[i];
            std::string fullPath = ResolveAssetPath(localPath, *m_gameInfo);
            if (!fullPath.empty()) texID = LoadTexture(fullPath);
        }

        // 2. Try Stripped Match (maps/cs_office/stone/wall_-128_0_64 -> stone/wall)
        if (texID == 0) {
            std::string stripped = StripSourceSuffix(key);
            
            // If the texture is in a map subfolder, also try the base directory
            // e.g. maps/cs_office/metal/door -> metal/door
            if (stripped.rfind("maps/", 0) == 0) {
                size_t secondSlash = stripped.find('/', 5); // find slash after maps/mapname/
                if (secondSlash != std::string::npos) {
                    std::string baseDirMatch = stripped.substr(secondSlash + 1);
                    for (int i = 0; extensions[i] && texID == 0; ++i) {
                        std::string localPath = "materials/" + baseDirMatch + extensions[i];
                        std::string fullPath = ResolveAssetPath(localPath, *m_gameInfo);
                        if (!fullPath.empty()) texID = LoadTexture(fullPath);
                    }
                }
            }

            // Fallback to searching the stripped name in its current path
            if (texID == 0 && stripped != key) {
                for (int i = 0; extensions[i] && texID == 0; ++i) {
                    std::string localPath = "materials/" + stripped + extensions[i];
                    std::string fullPath = ResolveAssetPath(localPath, *m_gameInfo);
                    if (!fullPath.empty()) texID = LoadTexture(fullPath);
                }
            }
        }
    }

    if (texID == 0) {
        Logger::Warn("MaterialSystem: No texture found for '" + name + "', using fallback.");
        texID = m_fallbackTexture;
    }

    m_textureCache[key] = texID;
    return { texID, m_defaultShader };
}

// ─── Private helpers ─────────────────────────────────────────────────────────

uint32_t MaterialSystem::CreateDummyTexture() {
    uint8_t checker[] = { 
        255, 0, 255, 255,   0, 0, 0, 255,
        0, 0, 0, 255,       255, 0, 255, 255 
    };

    uint32_t tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, checker);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

uint32_t MaterialSystem::LoadTexture(const std::string& path) {
    int w, h, ch;
    // Force 4 channels for consistency (RGBA)
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) return 0;

    uint32_t tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    Logger::Info("MaterialSystem: Loaded '" + path + "' (" + std::to_string(w) + "x" + std::to_string(h) + ")");
    return tex;
}

} // namespace veex