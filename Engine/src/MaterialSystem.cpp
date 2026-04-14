#include "veex/MaterialSystem.h"
#include "veex/FileSystem.h"
#include "veex/GameInfo.h"
#include "veex/Logger.h"
#include "veex/VTFLoader.h"
#include "veex/VMTLoader.h"
#include "veex/GLHeaders.h"

#include "../third_party/stb/stb_image.h"

#include <glad/gl.h>
#include <algorithm>
#include <cctype>
#include <filesystem>

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
// VTF is checked first for Source Engine compatibility
static const char* kExts[] = { ".vtf", ".png", ".jpg", ".jpeg", ".tga", ".bmp", nullptr };

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
                              mat.metallicID, mat.normalID,
                              mat.specMaskID }) {
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
        Material mat;
        mat.textureID = m_fallbackTexture;
        mat.diffuseID = m_fallbackTexture;
        mat.shaderID = m_defaultShader;
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_materialCache[key] = mat;
        return mat;
    }

    Material mat;
    mat.shaderID = m_defaultShader;

    if (m_gameInfo) {
        // ── Try VMT First ──────────────────────────────────────────────────────
        std::string vmtPath = VMTLoader::FindVMTPath(key, *m_gameInfo);
        if (!vmtPath.empty()) {
            VMTLoader vmtLoader;
            if (vmtLoader.LoadFromFile(vmtPath, *m_gameInfo)) {
                const VMTMaterial& vmt = vmtLoader.GetMaterial();
                
                // Store VMT data in material
                mat.vmtData = std::make_shared<VMTMaterial>(vmt);
                
                // Copy VMT parameters to material
                mat.baseTextureScale = glm::vec2(vmt.texScale);
                mat.baseTextureOffset = vmt.baseTextureOffset;
                mat.bumpTextureScale = glm::vec2(vmt.bumpScale);
                mat.bumpTextureOffset = vmt.bumpTextureOffset;
                mat.detailScale = vmt.detailScale;
                mat.detailBlendFactor = vmt.detailBlendFactor;
                mat.detailBlendMode = vmt.detailBlendMode;
                mat.envMapTint = vmt.envMapTint;
                mat.envMapSaturation = vmt.envMapSaturation;
                mat.envMapContrast = vmt.envMapContrast;
                mat.isTranslucent = vmt.isTranslucent;
                mat.isAlphaTested = vmt.isAlphaTested;
                mat.isTwoSided = vmt.isTwoSided;
                mat.isNoCull = vmt.isNoCull;
                mat.hasEnvMap = vmt.HasEnvMap();
                mat.shaderFlags = vmt.shaderFlags;
                
                // Load base texture from VMT
                if (!vmt.baseTexture.empty()) {
                    std::string albedoPath = FindMaterialPath(vmt.baseTexture, *m_gameInfo);
                    if (!albedoPath.empty()) {
                        mat.textureID = LoadTexture(albedoPath, true);
                        mat.diffuseID = mat.textureID;
                    }
                }
                
                // Load bump map from VMT
                if (!vmt.bumpMap.empty()) {
                    std::string normalPath = FindMaterialPath(vmt.bumpMap, *m_gameInfo);
                    if (!normalPath.empty()) {
                        mat.normalID = LoadTexture(normalPath);
                    }
                }
                
                // Load detail texture from VMT
                if (!vmt.detailTexture.empty()) {
                    std::string detailPath = FindMaterialPath(vmt.detailTexture, *m_gameInfo);
                    if (!detailPath.empty()) {
                        mat.detailID = LoadTexture(detailPath);
                        mat.detailPath = vmt.detailTexture;
                    }
                }
                
                // Load roughness/specmask from VMT
                if (!vmt.roughnessTexture.empty()) {
                    std::string roughPath = FindMaterialPath(vmt.roughnessTexture, *m_gameInfo);
                    if (!roughPath.empty()) {
                        mat.roughnessID = LoadTexture(roughPath);
                    }
                }
                
                // Log VMT material load
                Logger::Info("MaterialSystem: Loaded VMT material '" + key + "' (" + vmt.shaderType + ")");
            }
        }
        
        // ── Fallback: Direct texture loading (no VMT) ──────────────────────────
        if (mat.textureID == 0) {
            // ── Albedo ────────────────────────────────────────────────────────
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
                mat.textureID = LoadTexture(albedoPath, true);
                mat.diffuseID = mat.textureID;
            }

            // ── PBR maps — derive base name by stripping the extension ────────
            if (!albedoPath.empty()) {
                mat.roughnessID = TryLoadSuffix(key, "_roughness");
                mat.metallicID  = TryLoadSuffix(key, "_metallic");
                mat.normalID    = TryLoadSuffix(key, "_normal");
                mat.specMaskID  = TryLoadSuffix(key, "_specmask");
                if (mat.normalID == 0)
                    mat.normalID = TryLoadSuffix(key, "_bumpmap");
                if (mat.normalID == 0)
                    mat.normalID = TryLoadSuffix(key, "_bump");
            }
        }
    }

    if (mat.textureID == 0) {
        mat.textureID = m_fallbackTexture;
        mat.diffuseID = m_fallbackTexture;
    }

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

Material MaterialSystem::LoadMaterial(const std::string& basePath) {
    Material mat;
    mat.textureID  = LoadTexture(basePath + "_diffuse.png", true);
    mat.diffuseID  = mat.textureID;
    mat.metallicID = LoadTexture(basePath + "_metallic.png");
    mat.roughnessID= LoadTexture(basePath + "_roughness.png");
    mat.normalID   = LoadTexture(basePath + "_normal.png");
    return mat;
}

GLuint MaterialSystem::LoadTexture(const std::string& path, bool srgb) {
    if (!std::filesystem::exists(path)) {
        Logger::Warn("MaterialSystem: Texture not found: " + path);
        return 0;
    }

    // Check if this is a VTF file
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
        if (ext == ".vtf") {
            // Use VTF loader
            VTFLoader loader;
            if (!loader.LoadFromFile(path)) {
                Logger::Warn("MaterialSystem: Failed to load VTF texture: " + path);
                return 0;
            }
            
            int width = 0, height = 0;
            
            // Check if this is a compressed texture
            if (loader.IsCompressed()) {
                // Check if DXT compression is supported
                if (IsDXTSupported()) {
                    // Use compressed texture upload
                    uint32_t glInternalFormat = 0;
                    size_t dataSize = 0;
                    const uint8_t* compressedData = loader.GetCompressedData(&width, &height, &glInternalFormat, &dataSize);
                    
                    if (!compressedData || dataSize == 0 || glInternalFormat == 0) {
                        Logger::Warn("MaterialSystem: VTF loader returned invalid compressed data: " + path);
                        return 0;
                    }
                    
                    GLuint textureID = 0;
                    glGenTextures(1, &textureID);
                    glBindTexture(GL_TEXTURE_2D, textureID);
                    
                    // Use glCompressedTexImage2D for compressed formats
                    glCompressedTexImage2D(GL_TEXTURE_2D, 0, glInternalFormat, width, height, 0, dataSize, compressedData);
                    
                    // Upload mipmaps if available in the VTF file
                    int mipCount = loader.GetMipCount();
                    if (mipCount > 1) {
                        for (int mip = 1; mip < mipCount; mip++) {
                            int mipWidth = std::max(1, width >> mip);
                            int mipHeight = std::max(1, height >> mip);
                            size_t mipDataSize = 0;
                            const uint8_t* mipData = loader.GetMipData(mip, &mipDataSize);
                            
                            if (mipData && mipDataSize > 0) {
                                glCompressedTexImage2D(GL_TEXTURE_2D, mip, glInternalFormat, mipWidth, mipHeight, 0, mipDataSize, mipData);
                            }
                        }
                    }
                    
                    // Set texture parameters based on VTF flags
                    uint32_t flags = loader.GetFlags();
                    
                    // Wrap mode
                    if (flags & static_cast<uint32_t>(VTFFlags::CLAMPS)) {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    } else {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    }
                    
                    if (flags & static_cast<uint32_t>(VTFFlags::CLAMPT)) {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    } else {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    }
                    
                    // Filter mode
                    if (flags & static_cast<uint32_t>(VTFFlags::POINTSAMPLE)) {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    } else {
                        // Use mipmaps if available, otherwise fall back to GL_LINEAR
                        if (mipCount > 1) {
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                        } else {
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        }
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    }
                    
                    // Anisotropic filtering
                    if (flags & static_cast<uint32_t>(VTFFlags::ANISOTROPIC)) {
                        float maxAniso = 0.0f;
                        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 
                                      static_cast<GLint>(maxAniso));
                    }
                    
                    glBindTexture(GL_TEXTURE_2D, 0);
                    Logger::Info("MaterialSystem: Loaded compressed VTF texture " + path);
                    return textureID;
                } else {
                    // DXT not supported, fall back to decompressed RGBA
                    Logger::Warn("MaterialSystem: DXT compression not supported, falling back to RGBA for: " + path);
                    const uint8_t* data = loader.GetRGBAData(&width, &height);
                    if (!data) {
                        Logger::Error("MaterialSystem: VTF loader returned no data: " + path);
                        return 0;
                    }
                    
                    GLuint textureID = 0;
                    glGenTextures(1, &textureID);
                    glBindTexture(GL_TEXTURE_2D, textureID);
                    
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                    glGenerateMipmap(GL_TEXTURE_2D);
                    
                    // Set texture parameters
                    uint32_t flags = loader.GetFlags();
                    if (flags & static_cast<uint32_t>(VTFFlags::CLAMPS)) {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    } else {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    }
                    if (flags & static_cast<uint32_t>(VTFFlags::CLAMPT)) {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    } else {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    }
                    if (flags & static_cast<uint32_t>(VTFFlags::POINTSAMPLE)) {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    } else {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    }
                    if (flags & static_cast<uint32_t>(VTFFlags::ANISOTROPIC)) {
                        float maxAniso = 0.0f;
                        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, static_cast<GLint>(maxAniso));
                    }
                    
                    glBindTexture(GL_TEXTURE_2D, 0);
                    Logger::Info("MaterialSystem: Loaded decompressed VTF texture " + path);
                    return textureID;
                }
            } else {
                // Use uncompressed texture upload
                const uint8_t* data = loader.GetRGBAData(&width, &height);
                if (!data) {
                    Logger::Warn("MaterialSystem: VTF loader returned no data: " + path);
                    return 0;
                }
                
                // Determine if texture has alpha
                bool hasAlpha = loader.HasAlpha();
                
                GLuint textureID = 0;
                glGenTextures(1, &textureID);
                glBindTexture(GL_TEXTURE_2D, textureID);
                
                GLenum internalFormat = hasAlpha ? (srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8) : (srgb ? GL_SRGB8 : GL_RGB8);
                GLenum format = hasAlpha ? GL_RGBA : GL_RGB;
                
                glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                             format, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);
                
                // Set texture parameters based on VTF flags
                uint32_t flags = loader.GetFlags();
                
                // Wrap mode
                if (flags & static_cast<uint32_t>(VTFFlags::CLAMPS)) {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                } else {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                }
                
                if (flags & static_cast<uint32_t>(VTFFlags::CLAMPT)) {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                } else {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                }
                
                // Filter mode
                if (flags & static_cast<uint32_t>(VTFFlags::POINTSAMPLE)) {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                } else if (flags & static_cast<uint32_t>(VTFFlags::TRILINEAR)) {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                } else {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                }
                
                // Anisotropic filtering
                if (flags & static_cast<uint32_t>(VTFFlags::ANISOTROPIC)) {
                    float maxAniso = 0.0f;
                    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 
                                  static_cast<GLint>(maxAniso));
                }
                
                glBindTexture(GL_TEXTURE_2D, 0);
                Logger::Info("MaterialSystem: Loaded VTF texture " + path);
                return textureID;
            }
        }
    
    // Standard image loading via stb_image
    int width = 0, height = 0, channels = 0;
    stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) {
        Logger::Warn("MaterialSystem: Failed to load texture: " + path);
        return 0;
    }

    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    GLenum internalFormat = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    Logger::Info("MaterialSystem: Loaded texture " + path);
    return textureID;
}

} // namespace veex
