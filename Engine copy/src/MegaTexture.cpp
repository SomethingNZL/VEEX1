#include "veex/MegaTexture.h"
#include "veex/Logger.h"
#include "veex/GameInfo.h"
#include "veex/GLHeaders.h"
#include <algorithm>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <limits>

namespace veex {

static uint32_t g_crc32Table[256] = {0};
static bool g_crc32Initialized = false;

static void InitCRC32Table() {
    if (g_crc32Initialized) return;
    const uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1) ? ((crc >> 1) ^ polynomial) : (crc >> 1);
        }
        g_crc32Table[i] = crc;
    }
    g_crc32Initialized = true;
}

static uint32_t CalculateCRC32(const uint8_t* data, size_t size) {
    InitCRC32Table();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        uint8_t index = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ g_crc32Table[index];
    }
    return crc ^ 0xFFFFFFFF;
}

MegaTexture::MegaTexture() {}
MegaTexture::~MegaTexture() { Shutdown(); }

bool MegaTexture::Initialize(const Config& config, const GameInfo& gameInfo) {
    if (m_status != Status::UNINITIALIZED) return true;
    
    m_status = Status::INITIALIZING;
    m_config = config;
    m_gameInfo = &gameInfo;
    
    Logger::Info("MegaTexture: Initializing (" + std::to_string(config.atlasWidth) + "x" + std::to_string(config.atlasHeight) + ")");
    
    m_cacheDirectory = MegaTextureCache::GetCacheDirectory(gameInfo);
    
    if (!std::filesystem::exists(m_cacheDirectory)) {
        try { std::filesystem::create_directories(m_cacheDirectory); }
        catch (const std::exception& e) { Logger::Error("Cache dir failed: " + std::string(e.what())); }
    }
    
    GLenum atlasFormat = TextureAtlas::IsDXTSupported() && config.useCompression ? 
        GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : GL_RGBA8;
    
    Atlas firstAtlas;
    if (!CreateAtlas(firstAtlas, config.atlasWidth, config.atlasHeight, atlasFormat)) {
        m_status = Status::ERROR;
        return false;
    }
    
    m_atlases.push_back(firstAtlas);
    m_status = Status::READY;
    Logger::Info("MegaTexture: Ready");
    return true;
}

void MegaTexture::Shutdown() {
    for (auto& atlas : m_atlases) DestroyAtlas(atlas);
    m_atlases.clear();
    m_textureSlots.clear();
    m_textureOrder.clear();
    m_status = Status::UNINITIALIZED;
}

bool MegaTexture::CreateAtlas(Atlas& atlas, int width, int height, GLenum format) {
    glGenTextures(1, &atlas.textureID);
    if (!atlas.textureID) return false;
    
    atlas.width = width;
    atlas.height = height;
    atlas.internalFormat = format;
    atlas.usedMemory = 0;
    atlas.textureCount = 0;
    atlas.skyline.resize(width, 0);
    
    glBindTexture(GL_TEXTURE_2D, atlas.textureID);
    
    bool isCompressed = (format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT || format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
    
    if (isCompressed) {
        size_t blockSize = (format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) ? 16 : 8;
        size_t totalSize = ((width + 3) / 4) * ((height + 3) / 4) * blockSize;
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, totalSize, nullptr);
    } else {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
    
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        glDeleteTextures(1, &atlas.textureID);
        atlas.textureID = 0;
        glBindTexture(GL_TEXTURE_2D, 0);
        return false;
    }
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return true;
}

void MegaTexture::DestroyAtlas(Atlas& atlas) {
    if (atlas.textureID) glDeleteTextures(1, &atlas.textureID);
    atlas.textureID = 0;
    atlas.skyline.clear();
}

const MegaTexture::TextureSlot* MegaTexture::PackTexture(const std::string& name, const uint8_t* data, int width, int height) {
    if (m_status != Status::READY || !data || width <= 0 || height <= 0) return nullptr;
    if (m_textureSlots.find(name) != m_textureSlots.end()) return &m_textureSlots[name];
    
    int alignedW = ((width + 3) / 4) * 4;
    int alignedH = ((height + 3) / 4) * 4;
    int reqW = alignedW + m_config.padding * 2;
    int reqH = alignedH + m_config.padding * 2;
    
    int x, y, atlasIdx;
    if (!FindSlotSkyline(reqW, reqH, m_atlases[m_currentAtlasIndex], x, y, atlasIdx)) {
        if ((int)m_atlases.size() < m_config.maxAtlases) {
            Atlas newAtlas;
            if (CreateAtlas(newAtlas, m_config.atlasWidth, m_config.atlasHeight, m_atlases[0].internalFormat)) {
                m_atlases.push_back(newAtlas);
                m_currentAtlasIndex = (int)m_atlases.size() - 1;
                atlasIdx = m_currentAtlasIndex;
                if (!FindSlotSkyline(reqW, reqH, m_atlases[atlasIdx], x, y, atlasIdx)) {
                    m_stats.failedCount++;
                    return nullptr;
                }
            } else { m_stats.failedCount++; return nullptr; }
        } else { m_stats.failedCount++; return nullptr; }
    }
    
    Atlas& targetAtlas = m_atlases[atlasIdx];
    
    std::vector<uint8_t> paddedData;
    const uint8_t* uploadData = data;
    size_t dataSize = width * height * 4;
    
    if (alignedW != width || alignedH != height) {
        paddedData.resize(alignedW * alignedH * 4, 0);
        for (int py = 0; py < height && py < alignedH; py++) {
            for (int px = 0; px < width && px < alignedW; px++) {
                std::memcpy(&paddedData[(py * alignedW + px) * 4], &data[(py * width + px) * 4], 4);
            }
        }
        uploadData = paddedData.data();
        dataSize = alignedW * alignedH * 4;
    }
    
    if (!UploadToAtlas(targetAtlas, x + m_config.padding, y + m_config.padding, alignedW, alignedH, uploadData, dataSize, GL_RGBA)) {
        m_stats.failedCount++;
        return nullptr;
    }
    
    for (int sx = x; sx < x + reqW && sx < targetAtlas.width; sx++) {
        targetAtlas.skyline[sx] = y + reqH;
    }
    
    TextureSlot slot;
    slot.atlasIndex = atlasIdx;
    slot.x = x + m_config.padding;
    slot.y = y + m_config.padding;
    slot.width = alignedW;
    slot.height = alignedH;
    slot.inputWidth = width;
    slot.inputHeight = height;
    slot.allocationID = m_nextSlotID++;
    slot.name = name;
    slot.checksum = CalculateCRC32(data, width * height * 4);
    
    m_textureSlots[name] = slot;
    m_textureOrder.push_back(name);
    targetAtlas.textureCount++;
    targetAtlas.usedMemory += alignedW * alignedH * 4;
    m_stats.textureCount++;
    
    return &m_textureSlots[name];
}

const MegaTexture::TextureSlot* MegaTexture::PackCompressedTexture(const std::string& name, const uint8_t* compressedData, int width, int height, GLenum format) {
    if (m_status != Status::READY || !compressedData || width <= 0 || height <= 0) return nullptr;
    if (m_textureSlots.find(name) != m_textureSlots.end()) return &m_textureSlots[name];
    
    int alignedW = ((width + 3) / 4) * 4;
    int alignedH = ((height + 3) / 4) * 4;
    int reqW = alignedW + m_config.padding * 2;
    int reqH = alignedH + m_config.padding * 2;
    
    int x, y, atlasIdx;
    if (!FindSlotSkyline(reqW, reqH, m_atlases[m_currentAtlasIndex], x, y, atlasIdx)) {
        if ((int)m_atlases.size() < m_config.maxAtlases) {
            Atlas newAtlas;
            if (CreateAtlas(newAtlas, m_config.atlasWidth, m_config.atlasHeight, m_atlases[0].internalFormat)) {
                m_atlases.push_back(newAtlas);
                m_currentAtlasIndex = (int)m_atlases.size() - 1;
                atlasIdx = m_currentAtlasIndex;
                if (!FindSlotSkyline(reqW, reqH, m_atlases[atlasIdx], x, y, atlasIdx)) {
                    m_stats.failedCount++;
                    return nullptr;
                }
            } else { m_stats.failedCount++; return nullptr; }
        } else { m_stats.failedCount++; return nullptr; }
    }
    
    Atlas& targetAtlas = m_atlases[atlasIdx];
    size_t blockSize = (format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) ? 16 : 8;
    size_t dataSize = ((alignedW + 3) / 4) * ((alignedH + 3) / 4) * blockSize;
    
    if (!UploadCompressedToAtlas(targetAtlas, x + m_config.padding, y + m_config.padding, alignedW, alignedH, compressedData, dataSize, format)) {
        m_stats.failedCount++;
        return nullptr;
    }
    
    for (int sx = x; sx < x + reqW && sx < targetAtlas.width; sx++) {
        targetAtlas.skyline[sx] = y + reqH;
    }
    
    TextureSlot slot;
    slot.atlasIndex = atlasIdx;
    slot.x = x + m_config.padding;
    slot.y = y + m_config.padding;
    slot.width = alignedW;
    slot.height = alignedH;
    slot.inputWidth = width;
    slot.inputHeight = height;
    slot.allocationID = m_nextSlotID++;
    slot.name = name;
    slot.checksum = CalculateCRC32(compressedData, dataSize);
    
    m_textureSlots[name] = slot;
    m_textureOrder.push_back(name);
    targetAtlas.textureCount++;
    targetAtlas.usedMemory += dataSize;
    m_stats.textureCount++;
    
    return &m_textureSlots[name];
}

bool MegaTexture::FindSlotSkyline(int width, int height, Atlas& atlas, int& outX, int& outY, int& outAtlasIndex) {
    int bestX = -1, bestY = std::numeric_limits<int>::max();
    
    for (int x = 0; x <= atlas.width - width; x++) {
        int maxY = 0;
        for (int sx = x; sx < x + width && sx < (int)atlas.skyline.size(); sx++) {
            maxY = std::max(maxY, atlas.skyline[sx]);
        }
        if (maxY + height <= atlas.height && maxY < bestY) {
            bestX = x;
            bestY = maxY;
        }
    }
    
    if (bestX < 0) return false;
    outX = bestX;
    outY = bestY;
    outAtlasIndex = 0;
    return true;
}

bool MegaTexture::UploadToAtlas(const Atlas& atlas, int x, int y, int width, int height, const uint8_t* data, size_t dataSize, GLenum uploadFormat) {
    glBindTexture(GL_TEXTURE_2D, atlas.textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, uploadFormat, GL_UNSIGNED_BYTE, data);
    GLenum err = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (err != GL_NO_ERROR) { Logger::Error("Upload failed: " + std::to_string(err)); return false; }
    return true;
}

bool MegaTexture::UploadCompressedToAtlas(const Atlas& atlas, int x, int y, int width, int height, const uint8_t* data, size_t dataSize, GLenum uploadFormat) {
    glBindTexture(GL_TEXTURE_2D, atlas.textureID);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, uploadFormat, dataSize, data);
    GLenum err = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (err != GL_NO_ERROR) { Logger::Error("Compressed upload failed: " + std::to_string(err)); return false; }
    return true;
}

int MegaTexture::PackTextures(const std::vector<TextureInfo>& textures) {
    int packed = 0;
    for (const auto& tex : textures) {
        const TextureSlot* slot = nullptr;
        if (tex.isCompressed && tex.format != 0) {
            slot = PackCompressedTexture(tex.name, tex.data.data(), tex.width, tex.height, tex.format);
        } else if (!tex.data.empty()) {
            slot = PackTexture(tex.name, tex.data.data(), tex.width, tex.height);
        }
        if (slot) packed++;
    }
    return packed;
}

const MegaTexture::TextureSlot* MegaTexture::FindTexture(const std::string& name) const {
    auto it = m_textureSlots.find(name);
    return (it != m_textureSlots.end()) ? &it->second : nullptr;
}

glm::vec4 MegaTexture::GetUVCrop(const std::string& name) const {
    const TextureSlot* slot = FindTexture(name);
    if (!slot) return glm::vec4(0.0f);
    const Atlas& atlas = m_atlases[slot->atlasIndex];
    return glm::vec4(
        static_cast<float>(slot->x) / static_cast<float>(atlas.width),
        static_cast<float>(slot->y) / static_cast<float>(atlas.height),
        static_cast<float>(slot->inputWidth) / static_cast<float>(atlas.width),
        static_cast<float>(slot->inputHeight) / static_cast<float>(atlas.height)
    );
}

GLuint MegaTexture::GetAtlasTextureID(int atlasIndex) const {
    if (atlasIndex < 0 || atlasIndex >= (int)m_atlases.size()) return 0;
    return m_atlases[atlasIndex].textureID;
}

void MegaTexture::BindAtlases(int baseUnit, GLuint shaderID) const {
    for (size_t i = 0; i < m_atlases.size(); i++) {
        glActiveTexture(GL_TEXTURE0 + baseUnit + (GLenum)i);
        glBindTexture(GL_TEXTURE_2D, m_atlases[i].textureID);
    }
    (void)shaderID;
}

bool MegaTexture::LoadFromCache(const std::string& mapName) {
    std::vector<MegaTextureCache::TextureEntry> entries;
    std::vector<uint8_t> atlasData;
    MegaTextureCache::Header header;
    
    if (!MegaTextureCache::LoadCache(mapName, *m_gameInfo, entries, atlasData, header)) {
        return false;
    }
    
    Logger::Info("MegaTexture: Loaded cache with " + std::to_string(entries.size()) + " textures");
    return true;
}

bool MegaTexture::SaveToCache(const std::string& mapName) const {
    std::vector<MegaTextureCache::TextureEntry> entries;
    entries.reserve(m_textureSlots.size());
    
    for (const auto& [name, slot] : m_textureSlots) {
        MegaTextureCache::TextureEntry entry;
        entry.name = slot.name;
        entry.atlasIndex = slot.atlasIndex;
        entry.inputWidth = slot.inputWidth;
        entry.inputHeight = slot.inputHeight;
        entry.checksum = slot.checksum;
        
        const Atlas& atlas = m_atlases[slot.atlasIndex];
        entry.uOffset = static_cast<float>(slot.x) / static_cast<float>(atlas.width);
        entry.vOffset = static_cast<float>(slot.y) / static_cast<float>(atlas.height);
        entry.uScale = static_cast<float>(slot.inputWidth) / static_cast<float>(atlas.width);
        entry.vScale = static_cast<float>(slot.inputHeight) / static_cast<float>(atlas.height);
        
        entries.push_back(entry);
    }
    
    // Extract atlas data from first atlas
    Atlas& firstAtlas = const_cast<Atlas&>(m_atlases[0]);
    size_t dataSize = firstAtlas.width * firstAtlas.height * 4;
    std::vector<uint8_t> atlasData(dataSize);
    
    glBindTexture(GL_TEXTURE_2D, firstAtlas.textureID);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlasData.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    
    MegaTextureCache::Header header;
    header.atlasWidth = firstAtlas.width;
    header.atlasHeight = firstAtlas.height;
    header.textureCount = entries.size();
    header.dataSize = atlasData.size();
    header.atlasCount = m_atlases.size();
    
    if (MegaTextureCache::SaveCache(mapName, *m_gameInfo, entries, atlasData, header)) {
        Logger::Info("MegaTexture: Saved cache with " + std::to_string(entries.size()) + " textures");
        return true;
    }
    return false;
}

bool MegaTexture::HasCache(const std::string& mapName) const {
    return MegaTextureCache::HasValidCache(mapName, *m_gameInfo);
}

MegaTexture::Stats MegaTexture::GetStats() const {
    Stats stats;
    stats.atlasCount = (int)m_atlases.size();
    stats.textureCount = (int)m_textureSlots.size();
    stats.failedCount = m_stats.failedCount;
    stats.usedMemory = 0;
    stats.totalMemory = 0;
    
    for (const auto& atlas : m_atlases) {
        stats.usedMemory += atlas.usedMemory;
        if (atlas.internalFormat == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
            stats.totalMemory += ((atlas.width + 3) / 4) * ((atlas.height + 3) / 4) * 16;
        } else {
            stats.totalMemory += atlas.width * atlas.height * 4;
        }
    }
    
    if (stats.totalMemory > 0) {
        stats.usagePercent = (static_cast<float>(stats.usedMemory) / stats.totalMemory) * 100.0f;
    }
    
    return stats;
}

bool MegaTexture::ValidateGLState(const char* operation) const {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        Logger::Error("MegaTexture::" + std::string(operation) + " GL error: " + std::to_string(err));
        m_lastGLError = err;
        return false;
    }
    m_lastGLError = GL_NO_ERROR;
    return true;
}

void MegaTexture::GenerateMipmaps() {
    for (auto& atlas : m_atlases) {
        if (atlas.textureID) {
            glBindTexture(GL_TEXTURE_2D, atlas.textureID);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    ValidateGLState("GenerateMipmaps");
}

void MegaTexture::Clear() {
    for (auto& atlas : m_atlases) {
        atlas.skyline.clear();
        atlas.skyline.resize(atlas.width, 0);
        atlas.usedMemory = 0;
        atlas.textureCount = 0;
    }
    m_textureSlots.clear();
    m_textureOrder.clear();
    m_stats = Stats();
    Logger::Info("MegaTexture: Cleared all textures");
}

uint32_t MegaTexture::CalculateChecksum(const uint8_t* data, size_t size) {
    return CalculateCRC32(data, size);
}

// ─── MegaTextureCache Implementation ─────────────────────────────────────────

std::string MegaTextureCache::GetCacheDirectory(const GameInfo& gameInfo) {
    for (const auto& searchPath : gameInfo.searchPaths) {
        if (searchPath.isVPK) continue;
        if (searchPath.isDirectory) {
            return searchPath.path + "/maps";
        }
    }
    return gameInfo.enginePath + "/maps";
}

bool MegaTextureCache::HasValidCache(const std::string& mapName, const GameInfo& gameInfo) {
    std::string cacheDir = GetCacheDirectory(gameInfo);
    std::string indexPath = cacheDir + "/" + mapName + ".mtexi";
    std::string cachePath = cacheDir + "/" + mapName + ".megatex";
    
    if (!std::filesystem::exists(cacheDir)) {
        try { std::filesystem::create_directories(cacheDir); }
        catch (...) { return false; }
    }
    
    return std::filesystem::exists(indexPath) && std::filesystem::exists(cachePath);
}

bool MegaTextureCache::LoadCache(const std::string& mapName, const GameInfo& gameInfo,
                                  std::vector<TextureEntry>& outEntries,
                                  std::vector<uint8_t>& outAtlasData,
                                  Header& outHeader) {
    std::string cacheDir = GetCacheDirectory(gameInfo);
    std::string indexPath = cacheDir + "/" + mapName + ".mtexi";
    std::string cachePath = cacheDir + "/" + mapName + ".megatex";
    
    if (!std::filesystem::exists(indexPath)) return false;
    
    std::ifstream indexFile(indexPath, std::ios::binary);
    if (!indexFile.is_open()) return false;
    
    // Read header
    indexFile.read(reinterpret_cast<char*>(&outHeader), sizeof(Header));
    
    if (outHeader.magic != MAGIC || outHeader.version != VERSION) {
        Logger::Warn("MegaTextureCache: Invalid cache format");
        return false;
    }
    
    // Read texture entries
    outEntries.resize(outHeader.textureCount);
    for (uint32_t i = 0; i < outHeader.textureCount; i++) {
        // Read name length and name
        uint32_t nameLen = 0;
        indexFile.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        
        std::string name(nameLen, '\0');
        indexFile.read(&name[0], nameLen);
        outEntries[i].name = name;
        
        // Read UV coordinates
        indexFile.read(reinterpret_cast<char*>(&outEntries[i].uOffset), sizeof(float));
        indexFile.read(reinterpret_cast<char*>(&outEntries[i].vOffset), sizeof(float));
        indexFile.read(reinterpret_cast<char*>(&outEntries[i].uScale), sizeof(float));
        indexFile.read(reinterpret_cast<char*>(&outEntries[i].vScale), sizeof(float));
        
        // Read dimensions and checksum
        indexFile.read(reinterpret_cast<char*>(&outEntries[i].inputWidth), sizeof(int));
        indexFile.read(reinterpret_cast<char*>(&outEntries[i].inputHeight), sizeof(int));
        indexFile.read(reinterpret_cast<char*>(&outEntries[i].checksum), sizeof(uint32_t));
        indexFile.read(reinterpret_cast<char*>(&outEntries[i].atlasIndex), sizeof(int));
    }
    
    indexFile.close();
    
    // Read atlas data
    if (std::filesystem::exists(cachePath)) {
        std::ifstream cacheFile(cachePath, std::ios::binary | std::ios::ate);
        if (cacheFile.is_open()) {
            size_t dataSize = cacheFile.tellg();
            cacheFile.seekg(0, std::ios::beg);
            outAtlasData.resize(dataSize);
            cacheFile.read(reinterpret_cast<char*>(outAtlasData.data()), dataSize);
            cacheFile.close();
        }
    }
    
    Logger::Info("MegaTextureCache: Loaded " + std::to_string(outEntries.size()) + " textures from cache");
    return true;
}

bool MegaTextureCache::SaveCache(const std::string& mapName, const GameInfo& gameInfo,
                                  const std::vector<TextureEntry>& entries,
                                  const std::vector<uint8_t>& atlasData,
                                  const Header& header) {
    std::string cacheDir = GetCacheDirectory(gameInfo);
    
    if (!std::filesystem::exists(cacheDir)) {
        try { std::filesystem::create_directories(cacheDir); }
        catch (const std::exception& e) {
            Logger::Error("MegaTextureCache: Failed to create cache directory: " + std::string(e.what()));
            return false;
        }
    }
    
    std::string indexPath = cacheDir + "/" + mapName + ".mtexi";
    std::string cachePath = cacheDir + "/" + mapName + ".megatex";
    
    // Write index file
    std::ofstream indexFile(indexPath, std::ios::binary);
    if (!indexFile.is_open()) {
        Logger::Error("MegaTextureCache: Failed to create index file");
        return false;
    }
    
    indexFile.write(reinterpret_cast<const char*>(&header), sizeof(Header));
    
    for (const auto& entry : entries) {
        uint32_t nameLen = (uint32_t)entry.name.length();
        indexFile.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
        indexFile.write(entry.name.c_str(), nameLen);
        
        indexFile.write(reinterpret_cast<const char*>(&entry.uOffset), sizeof(float));
        indexFile.write(reinterpret_cast<const char*>(&entry.vOffset), sizeof(float));
        indexFile.write(reinterpret_cast<const char*>(&entry.uScale), sizeof(float));
        indexFile.write(reinterpret_cast<const char*>(&entry.vScale), sizeof(float));
        
        indexFile.write(reinterpret_cast<const char*>(&entry.inputWidth), sizeof(int));
        indexFile.write(reinterpret_cast<const char*>(&entry.inputHeight), sizeof(int));
        indexFile.write(reinterpret_cast<const char*>(&entry.checksum), sizeof(uint32_t));
        indexFile.write(reinterpret_cast<const char*>(&entry.atlasIndex), sizeof(int));
    }
    
    indexFile.close();
    
    // Write atlas data
    std::ofstream cacheFile(cachePath, std::ios::binary);
    if (!cacheFile.is_open()) {
        Logger::Error("MegaTextureCache: Failed to create cache file");
        return false;
    }
    
    cacheFile.write(reinterpret_cast<const char*>(atlasData.data()), atlasData.size());
    cacheFile.close();
    
    Logger::Info("MegaTextureCache: Saved cache (" + std::to_string(atlasData.size()) + " bytes)");
    return true;
}

uint32_t MegaTextureCache::CalculateCRC32(const uint8_t* data, size_t size) {
    InitCRC32Table();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        uint8_t index = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ g_crc32Table[index];
    }
    return crc ^ 0xFFFFFFFF;
}

} // namespace veex
