#include "veex/MegaTextureCache.h"
#include "veex/Logger.h"
#include "veex/FileSystem.h"
#include "veex/GameInfo.h"
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace veex {

// CRC32 table
static uint32_t g_crc32Table[256] = {0};
static bool g_crc32TableInitialized = false;

static void InitCRC32Table() {
    if (g_crc32TableInitialized) return;
    
    const uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
        g_crc32Table[i] = crc;
    }
    g_crc32TableInitialized = true;
}

static uint32_t CalculateCRC32Static(const uint8_t* data, size_t size) {
    InitCRC32Table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        uint8_t index = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ g_crc32Table[index];
    }
    return crc ^ 0xFFFFFFFF;
}

std::string MegaTextureCache::GetCacheDirectory(const GameInfo& gameInfo) {
    // Cache files are stored in regular filesystem directories, NOT in VPKs
    // (VPKs are read-only archives)
    //
    // Use the first writable search path from GameInfo
    // This is typically "Game" directory or similar
    
    for (const auto& searchPath : gameInfo.searchPaths) {
        // Skip VPK files - they're read-only
        if (searchPath.isVPK) {
            continue;
        }
        
        // Found a directory - this should be writable
        if (searchPath.isDirectory) {
            std::string cacheDir = searchPath.path + "/maps";
            return cacheDir;
        }
    }
    
    // Fallback to gameDir if no search paths worked
    Logger::Warn("MegaTextureCache: No writable search paths found, using enginePath");
    return gameInfo.enginePath + "/maps";
}

uint32_t MegaTextureCache::CalculateCRC32(const uint8_t* data, size_t size) {
    return CalculateCRC32Static(data, size);
}

bool MegaTextureCache::HasValidCache(const std::string& mapName, const GameInfo& gameInfo) {
    std::string cacheDir = GetCacheDirectory(gameInfo);
    std::string indexPath = cacheDir + "/" + mapName + ".mtexi";
    std::string cachePath = cacheDir + "/" + mapName + ".megatex";
    
    // Create directory if it doesn't exist
    if (!std::filesystem::exists(cacheDir)) {
        try {
            std::filesystem::create_directories(cacheDir);
            Logger::Info("MegaTextureCache: Created cache directory: " + cacheDir);
        } catch (const std::exception& e) {
            Logger::Error("MegaTextureCache: Failed to create cache directory: " + std::string(e.what()));
            return false;
        }
    }
    
    // Check if both files exist
    if (!std::filesystem::exists(indexPath) || !std::filesystem::exists(cachePath)) {
        Logger::Info("MegaTextureCache: No cache found for " + mapName);
        return false;
    }
    
    Logger::Info("MegaTextureCache: Cache files exist for " + mapName);
    Logger::Info("  Index: " + indexPath);
    Logger::Info("  Data: " + cachePath);
    return true;
}

bool MegaTextureCache::LoadCache(const std::string& mapName, const GameInfo& gameInfo) {
    // This is a placeholder - the actual loading would restore atlas data
    // For now, we just validate the cache exists
    return HasValidCache(mapName, gameInfo);
}

bool MegaTextureCache::BuildAndSaveCache(const std::string& mapName,
                                        int atlasWidth, int atlasHeight,
                                        const std::vector<TextureInfo>& textures,
                                        const std::vector<uint8_t>& compressedData,
                                        const GameInfo& gameInfo) {
    // Get cache directory from GameInfo
    std::string cacheDir = GetCacheDirectory(gameInfo);
    
    // Ensure directory exists
    if (!std::filesystem::exists(cacheDir)) {
        try {
            std::filesystem::create_directories(cacheDir);
            Logger::Info("MegaTextureCache: Created cache directory: " + cacheDir);
        } catch (const std::exception& e) {
            Logger::Error("MegaTextureCache: Failed to create cache directory: " + std::string(e.what()));
            return false;
        }
    }
    
    std::string indexPath = cacheDir + "/" + mapName + ".mtexi";
    std::string cachePath = cacheDir + "/" + mapName + ".megatex";
    
    Logger::Info("MegaTextureCache: Saving cache for " + mapName);
    Logger::Info("  Atlas size: " + std::to_string(atlasWidth) + "x" + std::to_string(atlasHeight));
    Logger::Info("  Textures: " + std::to_string(textures.size()));
    Logger::Info("  Compressed data: " + std::to_string(compressedData.size()) + " bytes");
    Logger::Info("  Cache directory: " + cacheDir);
    
    // Calculate overall checksum of compressed data
    uint32_t totalChecksum = CalculateCRC32Static(compressedData.data(), compressedData.size());
    
    // Save index file (.mtexi)
    std::vector<char> indexBuffer;
    indexBuffer.reserve(sizeof(Header) + textures.size() * 256);
    
    // Write header to buffer
    Header header;
    header.magic = MAGIC;
    header.version = VERSION;
    header.atlasWidth = atlasWidth;
    header.atlasHeight = atlasHeight;
    header.textureCount = textures.size();
    header.dataSize = compressedData.size();
    header.checksum = totalChecksum;
    
    const char* headerBytes = reinterpret_cast<const char*>(&header);
    indexBuffer.insert(indexBuffer.end(), headerBytes, headerBytes + sizeof(header));
    
    // Write texture entries to buffer
    for (const auto& tex : textures) {
        uint32_t nameLen = tex.name.length();
        const char* lenBytes = reinterpret_cast<const char*>(&nameLen);
        indexBuffer.insert(indexBuffer.end(), lenBytes, lenBytes + sizeof(nameLen));
        indexBuffer.insert(indexBuffer.end(), tex.name.c_str(), tex.name.c_str() + nameLen);
        
        // Write UV crop
        float u = tex.uvCrop.x;
        float v = tex.uvCrop.y;
        float uw = tex.uvCrop.z;
        float vh = tex.uvCrop.w;
        
        const char* uvBytes = reinterpret_cast<const char*>(&u);
        indexBuffer.insert(indexBuffer.end(), uvBytes, uvBytes + sizeof(u));
        uvBytes = reinterpret_cast<const char*>(&v);
        indexBuffer.insert(indexBuffer.end(), uvBytes, uvBytes + sizeof(v));
        uvBytes = reinterpret_cast<const char*>(&uw);
        indexBuffer.insert(indexBuffer.end(), uvBytes, uvBytes + sizeof(uw));
        uvBytes = reinterpret_cast<const char*>(&vh);
        indexBuffer.insert(indexBuffer.end(), uvBytes, uvBytes + sizeof(vh));
        
        // Write checksum
        uint32_t texChecksum = 0;
        const char* crcBytes = reinterpret_cast<const char*>(&texChecksum);
        indexBuffer.insert(indexBuffer.end(), crcBytes, crcBytes + sizeof(texChecksum));
    }
    
    // Write index file
    std::ofstream indexFile(indexPath, std::ios::binary);
    if (!indexFile.is_open()) {
        Logger::Error("MegaTextureCache: Failed to create index file: " + indexPath);
        return false;
    }
    
    indexFile.write(indexBuffer.data(), indexBuffer.size());
    indexFile.close();
    
    if (indexFile.fail()) {
        Logger::Error("MegaTextureCache: Failed to write index file");
        return false;
    }
    
    Logger::Info("MegaTextureCache: Saved index file (" + std::to_string(indexBuffer.size()) + " bytes)");
    
    // Save compressed data file (.megatex)
    std::ofstream cacheFile(cachePath, std::ios::binary);
    if (!cacheFile.is_open()) {
        Logger::Error("MegaTextureCache: Failed to create cache file: " + cachePath);
        return false;
    }
    
    cacheFile.write(reinterpret_cast<const char*>(compressedData.data()), compressedData.size());
    cacheFile.close();
    
    if (cacheFile.fail()) {
        Logger::Error("MegaTextureCache: Failed to write cache file");
        return false;
    }
    
    Logger::Info("MegaTextureCache: Saved compressed data file (" + 
                 std::to_string(compressedData.size()) + " bytes)");
    
    return true;
}

} // namespace veex
