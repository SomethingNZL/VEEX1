#include "veex/VMTLoader.h"
#include "veex/FileSystem.h"
#include "veex/GameInfo.h"
#include "veex/Logger.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace veex {

// ─── Static Helpers ────────────────────────────────────────────────────────────

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

static std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// ─── Shader Mapping ────────────────────────────────────────────────────────────

uint32_t VMTLoader::MapShaderToFlags(const std::string& shaderName) {
    std::string lower = ToLower(shaderName);
    
    // Strip variant suffixes (_DX9, _DX8, _HDR, etc.)
    size_t underscore = lower.find('_');
    if (underscore != std::string::npos) {
        std::string suffix = lower.substr(underscore);
        if (suffix == "_dx9" || suffix == "_dx8" || suffix == "_hdr" || 
            suffix == "_nobump" || suffix == "_nocubbed" || suffix == "_c17") {
            lower = lower.substr(0, underscore);
        }
    }
    
    uint32_t flags = SHADER_FEATURE_NONE;
    
    if (lower == "lightmappedgeneric") {
        flags = SHADER_FEATURE_LIGHTMAP | SHADER_FEATURE_PBR;
    } else if (lower == "vertexlitgeneric") {
        flags = SHADER_FEATURE_PBR;
    } else if (lower == "unlitgeneric") {
        flags = SHADER_FEATURE_NONE;
    } else if (lower == "skybox") {
        flags = SHADER_FEATURE_NONE; // Skybox uses its own shader
    } else if (lower == "refract" || lower == "refracting") {
        flags = SHADER_FEATURE_PBR | SHADER_FEATURE_REFRACTION;
    } else if (lower == "water") {
        flags = SHADER_FEATURE_PBR | SHADER_FEATURE_REFRACTION;
    } else if (lower == "sprite" || lower == "sprites") {
        flags = SHADER_FEATURE_ALPHA_TEST;
    } else if (lower == "particle") {
        flags = SHADER_FEATURE_ALPHA_TEST;
    } else if (lower == "worldvertextransition") {
        flags = SHADER_FEATURE_LIGHTMAP;
    } else {
        // Default to lightmapped for unknown shaders
        flags = SHADER_FEATURE_LIGHTMAP | SHADER_FEATURE_PBR;
    }
    
    return flags;
}

std::string VMTLoader::MapShaderToInternal(const std::string& shaderName) {
    std::string lower = ToLower(shaderName);
    
    // Strip variant suffixes
    size_t underscore = lower.find('_');
    if (underscore != std::string::npos) {
        std::string suffix = lower.substr(underscore);
        if (suffix == "_dx9" || suffix == "_dx8" || suffix == "_hdr" || 
            suffix == "_nobump" || suffix == "_nocubbed" || suffix == "_c17") {
            lower = lower.substr(0, underscore);
        }
    }
    
    if (lower == "lightmappedgeneric" || lower == "vertexlitgeneric" || 
        lower == "unlitgeneric" || lower == "refract" || lower == "water" ||
        lower == "worldvertextransition") {
        return "vr_standard";
    } else if (lower == "skybox") {
        return "skybox";
    } else if (lower == "sprite" || lower == "sprites" || lower == "particle") {
        return "vr_standard"; // Use standard with alpha test
    }
    
    // Default
    return "vr_standard";
}

// ─── Find VMT Path ─────────────────────────────────────────────────────────────

std::string VMTLoader::FindVMTPath(const std::string& materialName, const GameInfo& game) {
    // Try .vmt extension
    std::string path = ResolveAssetPath("materials/" + materialName + ".vmt", game);
    if (!path.empty()) return path;
    
    // Try lowercase
    std::string lower = ToLower(materialName);
    path = ResolveAssetPath("materials/" + lower + ".vmt", game);
    if (!path.empty()) return path;
    
    return "";
}

bool VMTLoader::MaterialExists(const std::string& materialName, const GameInfo& game) {
    return !FindVMTPath(materialName, game).empty();
}

// ─── Load From File ────────────────────────────────────────────────────────────

bool VMTLoader::LoadFromFile(const std::string& path, const GameInfo& game) {
    m_gameInfo = &game;
    
    auto kv = KeyValues::LoadFromFile(path);
    if (!kv) {
        Logger::Warn("VMTLoader: Failed to parse VMT file: " + path);
        return false;
    }
    
    return LoadFromKeyValues(kv);
}

bool VMTLoader::LoadFromKeyValues(std::shared_ptr<KVNode> root) {
    if (!root) return false;
    
    // Reset material
    m_material = VMTMaterial();
    
    // The root node should be the material name with the shader block as child
    m_material.name = root->key;
    
    // Find the shader block (first child that is a block)
    KVNode* shaderBlock = nullptr;
    for (auto& child : root->children) {
        if (child->IsBlock()) {
            m_material.shaderType = child->key;
            shaderBlock = child.get();
            break;
        }
    }
    
    if (!shaderBlock) {
        Logger::Warn("VMTLoader: No shader block found in VMT for: " + m_material.name);
        return false;
    }
    
    // Set shader flags
    m_material.shaderFlags = MapShaderToFlags(m_material.shaderType);
    
    // Parse the shader block
    ParseMaterialBlock(shaderBlock);
    
    Logger::Info("VMTLoader: Loaded material '" + m_material.name + "' (" + m_material.shaderType + ")");
    
    return true;
}

// ─── Parse Material Block ──────────────────────────────────────────────────────

void VMTLoader::ParseMaterialBlock(KVNode* block) {
    if (!block) return;
    
    // Parse texture parameters
    ParseTextureParams(block);
    
    // Parse material parameters
    ParseMaterialParams(block);
    
    // Parse material flags
    ParseMaterialFlags(block);
    
    // Parse shader variants (_DX9, _DX8, _HDR, etc.)
    ParseShaderVariants(block);
    
    // Parse proxies
    KVNode* proxiesNode = block->GetChild("Proxies");
    if (proxiesNode && proxiesNode->IsBlock()) {
        ParseProxies(proxiesNode);
    }
    
    // Parse keywords
    std::string keywords = GetString(block, "%keywords", "");
    if (!keywords.empty()) {
        std::istringstream iss(keywords);
        std::string keyword;
        while (iss >> keyword) {
            m_material.keywords.push_back(keyword);
        }
    }
}

// ─── Parse Texture Params ──────────────────────────────────────────────────────

void VMTLoader::ParseTextureParams(KVNode* block) {
    m_material.baseTexture = GetString(block, "$basetexture", "");
    m_material.bumpMap = GetString(block, "$bumpmap", "");
    m_material.detailTexture = GetString(block, "$detail", "");
    m_material.envMap = GetString(block, "$envmap", "");
    
    // Emissive/self-illumination
    std::string selfIllum = GetString(block, "$selfillum", "");
    std::string emissive = GetString(block, "$emissive", "");
    std::string emissiveBlink = GetString(block, "$emissiveblink", "");
    
    if (!selfIllum.empty()) {
        m_material.emissiveTexture = selfIllum;
    } else if (!emissive.empty()) {
        m_material.emissiveTexture = emissive;
    } else if (!emissiveBlink.empty()) {
        m_material.emissiveTexture = emissiveBlink;
    }
    
    // PBR textures (custom extensions for our engine)
    m_material.roughnessTexture = GetString(block, "$roughness", "");
    m_material.metallicTexture = GetString(block, "$metallic", "");
    
    // Also check for specmask (Source-style combined PBR map)
    std::string specMask = GetString(block, "$phongmask", "");
    if (specMask.empty()) {
        specMask = GetString(block, "$specmask", "");
    }
    // We'll use roughness slot for specmask if no explicit roughness
    if (!specMask.empty() && m_material.roughnessTexture.empty()) {
        m_material.roughnessTexture = specMask;
    }
}

// ─── Parse Material Params ─────────────────────────────────────────────────────

void VMTLoader::ParseMaterialParams(KVNode* block) {
    m_material.detailScale = GetFloat(block, "$detailscale", 1.0f);
    m_material.detailBlendFactor = GetFloat(block, "$detailblendfactor", 1.0f);
    m_material.detailBlendMode = GetInt(block, "$detailblendmode", 0);
    
    m_material.bumpScale = GetFloat(block, "$bumpscale", 1.0f);
    m_material.texScale = GetFloat(block, "$texscale", 1.0f);
    
    m_material.baseTextureOffset = GetVec2(block, "$baseTextureOffset", glm::vec2(0.0f));
    m_material.bumpTextureOffset = GetVec2(block, "$bumpTextureOffset", glm::vec2(0.0f));
    
    m_material.envMapTint = GetVec3(block, "$envmaptint", glm::vec3(1.0f));
    m_material.envMapSaturation = GetFloat(block, "$envmapsaturation", 1.0f);
    m_material.envMapContrast = GetFloat(block, "$envmapcontrast", 1.0f);
    
    m_material.alpha = GetFloat(block, "$alpha", 1.0f);
    m_material.translucency = GetFloat(block, "$translucency", 0.0f);
    
    m_material.surfaceProp = GetString(block, "$surfaceprop", "");
    
    // Check for translucency flags
    std::string translucent = GetString(block, "$translucent", "");
    if (!translucent.empty()) {
        m_material.isTranslucent = true;
    }
    
    std::string addSelf = GetString(block, "$addself", "");
    if (!addSelf.empty()) {
        m_material.isTranslucent = true;
    }
}

// ─── Parse Material Flags ──────────────────────────────────────────────────────

void VMTLoader::ParseMaterialFlags(KVNode* block) {
    // Translucency flags
    if (GetString(block, "$translucent", "") != "") m_material.isTranslucent = true;
    if (GetString(block, "$addself", "") != "") m_material.isTranslucent = true;
    
    // Alpha test
    if (GetString(block, "$alphatest", "") != "") m_material.isAlphaTested = true;
    
    // Two-sided / no cull
    if (GetString(block, "$nocull", "") != "") m_material.isNoCull = true;
    if (GetString(block, "$twosided", "") != "") m_material.isTwoSided = true;
    
    // Vertex alpha/color
    if (GetString(block, "$vertexalpha", "") != "") m_material.isVertexAlpha = true;
    if (GetString(block, "$vertexcolor", "") != "") m_material.isVertexColor = true;
    
    // Normal map alpha envmap mask
    if (GetString(block, "$normalmapalphaenvmapmask", "") == "1") {
        m_material.hasNormalMapAlphaEnvMapMask = true;
    }
    
    // Base texture alpha envmap mask
    if (GetString(block, "$basealphaenvmapmask", "") == "1") {
        m_material.hasBaseTextureAlphaEnvMapMask = true;
    }
    
    // Update shader flags based on material properties
    if (m_material.isTranslucent) {
        m_material.shaderFlags |= SHADER_FEATURE_TRANSLUCENT;
    }
    if (m_material.isAlphaTested) {
        m_material.shaderFlags |= SHADER_FEATURE_ALPHA_TEST;
    }
    if (m_material.HasNormalMap()) {
        m_material.shaderFlags |= SHADER_FEATURE_NORMAL_MAP;
    }
}

// ─── Parse Proxies ─────────────────────────────────────────────────────────────

void VMTLoader::ParseProxies(KVNode* proxiesBlock) {
    // Parse TextureTransform proxies for UV manipulation
    auto transforms = proxiesBlock->GetChildren("TextureTransform");
    for (KVNode* transform : transforms) {
        std::string resultVar = GetString(transform, "resultVar", "");
        
        if (resultVar == "$baseTextureTransform") {
            m_material.baseTextureOffset = GetVec2(transform, "translateVar", m_material.baseTextureOffset);
            // scaleVar would be $texscale which we already parsed
        } else if (resultVar == "$bumptransform") {
            m_material.bumpTextureOffset = GetVec2(transform, "translateVar", m_material.bumpTextureOffset);
            // scaleVar would be $bumpscale which we already parsed
        }
    }
}

// ─── Parse Shader Variants ─────────────────────────────────────────────────────

void VMTLoader::ParseShaderVariants(KVNode* block) {
    // Look for variant blocks (_DX9, _DX8, _HDR, etc.)
    // These override or extend the base material properties
    for (auto& child : block->children) {
        if (!child->IsBlock()) continue;
        
        std::string key = ToLower(child->key);
        
        // Check if this is a shader variant
        if (key.find("_dx9") != std::string::npos ||
            key.find("_dx8") != std::string::npos ||
            key.find("_hdr") != std::string::npos ||
            key.find("_nobump") != std::string::npos ||
            key.find("_nocubbed") != std::string::npos) {
            
            // Parse this variant's overrides
            // For now, we just merge the texture params (primarily bump maps)
            std::string variantBump = GetString(child.get(), "$bumpmap", "");
            if (!variantBump.empty() && m_material.bumpMap.empty()) {
                m_material.bumpMap = variantBump;
            }
            
            std::string variantBase = GetString(child.get(), "$basetexture", "");
            if (!variantBase.empty() && m_material.baseTexture.empty()) {
                m_material.baseTexture = variantBase;
            }
            
            // Check for normalmapalphaenvmapmask override
            std::string nmaem = GetString(child.get(), "$normalmapalphaenvmapmask", "");
            if (nmaem == "1") {
                m_material.hasNormalMapAlphaEnvMapMask = true;
            }
            
            // Only use the first matching variant
            break;
        }
    }
}

// ─── Helper Methods ────────────────────────────────────────────────────────────

std::string VMTLoader::GetString(KVNode* parent, const std::string& key, const std::string& defaultVal) const {
    if (!parent) return defaultVal;
    KVNode* child = parent->GetChild(key);
    if (!child) return defaultVal;
    return Trim(child->value);
}

float VMTLoader::GetFloat(KVNode* parent, const std::string& key, float defaultVal) const {
    std::string val = GetString(parent, key, "");
    if (val.empty()) return defaultVal;
    try {
        return std::stof(val);
    } catch (...) {
        return defaultVal;
    }
}

int VMTLoader::GetInt(KVNode* parent, const std::string& key, int defaultVal) const {
    std::string val = GetString(parent, key, "");
    if (val.empty()) return defaultVal;
    try {
        return std::stoi(val);
    } catch (...) {
        return defaultVal;
    }
}

glm::vec2 VMTLoader::GetVec2(KVNode* parent, const std::string& key, const glm::vec2& defaultVal) const {
    std::string val = GetString(parent, key, "");
    if (val.empty()) return defaultVal;
    
    // Handle "[x y]" format
    if (val.front() == '[' && val.back() == ']') {
        val = val.substr(1, val.size() - 2);
    }
    
    std::istringstream iss(val);
    float x, y;
    if (iss >> x >> y) {
        return glm::vec2(x, y);
    }
    return defaultVal;
}

glm::vec3 VMTLoader::GetVec3(KVNode* parent, const std::string& key, const glm::vec3& defaultVal) const {
    std::string val = GetString(parent, key, "");
    if (val.empty()) return defaultVal;
    
    // Handle "[x y z]" format
    if (val.front() == '[' && val.back() == ']') {
        val = val.substr(1, val.size() - 2);
    }
    
    std::istringstream iss(val);
    float x, y, z;
    if (iss >> x >> y >> z) {
        return glm::vec3(x, y, z);
    }
    return defaultVal;
}

} // namespace veex