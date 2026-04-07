#pragma once
#ifdef __MINGW32__
#include <cmath>
// Manually pull missing math symbols into the global namespace
namespace std { using ::acos; using ::asin; using ::atan; }
#endif

#define GLM_FORCE_PURE
#include <glm/glm.hpp>
#include <cmath>
// veex/Shader.h  —  ShaderKit: veex shader management & permutation system
//
// ShaderKit is the single owner of all GPU program objects. It handles:
//   • Source-engine-style static combo preprocessing (feature flags → #defines)
//   • Uniform location caching (critical on low-end GL 3.3 hardware)
//   • Hot-reload (LoadFromFiles can be called again on a live Shader)
//   • Sampler-unit binding abstraction so callers never touch raw GL units
//
// API is 100 % compatible with the previous Shader class.  The internal brand
// name "ShaderKit" appears in log output; the C++ type is still veex::Shader.

#include <string>
#include <unordered_map>
#include <vector>

namespace veex {

// ── Static Combo Feature Flags ────────────────────────────────────────────────
// Each flag maps to a #define injected into GLSL before compilation.
// Combine with | to build a ShaderCombo::flags bitmask.
enum ShaderFeatureFlags : uint32_t {
    SHADER_FEATURE_NONE         = 0,
    SHADER_FEATURE_NORMAL_MAP   = (1u << 0),   // → #define ENABLE_NORMAL_MAPPING 1
    SHADER_FEATURE_SPECULAR     = (1u << 1),   // → #define ENABLE_SPECULAR 1
    SHADER_FEATURE_LIGHTMAP     = (1u << 2),   // → #define ENABLE_LIGHTMAPS 1
    SHADER_FEATURE_FOG          = (1u << 3),   // → #define ENABLE_FOG 1
    SHADER_FEATURE_SKINNED      = (1u << 4),   // → #define ENABLE_SKINNING 1
    SHADER_FEATURE_PBR          = (1u << 5),   // → #define ENABLE_PBR 1
    SHADER_FEATURE_ALPHA_TEST   = (1u << 6),   // → #define ENABLE_ALPHA_TEST 1
    SHADER_FEATURE_TRANSLUCENT  = (1u << 7),   // → #define ENABLE_TRANSLUCENCY 1
    SHADER_FEATURE_REFRACTION   = (1u << 8),   // → #define ENABLE_REFRACTION 1
    SHADER_FEATURE_ENVMAP       = (1u << 9),   // → #define ENABLE_ENVMAP 1
};

// ── ShaderCombo ───────────────────────────────────────────────────────────────
// Uniquely identifies a compiled permutation of a shader pair.
// Equivalent to a Source 2 "static combo" tuple.
struct ShaderCombo {
    std::string name;                                      // human-readable label for logs
    uint32_t    flags  = SHADER_FEATURE_NONE;              // feature-flag bitmask
    std::unordered_map<std::string, std::string> macros;   // arbitrary extra #defines
};

// ── MaterialParams ────────────────────────────────────────────────────────────
// Per-material scalar/vector values forwarded from BSP → Renderer → ShaderKit.
// ShaderKit uploads these as uniforms once per batch; the shader reads them
// without branching on missing maps (zeroed fields = safe defaults).
struct MaterialParams {
    float roughness    = 0.7f;   // PBR roughness scalar (overridden by roughness map)
    float metallic     = 0.0f;   // PBR metallic scalar  (overridden by metallic map)
    float alphaRef     = 0.5f;   // alpha-test cutoff [0,1]
    float emissiveScale= 1.0f;   // emissive intensity multiplier
    float refractionScale = 0.05f; // water/glass refraction strength
    float envmapTint   = 1.0f;   // environment-map contribution scale

    // Texture-unit presence flags — ShaderKit sets u_Has* uniforms from these.
    bool hasNormalMap   = false;
    bool hasRoughnessMap= false;
    bool hasMetallicMap = false;
    bool hasSpecMaskMap = false;
    bool hasEmissiveMap = false;
};

// ── SceneParams ───────────────────────────────────────────────────────────────
// World-level values the Renderer exposes to ShaderKit for UBO upload.
// Kept in a plain struct so non-GL code can fill it without GL headers.
// std140 alignment rules apply when copied into the GPU buffer.
#include <glm/glm.hpp>

struct SceneParams {
    glm::mat4 viewProj;           // camera view-projection matrix
    glm::vec4 cameraPos;          // world-space camera position (w unused)
    glm::vec4 sunDirection;       // normalised sun direction (w=0 → direction vector)
    glm::vec4 sunColor;           // linear RGB sun color   (w=1 for alignment)
    glm::vec4 fogParams;          // x=start, y=end, z=density, w unused
    glm::vec4 fogColor;           // linear RGB fog color   (w=1)
    glm::vec4 ambientCube[6];     // Source-style ambient cube (±X ±Y ±Z faces)
    // ── Water / glass ────────────────────────────────────────────────────────
    glm::vec4 waterParams;        // x=time, y=waveScale, z=waveSpeed, w=opacity
    glm::vec4 waterColor;         // tint under the surface
    // ── Misc ─────────────────────────────────────────────────────────────────
    float     lightmapExposure;   // Source overbright scale applied in fragment
    float     time;               // seconds since startup (for animated shaders)
    float     _pad0, _pad1;       // keep struct size a multiple of 16 bytes
};
static_assert(sizeof(SceneParams) % 16 == 0,
    "SceneParams must be 16-byte aligned for std140");

// ── Shader (ShaderKit) ────────────────────────────────────────────────────────
class Shader {
public:
    Shader();
    ~Shader();

    // Non-copyable; move is fine.
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) noexcept;
    Shader& operator=(Shader&&) noexcept;

    // ── Compilation ──────────────────────────────────────────────────────────
    // Loads, preprocesses (static combos), compiles, and links a shader pair.
    // Safe to call on an already-live Shader — old program is deleted first.
    bool LoadFromFiles(const std::string& vertexPath,
                       const std::string& fragmentPath,
                       const ShaderCombo& combo = {});

    // ── Activation ───────────────────────────────────────────────────────────
    void Bind()   const;
    void Unbind() const;

    // ── Sampler units ────────────────────────────────────────────────────────
    // Sets the sampler uniform to the given texture-unit index.
    // Callers should never pass raw GL unit indices elsewhere.
    void BindSampler(const std::string& name, uint32_t unit);

    // ── Uniform setters ──────────────────────────────────────────────────────
    // Cached — first call queries glGetUniformLocation; subsequent calls hit
    // the hash-map and cost ~1 ns each.
    void SetInt  (const std::string& name, int   v);
    void SetFloat(const std::string& name, float v);
    void SetVec2 (const std::string& name, float x, float y);
    void SetVec3 (const std::string& name, float x, float y, float z);
    void SetVec4 (const std::string& name, float x, float y, float z, float w);
    void SetMat3 (const std::string& name, const float* m);   // column-major
    void SetMat4 (const std::string& name, const float* m);   // column-major

    // Legacy overloaded aliases (kept for callers that used SetUniform directly)
    void SetUniform(const std::string& name, int   v) { SetInt(name, v); }
    void SetUniform(const std::string& name, float v) { SetFloat(name, v); }
    void SetUniform(const std::string& name, float x, float y)
        { SetVec2(name, x, y); }
    void SetUniform(const std::string& name, float x, float y, float z)
        { SetVec3(name, x, y, z); }
    void SetUniform(const std::string& name, float x, float y, float z, float w)
        { SetVec4(name, x, y, z, w); }
    void SetUniform(const std::string& name, const float* mat4ptr)
        { SetMat4(name, mat4ptr); }
    void SetUniformMat3(const std::string& name, const float* mat3ptr)
        { SetMat3(name, mat3ptr); }

    // ── Material & scene uniform upload ──────────────────────────────────────
    // These methods let ShaderKit own the upload logic so Renderer and BSP
    // never call raw glUniform* themselves.
    void UploadMaterialParams(const MaterialParams& mat);
    void UploadSceneBlock    (uint32_t uboHandle, const SceneParams& scene);

    // ── Introspection ────────────────────────────────────────────────────────
    uint32_t GetProgramID() const { return m_program; }
    bool     IsValid()      const { return m_program != 0; }

    // Binds the "SceneBlock" UBO at binding point 0 (call after Bind()).
    void BindSceneUBO(uint32_t uboHandle) const;

private:
    // ── Internals ────────────────────────────────────────────────────────────
    std::string PreprocessSource(const std::string& rawSource, const ShaderCombo& combo);
    bool CompileShader(uint32_t& outShader, const std::string& source, uint32_t glType);
    bool LinkProgram  (uint32_t vShader, uint32_t fShader);
    int  GetUniformLocation(const std::string& name);
    void LogShaderError(uint32_t glObject, uint32_t glType, bool isProgram) const;

    uint32_t m_program = 0;
    std::string m_vertexPath;
    std::string m_fragmentPath;

    // Uniform location cache — populated lazily on first Set* call.
    std::unordered_map<std::string, int> m_uniformLocations;
};

} // namespace veex
