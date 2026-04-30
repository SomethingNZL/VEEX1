#pragma once
#include "veex/KeyValues.h"
#include "veex/Shader.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace veex {

class GameInfo;

// ── VMT Material Definition ──────────────────────────────────────────────────
// Parsed from .vmt files. Contains all material properties and texture references.
struct VMTMaterial {
    std::string name;                    // Material name (e.g., "brick/brickwall017a")
    std::string shaderType;              // Shader name from VMT (e.g., "LightmappedGeneric")
    
    // ── Texture References ─────────────────────────────────────────────────────
    std::string baseTexture;             // $basetexture
    std::string bumpMap;                 // $bumpmap
    std::string detailTexture;           // $detail
    std::string envMap;                  // $envmap
    std::string emissiveTexture;         // $selfillum or $emissive
    std::string roughnessTexture;        // $roughness (custom extension)
    std::string metallicTexture;         // $metallic (custom extension)
    
    // ── Material Parameters ────────────────────────────────────────────────────
    float detailScale = 1.0f;            // $detailscale
    float detailBlendFactor = 1.0f;      // $detailblendfactor
    int detailBlendMode = 0;             // $detailblendmode
    
    float bumpScale = 1.0f;              // $bumpscale
    float texScale = 1.0f;               // $texscale
    
    glm::vec2 baseTextureOffset = glm::vec2(0.0f);   // $baseTextureOffset
    glm::vec2 bumpTextureOffset = glm::vec2(0.0f);   // $bumpTextureOffset
    
    glm::vec3 envMapTint = glm::vec3(1.0f);          // $envmaptint
    float envMapSaturation = 1.0f;       // $envmapsaturation
    float envMapContrast = 1.0f;         // $envmapcontrast
    
    float alpha = 1.0f;                  // $alpha
    float translucency = 0.0f;           // $translucency
    
    // ── Material Flags ─────────────────────────────────────────────────────────
    bool isTranslucent = false;
    bool isAlphaTested = false;
    bool isTwoSided = false;
    bool isNoCull = false;
    bool isVertexAlpha = false;
    bool isVertexColor = false;
    bool hasNormalMapAlphaEnvMapMask = false;
    bool hasBaseTextureAlphaEnvMapMask = false;
    
    // ── Surface Properties ─────────────────────────────────────────────────────
    std::string surfaceProp;             // $surfaceprop (for physics/audio)
    
    // ── Keywords ───────────────────────────────────────────────────────────────
    std::vector<std::string> keywords;   // %keywords
    
    // ── Shader Feature Flags ───────────────────────────────────────────────────
    uint32_t shaderFlags = SHADER_FEATURE_NONE;
    
    // ── Helper Methods ─────────────────────────────────────────────────────────
    // Determine if this material needs normal mapping
    bool HasNormalMap() const { return !bumpMap.empty(); }
    
    // Determine if this material needs detail blending
    bool HasDetail() const { return !detailTexture.empty(); }
    
    // Determine if this material needs environment mapping
    bool HasEnvMap() const { return !envMap.empty(); }
    
    // Determine if this material is emissive
    bool HasEmissive() const { return !emissiveTexture.empty(); }
    
    // Get the appropriate shader feature flags for this material
    uint32_t GetShaderFlags() const { return shaderFlags; }
};

// ── VMT Loader ─────────────────────────────────────────────────────────────────
// Parses .vmt files and builds VMTMaterial definitions.
class VMTLoader {
public:
    VMTLoader() = default;
    ~VMTLoader() = default;
    
    // Load a VMT file from disk or VPK
    // Returns true if successful, false otherwise
    bool LoadFromFile(const std::string& path, const GameInfo& game);
    
    // Load a VMT from already-parsed KeyValues data
    bool LoadFromKeyValues(std::shared_ptr<KVNode> root);
    
    // Get the parsed material
    const VMTMaterial& GetMaterial() const { return m_material; }
    
    // Check if a VMT file exists for the given material name
    static bool MaterialExists(const std::string& materialName, const GameInfo& game);
    
    // Find a VMT file path for a material
    static std::string FindVMTPath(const std::string& materialName, const GameInfo& game);
    
    // ── Shader Mapping ─────────────────────────────────────────────────────────
    // Map Source shader names to our internal shader types and feature flags
    static uint32_t MapShaderToFlags(const std::string& shaderName);
    static std::string MapShaderToInternal(const std::string& shaderName);
    
private:
    // Parse the main material block
    void ParseMaterialBlock(KVNode* block);
    
    // Parse texture parameters
    void ParseTextureParams(KVNode* block);
    
    // Parse material parameters
    void ParseMaterialParams(KVNode* block);
    
    // Parse material flags
    void ParseMaterialFlags(KVNode* block);
    
    // Parse proxy blocks (TextureTransform, etc.)
    void ParseProxies(KVNode* proxiesBlock);
    
    // Parse shader variant blocks (_DX9, _DX8, _HDR, etc.)
    void ParseShaderVariants(KVNode* block);
    
    // Helper to extract string value from KVNode
    std::string GetString(KVNode* parent, const std::string& key, const std::string& defaultVal = "") const;
    
    // Helper to extract float value from KVNode
    float GetFloat(KVNode* parent, const std::string& key, float defaultVal = 0.0f) const;
    
    // Helper to extract int value from KVNode
    int GetInt(KVNode* parent, const std::string& key, int defaultVal = 0) const;
    
    // Helper to extract vec2 from KVNode (format: "[x y]" or "x y")
    glm::vec2 GetVec2(KVNode* parent, const std::string& key, const glm::vec2& defaultVal = glm::vec2(0.0f)) const;
    
    // Helper to extract vec3 from KVNode (format: "[x y z]" or "x y z")
    glm::vec3 GetVec3(KVNode* parent, const std::string& key, const glm::vec3& defaultVal = glm::vec3(1.0f)) const;
    
    VMTMaterial m_material;
    const GameInfo* m_gameInfo = nullptr;
};

} // namespace veex