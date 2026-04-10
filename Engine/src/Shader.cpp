// veex/Shader.cpp  —  ShaderKit implementation
//
// All log output is prefixed with [ShaderKit] for easy grep/filtering.
// Internally this file is the ShaderKit subsystem; externally it is
// the veex::Shader class (API-compatible with the previous version).

#include "veex/Shader.h"
#include "veex/Logger.h"
#include <glad/gl.h>
#include <fstream>
#include <sstream>
#include <utility>

namespace veex {

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string ReadFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ── Shader ctor / dtor / move ─────────────────────────────────────────────────

Shader::Shader() : m_program(0) {}

Shader::~Shader()
{
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

Shader::Shader(Shader&& o) noexcept
    : m_program(o.m_program)
    , m_vertexPath(std::move(o.m_vertexPath))
    , m_fragmentPath(std::move(o.m_fragmentPath))
    , m_uniformLocations(std::move(o.m_uniformLocations))
{
    o.m_program = 0;
}

Shader& Shader::operator=(Shader&& o) noexcept
{
    if (this != &o) {
        if (m_program) glDeleteProgram(m_program);
        m_program        = o.m_program;
        m_vertexPath     = std::move(o.m_vertexPath);
        m_fragmentPath   = std::move(o.m_fragmentPath);
        m_uniformLocations = std::move(o.m_uniformLocations);
        o.m_program = 0;
    }
    return *this;
}

// ── LoadFromFiles ─────────────────────────────────────────────────────────────

bool Shader::LoadFromFiles(const std::string& vertexPath,
                           const std::string& fragmentPath,
                           const ShaderCombo& combo)
{
    m_vertexPath   = vertexPath;
    m_fragmentPath = fragmentPath;

    // ── Read source ───────────────────────────────────────────────────────────
    std::string rawVert = ReadFile(vertexPath);
    if (rawVert.empty()) {
        Logger::Error("[ShaderKit] Cannot open vertex shader: " + vertexPath);
        return false;
    }
    std::string rawFrag = ReadFile(fragmentPath);
    if (rawFrag.empty()) {
        Logger::Error("[ShaderKit] Cannot open fragment shader: " + fragmentPath);
        return false;
    }

    // ── Hot-reload cleanup ────────────────────────────────────────────────────
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
        m_uniformLocations.clear();
        Logger::Info("[ShaderKit] Hot-reloading combo [" + combo.name + "]");
    }

    // ── Preprocess (static combos + macro injection) ──────────────────────────
    std::string procVert = PreprocessSource(rawVert, combo);
    std::string procFrag = PreprocessSource(rawFrag, combo);

    // ── Compile ───────────────────────────────────────────────────────────────
    uint32_t vShader = 0, fShader = 0;
    bool ok = true;
    ok = ok && CompileShader(vShader, procVert, GL_VERTEX_SHADER);
    ok = ok && CompileShader(fShader, procFrag, GL_FRAGMENT_SHADER);

    if (!ok) {
        if (vShader) glDeleteShader(vShader);
        if (fShader) glDeleteShader(fShader);
        Logger::Error("[ShaderKit] Compilation failed for combo [" + combo.name + "]");
        return false;
    }

    // ── Link ──────────────────────────────────────────────────────────────────
    if (!LinkProgram(vShader, fShader)) {
        glDeleteShader(vShader);
        glDeleteShader(fShader);
        Logger::Error("[ShaderKit] Linking failed for combo [" + combo.name
                      + "] (vert=" + vertexPath + ")");
        return false;
    }

    glDeleteShader(vShader);
    glDeleteShader(fShader);

    // Count how many feature flags are set for a useful summary line.
    int featureCount = 0;
    for (uint32_t b = combo.flags; b; b &= b - 1) ++featureCount;

    Logger::Info("[ShaderKit] Compiled combo [" + combo.name + "]"
                 "  features=" + std::to_string(featureCount)
                 + "  macros=" + std::to_string(combo.macros.size())
                 + "  glProgram=" + std::to_string(m_program));
    return true;
}

// ── PreprocessSource ──────────────────────────────────────────────────────────
//
// Builds the final GLSL source that will be handed to the driver:
//   1. #version 330 core  (always first — GL spec requirement)
//   2. Static combo #defines derived from ShaderCombo::flags
//   3. Arbitrary extra macros from ShaderCombo::macros
//   4. The raw source with its own #version line stripped

std::string Shader::PreprocessSource(const std::string& rawSource,
                                     const ShaderCombo& combo)
{
    std::ostringstream out;

    // 1. Version directive — must be the very first token the driver sees.
    out << "#version 330 core\n\n";

    // 2. Feature-flag defines (ShaderKit static combos).
    out << "// ── ShaderKit static combos ─────────────────────────────────\n";
    auto flag = [&](uint32_t bit, const char* define) {
        if (combo.flags & bit) out << "#define " << define << " 1\n";
    };
    flag(SHADER_FEATURE_NORMAL_MAP,  "ENABLE_NORMAL_MAPPING");
    flag(SHADER_FEATURE_SPECULAR,    "ENABLE_SPECULAR");
    flag(SHADER_FEATURE_LIGHTMAP,    "ENABLE_LIGHTMAPS");
    flag(SHADER_FEATURE_FOG,         "ENABLE_FOG");
    flag(SHADER_FEATURE_SKINNED,     "ENABLE_SKINNING");
    flag(SHADER_FEATURE_PBR,         "ENABLE_PBR");
    flag(SHADER_FEATURE_ALPHA_TEST,  "ENABLE_ALPHA_TEST");
    flag(SHADER_FEATURE_TRANSLUCENT, "ENABLE_TRANSLUCENCY");
    flag(SHADER_FEATURE_REFRACTION,  "ENABLE_REFRACTION");
    flag(SHADER_FEATURE_ENVMAP,      "ENABLE_ENVMAP");

    // 3. Caller-supplied macros (arbitrary key=value pairs).
    if (!combo.macros.empty()) {
        out << "// ── ShaderKit extra macros ──────────────────────────────────\n";
        for (const auto& [key, val] : combo.macros)
            out << "#define " << key << " " << val << "\n";
    }
    out << "// ─────────────────────────────────────────────────────────────\n\n";

    // 4. Raw source — strip the original #version line to avoid a duplicate.
    std::string clean = rawSource;
    size_t verPos = clean.find("#version");
    if (verPos != std::string::npos) {
        size_t eol = clean.find('\n', verPos);
        if (eol != std::string::npos)
            clean.erase(0, eol + 1);
        else
            clean.erase(0, clean.size());
    }
    out << clean;
    return out.str();
}

// ── CompileShader ─────────────────────────────────────────────────────────────

bool Shader::CompileShader(uint32_t& outShader,
                           const std::string& source,
                           uint32_t glType)
{
    outShader = glCreateShader(glType);
    const char* src = source.c_str();
    glShaderSource(outShader, 1, &src, nullptr);
    glCompileShader(outShader);

    int success = 0;
    glGetShaderiv(outShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        LogShaderError(outShader, glType, /*isProgram=*/false);
        glDeleteShader(outShader);
        outShader = 0;
        return false;
    }
    return true;
}

// ── LinkProgram ───────────────────────────────────────────────────────────────

bool Shader::LinkProgram(uint32_t vShader, uint32_t fShader)
{
    m_program = glCreateProgram();
    glAttachShader(m_program, vShader);
    glAttachShader(m_program, fShader);
    glLinkProgram(m_program);

    int success = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        LogShaderError(m_program, 0, /*isProgram=*/true);
        glDeleteProgram(m_program);
        m_program = 0;
        return false;
    }
    return true;
}

// ── LogShaderError ────────────────────────────────────────────────────────────

void Shader::LogShaderError(uint32_t glObject, uint32_t glType, bool isProgram) const
{
    int logLen = 0;
    if (isProgram)
        glGetProgramiv(glObject, GL_INFO_LOG_LENGTH, &logLen);
    else
        glGetShaderiv(glObject, GL_INFO_LOG_LENGTH, &logLen);

    if (logLen <= 0) return;

    std::vector<char> buf(logLen);
    if (isProgram) {
        glGetProgramInfoLog(glObject, logLen, nullptr, buf.data());
        Logger::Error("[ShaderKit] Link error (vert=" + m_vertexPath + "):\n"
                      + std::string(buf.data()));
    } else {
        glGetShaderInfoLog(glObject, logLen, nullptr, buf.data());
        const char* stageLabel = (glType == GL_VERTEX_SHADER) ? "vertex" : "fragment";
        Logger::Error(std::string("[ShaderKit] Compile error in ") + stageLabel
                      + " shader (" + m_vertexPath + "):\n"
                      + std::string(buf.data()));
    }
}

// ── Bind / Unbind ─────────────────────────────────────────────────────────────

void Shader::Bind() const
{
    if (m_program) glUseProgram(m_program);
}

void Shader::Unbind() const
{
    glUseProgram(0);
}

// ── BindSampler ───────────────────────────────────────────────────────────────

void Shader::BindSampler(const std::string& name, uint32_t unit)
{
    // Sampler uniforms are set to the integer texture-unit index.
    // ShaderKit owns this mapping so callers never touch raw GL units.
    SetInt(name, static_cast<int>(unit));
}

// ── BindSceneUBO ──────────────────────────────────────────────────────────────

void Shader::BindSceneUBO(uint32_t uboHandle) const
{
    if (!m_program || !uboHandle) return;
    uint32_t blockIdx = glGetUniformBlockIndex(m_program, "SceneBlock");
    if (blockIdx != GL_INVALID_INDEX)
        glUniformBlockBinding(m_program, blockIdx, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboHandle);
}

// ── UploadSceneBlock ──────────────────────────────────────────────────────────
// Fills the GPU-side UBO from a SceneParams struct.
// Caller must have called Bind() first.

void Shader::UploadSceneBlock(uint32_t uboHandle, const SceneParams& scene)
{
    if (!uboHandle) return;
    glBindBuffer(GL_UNIFORM_BUFFER, uboHandle);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneParams), &scene);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    BindSceneUBO(uboHandle);
}

// ── UploadMaterialParams ──────────────────────────────────────────────────────
// Uploads all per-material scalar uniforms in a single call.
// Shader must already be Bind()-ed.

void Shader::UploadMaterialParams(const MaterialParams& mat)
{
    SetFloat("u_Roughness",       mat.roughness);
    SetFloat("u_Metallic",        mat.metallic);
    SetFloat("u_AlphaRef",        mat.alphaRef);
    SetFloat("u_EmissiveScale",   mat.emissiveScale);
    SetFloat("u_RefractionScale", mat.refractionScale);
    SetFloat("u_EnvmapTint",      mat.envmapTint);
    SetInt  ("u_HasNormalMap",    mat.hasNormalMap    ? 1 : 0);
    SetInt  ("u_HasRoughnessMap", mat.hasRoughnessMap ? 1 : 0);
    SetInt  ("u_HasMetallicMap",  mat.hasMetallicMap  ? 1 : 0);
    SetInt  ("u_HasEmissiveMap",  mat.hasEmissiveMap  ? 1 : 0);

    // ── PBR-lite tuning parameters ─────────────────────────────────────────────
    // Upload the new uniforms for the hybrid RNM + Source lightmap shading model.
    SetFloat("u_RNMScale",              mat.rnmScale);
    SetFloat("u_LightmapSoftness",      mat.lightmapSoftness);
    SetFloat("u_DiffuseFlattening",     mat.diffuseFlattening);
    SetFloat("u_EdgePower",             mat.edgePower);
    SetFloat("u_GeometricRoughnessPower", mat.geometricRoughnessPower);
    SetFloat("u_LightmapBrightness",    mat.lightmapBrightness);
}

// ── Feature-based shader selection ──────────────────────────────────────────────
// Determines the appropriate shader feature flags based on material properties.
// This enables per-face shader selection for optimal rendering.

uint32_t Shader::GetShaderFeaturesForMaterial(const std::string& materialName,
                                               bool hasNormalMap,
                                               bool hasLightmap,
                                               bool isTranslucent,
                                               bool isSky,
                                               bool isWater)
{
    uint32_t flags = SHADER_FEATURE_NONE;

    // ── Base features ───────────────────────────────────────────────────────────
    // All world geometry gets fog by default
    flags |= SHADER_FEATURE_FOG;

    // ── Material-specific features ──────────────────────────────────────────────
    if (hasNormalMap) {
        flags |= SHADER_FEATURE_NORMAL_MAP;
    }

    if (hasLightmap) {
        flags |= SHADER_FEATURE_LIGHTMAP;
    }

    // ── Special material types ──────────────────────────────────────────────────
    if (isTranslucent) {
        flags |= SHADER_FEATURE_TRANSLUCENT | SHADER_FEATURE_ALPHA_TEST;
    }

    if (isSky) {
        // Sky materials use envmap and don't need lightmaps
        flags |= SHADER_FEATURE_ENVMAP;
        flags &= ~SHADER_FEATURE_LIGHTMAP;
        flags &= ~SHADER_FEATURE_FOG;
    }

    if (isWater) {
        flags |= SHADER_FEATURE_REFRACTION | SHADER_FEATURE_TRANSLUCENT;
        flags |= SHADER_FEATURE_ENVMAP;
    }

    // ── PBR-lite: Always enable for world geometry ──────────────────────────────
    // The hybrid RNM + Source lightmap model is our base shading approach
    flags |= SHADER_FEATURE_PBR;

    // ── Material name heuristics (Source Engine style) ──────────────────────────
    // Check material name for common patterns
    if (materialName.find("metal") != std::string::npos ||
        materialName.find("chrome") != std::string::npos ||
        materialName.find("steel") != std::string::npos) {
        flags |= SHADER_FEATURE_SPECULAR;
    }

    if (materialName.find("glass") != std::string::npos ||
        materialName.find("window") != std::string::npos) {
        flags |= SHADER_FEATURE_TRANSLUCENT | SHADER_FEATURE_REFRACTION;
    }

    return flags;
}

// ── Uniform location cache ────────────────────────────────────────────────────

int Shader::GetUniformLocation(const std::string& name)
{
    auto it = m_uniformLocations.find(name);
    if (it != m_uniformLocations.end()) return it->second;

    int loc = glGetUniformLocation(m_program, name.c_str());
    // loc == -1 is valid (uniform optimized out by driver); we still cache it
    // so we don't re-query on every frame.
    m_uniformLocations[name] = loc;
    return loc;
}

// ── Uniform setters ───────────────────────────────────────────────────────────

void Shader::SetInt(const std::string& name, int v)
{
    int loc = GetUniformLocation(name);
    if (loc != -1) glUniform1i(loc, v);
}

void Shader::SetFloat(const std::string& name, float v)
{
    int loc = GetUniformLocation(name);
    if (loc != -1) glUniform1f(loc, v);
}

void Shader::SetVec2(const std::string& name, float x, float y)
{
    int loc = GetUniformLocation(name);
    if (loc != -1) glUniform2f(loc, x, y);
}

void Shader::SetVec3(const std::string& name, float x, float y, float z)
{
    int loc = GetUniformLocation(name);
    if (loc != -1) glUniform3f(loc, x, y, z);
}

void Shader::SetVec4(const std::string& name, float x, float y, float z, float w)
{
    int loc = GetUniformLocation(name);
    if (loc != -1) glUniform4f(loc, x, y, z, w);
}

void Shader::SetMat3(const std::string& name, const float* m)
{
    int loc = GetUniformLocation(name);
    if (loc != -1) glUniformMatrix3fv(loc, 1, GL_FALSE, m);
}

void Shader::SetMat4(const std::string& name, const float* m)
{
    int loc = GetUniformLocation(name);
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, m);
}

} // namespace veex