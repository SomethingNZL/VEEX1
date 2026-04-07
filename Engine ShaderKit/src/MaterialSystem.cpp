#include "veex/MaterialSystem.h"
#include "veex/FileSystem.h"
#include "veex/GameInfo.h"
#include "veex/Logger.h"

#include "../third_party/stb/stb_image.h"

#include <glad/gl.h>
#include <algorithm>
#include <cctype>

namespace veex {

// Global GL upload lock — same as before
static std::mutex g_glContextMutex;

// ─── Helpers ─────────────────────────────────────────────────────────────────

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

static std::string StripSourceSuffix(const std::string& name) {
    std::string result = name;
    while (true) {
        size_t lastUnderscore = result.find_last_of('_');
        if (lastUnderscore == std::string::npos || lastUnderscore == 0) break;
        std::string suffix = result.substr(lastUnderscore + 1);
        bool isCoord = !suffix.empty() && (std::isdigit(suffix[0]) || suffix[0] == '-');
        if (isCoord || suffix == "patch") {
            result = result.substr(0, lastUnderscore);
        } else {
            break;
        }
    }
    return result;
}

// Try every supported extension for a given search key under materials/.
// Returns the resolved disk path, or "" if not found.
static const char* kExts[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", nullptr };

static std::string FindMaterialPath(const std::string& searchKey, const GameInfo& game) {
    for (int i = 0; kExts[i]; ++i) {
        std::string p = ResolveAssetPath("materials/" + searchKey + kExts[i], game);
        if (!p.empty()) return p;
    }
    return "";
}

// ─── Initialize / Shutdown ───────────────────────────────────────────────────

void MaterialSystem::Initialize(const GameInfo& game) {
    m_gameInfo        = &game;
    m_fallbackTexture = CreateDummyTexture();
    Logger::Info("MaterialSystem: Initialized.");
}

void MaterialSystem::Shutdown() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // Collect every unique GL handle so we delete each once.
    std::unordered_map<uint32_t, bool> seen;
    for (auto& [key, mat] : m_materialCache) {
        for (uint32_t id : { mat.textureID, mat.roughnessID,
                              mat.metallicID, mat.normalID }) {
            if (id != 0 && id != m_fallbackTexture && !seen[id]) {
                glDeleteTextures(1, &id);
                seen[id] = true;
            }
        }
    }
    m_materialCache.clear();

    if (m_fallbackTexture != 0) {
        glDeleteTextures(1, &m_fallbackTexture);
        m_fallbackTexture = 0;
    }
}

// ─── GetMaterial ─────────────────────────────────────────────────────────────

Material MaterialSystem::GetMaterial(const std::string& name) {
    std::string key = ToLower(name);
    std::replace(key.begin(), key.end(), '\\', '/');

    // 1. Cache hit
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_materialCache.find(key);
        if (it != m_materialCache.end()) return it->second;
    }

    // 2. Tools / skip filters
    if (key.rfind("tools/", 0) == 0 || key == "trigger" ||
        key == "nodraw"             || key == "sky") {
        Material mat{ m_fallbackTexture, m_defaultShader };
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_materialCache[key] = mat;
        return mat;
    }

    Material mat;
    mat.shaderID = m_defaultShader;

    if (m_gameInfo) {
        // ── Albedo ────────────────────────────────────────────────────────────
        std::string albedoPath = FindMaterialPath(key, *m_gameInfo);

        // Fallback: strip Source coordinate suffixes then try without maps/ prefix
        if (albedoPath.empty()) {
            std::string stripped = StripSourceSuffix(key);
            if (stripped.rfind("maps/", 0) == 0) {
                size_t secondSlash = stripped.find('/', 5);
                if (secondSlash != std::string::npos)
                    albedoPath = FindMaterialPath(stripped.substr(secondSlash + 1), *m_gameInfo);
            }
            if (albedoPath.empty() && stripped != key)
                albedoPath = FindMaterialPath(stripped, *m_gameInfo);
        }

        if (!albedoPath.empty()) {
            mat.textureID = LoadTexture(albedoPath);
        }

        // ── PBR maps — derive base name by stripping the extension ────────────
        // We use the resolved albedo path's stem so the suffix search lands in
        // the same directory, e.g.:
        //   albedo  → materials/brick/brickfloor001a.png
        //   roughs  → materials/brick/brickfloor001a_roughness.png  (etc.)
        //
        // We search by constructing the material key (relative, no extension)
        // and appending the suffix before the extension search loop.
        if (!albedoPath.empty()) {
            // Build the key without extension so TryLoadSuffix can append suffix+ext.
            // key already has no extension (it comes from the BSP texture name).
            mat.roughnessID = TryLoadSuffix(key, "_roughness");
            mat.metallicID  = TryLoadSuffix(key, "_metallic");
            mat.normalID    = TryLoadSuffix(key, "_normal");
        }
    }

    if (mat.textureID == 0) mat.textureID = m_fallbackTexture;

    // 3. Cache update
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_materialCache[key] = mat;
    }

    return mat;
}

// ─── TryLoadSuffix ───────────────────────────────────────────────────────────
// Looks for  materials/<baseKey><suffix>.<ext>  for every supported extension.
// Returns 0 if nothing found — caller uses scalar uniform fallback.

uint32_t MaterialSystem::TryLoadSuffix(const std::string& baseKey,
                                        const std::string& suffix) {
    if (!m_gameInfo) return 0;

    std::string path = FindMaterialPath(baseKey + suffix, *m_gameInfo);

    // Also try stripping Source coordinate suffixes from the base key
    if (path.empty()) {
        std::string stripped = StripSourceSuffix(baseKey);
        if (stripped != baseKey)
            path = FindMaterialPath(stripped + suffix, *m_gameInfo);

        // And the maps/ prefix strip
        if (path.empty() && stripped.rfind("maps/", 0) == 0) {
            size_t secondSlash = stripped.find('/', 5);
            if (secondSlash != std::string::npos)
                path = FindMaterialPath(
                    stripped.substr(secondSlash + 1) + suffix, *m_gameInfo);
        }
    }

    if (path.empty()) return 0;

    uint32_t id = LoadTexture(path);
    if (id != 0)
        Logger::Info("MaterialSystem: Loaded PBR map " + path);
    return id;
}

// ─── Private helpers ─────────────────────────────────────────────────────────

uint32_t MaterialSystem::CreateDummyTexture() {
    uint8_t checker[] = { 255,0,255,255, 0,0,0,255, 0,0,0,255, 255,0,255,255 };
    std::lock_guard<std::mutex> lock(g_glContextMutex);
    uint32_t tex;
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
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) return 0;

    std::lock_guard<std::mutex> lock(g_glContextMutex);
    uint32_t tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    Logger::Info("MaterialSystem: Loaded " + path);
    return tex;
}

} // namespace veex
