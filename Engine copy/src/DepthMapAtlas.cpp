#include "veex/DepthMapAtlas.h"
#include "veex/DepthMapGenerator.h"
#include "veex/Logger.h"
#include "veex/GLHeaders.h"

namespace veex {

DepthMapAtlas::DepthMapAtlas() {}

DepthMapAtlas::~DepthMapAtlas() {
    Shutdown();
}

bool DepthMapAtlas::Initialize(const GameInfo& gameInfo, int atlasSize) {
    if (m_initialized) {
        return true;
    }

    MegaTexture::Config config;
    config.atlasWidth = atlasSize;
    config.atlasHeight = atlasSize;
    config.maxAtlases = 2;
    config.padding = 2;
    config.generateMipmaps = true;
    config.useCompression = false; // RGBA8 for height maps (single channel stored in RGB)
    config.enableCaching = true;
    config.verboseLogging = false;

    if (!m_megaTexture.Initialize(config, gameInfo)) {
        Logger::Error("DepthMapAtlas: Failed to initialize MegaTexture");
        return false;
    }

    m_initialized = true;
    Logger::Info("DepthMapAtlas: Initialized " + std::to_string(atlasSize) + "x" + std::to_string(atlasSize));
    return true;
}

void DepthMapAtlas::Shutdown() {
    m_megaTexture.Shutdown();
    m_initialized = false;
}

bool DepthMapAtlas::GenerateAndPack(const std::string& materialName,
                                     const uint8_t* normalData, int normalW, int normalH,
                                     const uint8_t* diffuseData,
                                     int diffuseW, int diffuseH) {
    if (!m_initialized) {
        Logger::Error("DepthMapAtlas::GenerateAndPack called on uninitialized atlas");
        return false;
    }

    if (m_megaTexture.FindTexture(materialName)) {
        // Already packed
        return true;
    }

    if (!normalData || normalW <= 0 || normalH <= 0) {
        return false;
    }

    // Generate the depth map
    std::vector<uint8_t> heightData;
    bool generated = false;

    if (diffuseData && diffuseW > 0 && diffuseH > 0) {
        // If diffuse dimensions differ from normal, we can't easily blend pixel-for-pixel.
        // For simplicity, only use diffuse if sizes match.
        if (diffuseW == normalW && diffuseH == normalH) {
            generated = DepthMapGenerator::Generate(normalData, diffuseData, normalW, normalH, heightData);
        } else {
            Logger::Warn("DepthMapAtlas: Diffuse size (" + std::to_string(diffuseW) + "x" + std::to_string(diffuseH) +
                         ") differs from normal size (" + std::to_string(normalW) + "x" + std::to_string(normalH) +
                         ") for " + materialName + ", using normal-only generation");
            generated = DepthMapGenerator::Generate(normalData, nullptr, normalW, normalH, heightData);
        }
    } else {
        generated = DepthMapGenerator::Generate(normalData, nullptr, normalW, normalH, heightData);
    }

    if (!generated || heightData.empty()) {
        Logger::Warn("DepthMapAtlas: Failed to generate depth map for " + materialName);
        return false;
    }

    // Pack into atlas
    const MegaTexture::TextureSlot* slot = m_megaTexture.PackTexture(materialName, heightData.data(), normalW, normalH);
    if (!slot) {
        Logger::Warn("DepthMapAtlas: Failed to pack depth map for " + materialName);
        return false;
    }

    Logger::Info("DepthMapAtlas: Generated and packed depth map for " + materialName +
                 " (" + std::to_string(normalW) + "x" + std::to_string(normalH) + ")");
    return true;
}

bool DepthMapAtlas::PackDepthMap(const std::string& materialName,
                                  const uint8_t* data, int width, int height) {
    if (!m_initialized) {
        return false;
    }

    if (m_megaTexture.FindTexture(materialName)) {
        return true;
    }

    if (!data || width <= 0 || height <= 0) {
        return false;
    }

    const MegaTexture::TextureSlot* slot = m_megaTexture.PackTexture(materialName, data, width, height);
    return slot != nullptr;
}

const MegaTexture::TextureSlot* DepthMapAtlas::FindDepthMap(const std::string& materialName) const {
    return m_megaTexture.FindTexture(materialName);
}

glm::vec4 DepthMapAtlas::GetUVCrop(const std::string& materialName) const {
    return m_megaTexture.GetUVCrop(materialName);
}

GLuint DepthMapAtlas::GetAtlasTextureID() const {
    return m_megaTexture.GetAtlasTextureID(0);
}

void DepthMapAtlas::Bind(int textureUnit) const {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, GetAtlasTextureID());
}

bool DepthMapAtlas::IsReady() const {
    return m_initialized && m_megaTexture.IsReady();
}

MegaTexture::Stats DepthMapAtlas::GetStats() const {
    return m_megaTexture.GetStats();
}

} // namespace veex
