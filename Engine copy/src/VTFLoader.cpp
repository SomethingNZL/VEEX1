#include "veex/VTFLoader.h"
#include "veex/Logger.h"
#include "VTFParser.hpp"
#include "veex/GLHeaders.h"
#include <cstring>
#include <fstream>
#include <algorithm>
#include <vector>

// Map VTFParser ImageFormat to our VTFFormat
static veex::VTFFormat MapVTFParserFormat(VtfParser::ImageFormat fmt) {
    switch (fmt) {
        case VtfParser::ImageFormat::NONE: return veex::VTFFormat::NONE;
        case VtfParser::ImageFormat::RGBA8888: return veex::VTFFormat::RGBA8888;
        case VtfParser::ImageFormat::ABGR8888: return veex::VTFFormat::RGBA8888; // Treat as RGBA
        case VtfParser::ImageFormat::RGB888: return veex::VTFFormat::RGB888;
        case VtfParser::ImageFormat::BGR888: return veex::VTFFormat::BGR888;
        case VtfParser::ImageFormat::RGB565: return veex::VTFFormat::RGB565;
        case VtfParser::ImageFormat::I8: return veex::VTFFormat::I8;
        case VtfParser::ImageFormat::IA88: return veex::VTFFormat::IA88;
        case VtfParser::ImageFormat::RGB888_BLUESCREEN: return veex::VTFFormat::RGB888_BLUESCREEN;
        case VtfParser::ImageFormat::BGR888_BLUESCREEN: return veex::VTFFormat::BGR888_BLUESCREEN;
        case VtfParser::ImageFormat::ARGB8888: return veex::VTFFormat::ARGB8888;
        case VtfParser::ImageFormat::DXT1: return veex::VTFFormat::DXT1;
        case VtfParser::ImageFormat::DXT3: return veex::VTFFormat::DXT3;
        case VtfParser::ImageFormat::DXT5: return veex::VTFFormat::DXT5;
        case VtfParser::ImageFormat::BGRX8888: return veex::VTFFormat::BGRX8888;
        case VtfParser::ImageFormat::BGRA8888: return veex::VTFFormat::BGRA8888;
        case VtfParser::ImageFormat::BGRA5551: return veex::VTFFormat::BGRA5551;
        case VtfParser::ImageFormat::BGR565: return veex::VTFFormat::BGR565;
        case VtfParser::ImageFormat::BGRA4444: return veex::VTFFormat::BGRA4444;
        case VtfParser::ImageFormat::UV88: return veex::VTFFormat::IA88; // Similar to IA88
        case VtfParser::ImageFormat::UVWQ8888: return veex::VTFFormat::RGBA8888; // Similar to RGBA
        case VtfParser::ImageFormat::RGBA16161616F: return veex::VTFFormat::RGBA8888; // Convert to 8-bit
        case VtfParser::ImageFormat::RGBA16161616: return veex::VTFFormat::RGBA8888; // Convert to 8-bit
        case VtfParser::ImageFormat::UVLX8888: return veex::VTFFormat::RGBA8888; // Similar to RGBA
        default: return veex::VTFFormat::NONE;
    }
}

namespace veex {

// Forward declarations for DXT decompression helpers
static void DecompressDXT1(const uint8_t* compressedData, uint8_t* rgbaData, int width, int height);
static void DecompressDXT3(const uint8_t* compressedData, uint8_t* rgbaData, int width, int height);
static void DecompressDXT5(const uint8_t* compressedData, uint8_t* rgbaData, int width, int height);
static void DecompressATI2N(const uint8_t* compressedData, uint8_t* rgbaData, int width, int height);

// ─── VTFLoader Implementation ────────────────────────────────────────────────

VTFLoader::VTFLoader() {}

VTFLoader::~VTFLoader() {}

bool VTFLoader::LoadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        Logger::Error("VTFLoader: Cannot open file: " + path);
        return false;
    }
    
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        Logger::Error("VTFLoader: Failed to read file: " + path);
        return false;
    }
    
    return LoadFromMemory(buffer.data(), buffer.size());
}

bool VTFLoader::LoadFromMemory(const uint8_t* data, size_t size) {
    try {
        // Use VTFParser to parse the VTF file
        VtfParser::Vtf vtf(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), size));
        
        // Get image info
        auto extent = vtf.getHighResImageExtent();
        m_width = extent.width;
        m_height = extent.height;
        VTFFormat format = MapVTFParserFormat(vtf.getHighResImageFormat());
        m_format = static_cast<int32_t>(format);
        m_isCompressed = IsCompressedFormat(format);
        
        // Get mipmap count
        m_mipCount = vtf.getMipLevels();
        
        Logger::Info("VTFLoader: Loading " + std::to_string(m_width) + "x" + 
                    std::to_string(m_height) + " " + FormatToString(format) + 
                    " (using VTFParser, " + std::to_string(m_mipCount) + " mip levels)");
        
        // Get the high-res image data - extract all mipmap levels
        auto imageData = vtf.getHighResImageData();
        
        // Store all mipmap levels
        m_mipData.clear();
        m_mipData.resize(m_mipCount);
        
        // For ATI2N, we decompress to RGBA for compatibility with DXT5nm shader
        // So we treat it as uncompressed after decompression
        bool treatAsUncompressed = (format == VTFFormat::ATI2N);
        
        for (int mip = 0; mip < m_mipCount; mip++) {
            int mipWidth = std::max(1, m_width >> mip);
            int mipHeight = std::max(1, m_height >> mip);
            
            size_t sliceOffset = vtf.getImageSliceOffset(mip, 0, 0, 0); // mip, frame 0, face 0, depth 0
            const uint8_t* srcData = reinterpret_cast<const uint8_t*>(imageData.data()) + sliceOffset;
            
            // Calculate size of this mipmap level
            size_t sliceSize = CalculateImageSize(mipWidth, mipHeight, format);
            
            if (m_isCompressed && !treatAsUncompressed) {
                // Store compressed DXT data for this mip level
                m_mipData[mip].resize(sliceSize);
                std::memcpy(m_mipData[mip].data(), srcData, sliceSize);
            } else if (treatAsUncompressed) {
                // For ATI2N, decompress to RGBA for DXT5nm shader compatibility
                std::vector<uint8_t> rgbaData(mipWidth * mipHeight * 4);
                DecompressATI2N(srcData, rgbaData.data(), mipWidth, mipHeight);
                m_mipData[mip] = std::move(rgbaData);
            } else {
                // For uncompressed formats, convert to RGBA8888
                std::vector<uint8_t> rgbaData(mipWidth * mipHeight * 4);
                ConvertToRGBA(srcData, rgbaData.data(), mipWidth * mipHeight, format);
                m_mipData[mip] = std::move(rgbaData);
            }
        }
        
        // Set the base level data for backward compatibility
        if (m_mipCount > 0) {
            if (m_isCompressed && !treatAsUncompressed) {
                m_compressedData = m_mipData[0];
                m_rgbaData.clear();
            } else {
                m_rgbaData = m_mipData[0];
                m_compressedData.clear();
            }
        }
        
        m_loaded = true;
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("VTFLoader: Failed to parse VTF: " + std::string(e.what()));
        return false;
    }
}

bool VTFLoader::IsValidVTF(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    
    char sig[4];
    if (!file.read(sig, 4)) return false;
    
    return sig[0] == 'V' && sig[1] == 'T' && sig[2] == 'F' && sig[3] == '\0';
}

const char* VTFLoader::FormatToString(VTFFormat format) {
    switch (format) {
        case VTFFormat::NONE:                return "NONE";
        case VTFFormat::RGBA8888:            return "RGBA8888";
        case VTFFormat::RGB888:              return "RGB888";
        case VTFFormat::RGB565:              return "RGB565";
        case VTFFormat::I8:                  return "I8";
        case VTFFormat::IA88:                return "IA88";
        case VTFFormat::RGB888_BLUESCREEN:   return "RGB888_BLUESCREEN";
        case VTFFormat::BGR888_BLUESCREEN:   return "BGR888_BLUESCREEN";
        case VTFFormat::ARGB8888:            return "ARGB8888";
        case VTFFormat::ARGB8888_BLUESCREEN: return "ARGB8888_BLUESCREEN";
        case VTFFormat::RGB565_BLUESCREEN:   return "RGB565_BLUESCREEN";
        case VTFFormat::DXT1:                return "DXT1";
        case VTFFormat::DXT3:                return "DXT3";
        case VTFFormat::DXT5:                return "DXT5";
        case VTFFormat::BGR888:              return "BGR888";
        case VTFFormat::BGRX8888:            return "BGRX8888";
        case VTFFormat::BGRA8888:            return "BGRA8888";
        case VTFFormat::DXT1_ONEBITALPHA:    return "DXT1_ONEBITALPHA";
        case VTFFormat::BGRA5551:            return "BGRA5551";
        case VTFFormat::RGBX8888:            return "RGBX8888";
        case VTFFormat::BGR565:              return "BGR565";
        case VTFFormat::BGR555:              return "BGR555";
        case VTFFormat::BGRA4444:            return "BGRA4444";
        case VTFFormat::DXT5_ONEBITALPHA:    return "DXT5_ONEBITALPHA";
        case VTFFormat::ATI1N:               return "ATI1N";
        case VTFFormat::ATI2N:               return "ATI2N";
        default:                             return "UNKNOWN";
    }
}

bool VTFLoader::HasAlpha() const {
    switch (static_cast<VTFFormat>(m_format)) {
        case VTFFormat::DXT3:
        case VTFFormat::DXT5:
        case VTFFormat::DXT5_ONEBITALPHA:
        case VTFFormat::DXT1_ONEBITALPHA:
        case VTFFormat::RGBA8888:
        case VTFFormat::BGRA8888:
        case VTFFormat::ARGB8888:
        case VTFFormat::IA88:
        case VTFFormat::BGRA5551:
        case VTFFormat::BGRA4444:
        case VTFFormat::ATI1N:
        case VTFFormat::ATI2N:
            return true;
        default:
            return false;
    }
}

bool VTFLoader::IsCompressed() const {
    return m_isCompressed;
}

bool VTFLoader::IsCompressedFormat(VTFFormat format) const {
    switch (format) {
        case VTFFormat::DXT1:
        case VTFFormat::DXT3:
        case VTFFormat::DXT5:
        case VTFFormat::DXT1_ONEBITALPHA:
        case VTFFormat::DXT5_ONEBITALPHA:
        case VTFFormat::ATI1N:
        case VTFFormat::ATI2N:
            return true;
        default:
            return false;
    }
}

size_t VTFLoader::CalculateImageSize(int width, int height, VTFFormat format) const {
    switch (format) {
        case VTFFormat::DXT1:
        case VTFFormat::DXT1_ONEBITALPHA:
            return ((width + 3) / 4) * ((height + 3) / 4) * 8;
            
        case VTFFormat::DXT3:
        case VTFFormat::DXT5:
        case VTFFormat::DXT5_ONEBITALPHA:
        case VTFFormat::ATI1N:
        case VTFFormat::ATI2N:
            return ((width + 3) / 4) * ((height + 3) / 4) * 16;
            
        case VTFFormat::RGBA8888:
        case VTFFormat::BGRA8888:
        case VTFFormat::ARGB8888:
        case VTFFormat::BGRX8888:
        case VTFFormat::RGBX8888:
            return width * height * 4;
            
        case VTFFormat::RGB888:
        case VTFFormat::BGR888:
            return width * height * 3;
            
        case VTFFormat::RGB565:
        case VTFFormat::BGR565:
        case VTFFormat::BGRA5551:
        case VTFFormat::BGRA4444:
        case VTFFormat::BGR555:
            return width * height * 2;
            
        case VTFFormat::I8:
            return width * height;
            
        case VTFFormat::IA88:
            return width * height * 2;
            
        default:
            return 0;
    }
}

const uint8_t* VTFLoader::GetRGBAData(int* outWidth, int* outHeight) {
    if (!m_loaded) {
        if (outWidth) *outWidth = 0;
        if (outHeight) *outHeight = 0;
        return nullptr;
    }
    
    // If we already have RGBA data, return it
    if (!m_rgbaData.empty()) {
        if (outWidth) *outWidth = m_width;
        if (outHeight) *outHeight = m_height;
        return m_rgbaData.data();
    }
    
    // If we have compressed data, decompress to RGBA on demand
    if (m_isCompressed && !m_compressedData.empty()) {
        m_rgbaData.resize(m_width * m_height * 4);
        
        switch (static_cast<VTFFormat>(m_format)) {
            case VTFFormat::DXT1:
            case VTFFormat::DXT1_ONEBITALPHA:
                DecompressDXT1(m_compressedData.data(), m_rgbaData.data(), m_width, m_height);
                break;
            case VTFFormat::DXT3:
                DecompressDXT3(m_compressedData.data(), m_rgbaData.data(), m_width, m_height);
                break;
            case VTFFormat::DXT5:
            case VTFFormat::DXT5_ONEBITALPHA:
                DecompressDXT5(m_compressedData.data(), m_rgbaData.data(), m_width, m_height);
                break;
            default:
                Logger::Warn("VTFLoader: No software decompressor for compressed format " +
                             std::string(FormatToString(static_cast<VTFFormat>(m_format))));
                std::memset(m_rgbaData.data(), 0, m_rgbaData.size());
                break;
        }
        
        if (outWidth) *outWidth = m_width;
        if (outHeight) *outHeight = m_height;
        return m_rgbaData.data();
    }
    
    if (outWidth) *outWidth = m_width;
    if (outHeight) *outHeight = m_height;
    return m_rgbaData.data();
}

const uint8_t* VTFLoader::GetRawData(int* outWidth, int* outHeight, 
                                      VTFFormat* outFormat, size_t* outSize) const {
    if (!m_loaded) {
        if (outWidth) *outWidth = 0;
        if (outHeight) *outHeight = 0;
        if (outFormat) *outFormat = VTFFormat::NONE;
        if (outSize) *outSize = 0;
        return nullptr;
    }
    
    if (m_isCompressed) {
        // For compressed formats, return compressed data
        if (outWidth) *outWidth = m_width;
        if (outHeight) *outHeight = m_height;
        if (outFormat) *outFormat = static_cast<VTFFormat>(m_format);
        if (outSize) *outSize = m_compressedData.size();
        return m_compressedData.data();
    } else {
        // For uncompressed formats, return RGBA data
        if (outWidth) *outWidth = m_width;
        if (outHeight) *outHeight = m_height;
        if (outFormat) *outFormat = static_cast<VTFFormat>(m_format);
        if (outSize) *outSize = m_rgbaData.size();
        return m_rgbaData.data();
    }
}

const uint8_t* VTFLoader::GetCompressedData(int* outWidth, int* outHeight, 
                                             uint32_t* outGLInternalFormat, size_t* outDataSize) const {
    if (!m_loaded || !m_isCompressed || m_compressedData.empty()) {
        if (outWidth) *outWidth = 0;
        if (outHeight) *outHeight = 0;
        if (outGLInternalFormat) *outGLInternalFormat = 0;
        if (outDataSize) *outDataSize = 0;
        return nullptr;
    }
    
    if (outWidth) *outWidth = m_width;
    if (outHeight) *outHeight = m_height;
    if (outGLInternalFormat) {
        switch (static_cast<VTFFormat>(m_format)) {
            case VTFFormat::DXT1:
            case VTFFormat::DXT1_ONEBITALPHA:
                *outGLInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                break;
            case VTFFormat::DXT3:
                *outGLInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                break;
            case VTFFormat::DXT5:
            case VTFFormat::DXT5_ONEBITALPHA:
                *outGLInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                break;
            default:
                *outGLInternalFormat = 0;
                break;
        }
    }
    if (outDataSize) *outDataSize = m_compressedData.size();
    return m_compressedData.data();
}

size_t VTFLoader::CalculateBlockSize(int width, int height, VTFFormat format) const {
    switch (format) {
        case VTFFormat::DXT1:
        case VTFFormat::DXT3:
        case VTFFormat::DXT5:
        case VTFFormat::DXT1_ONEBITALPHA:
        case VTFFormat::DXT5_ONEBITALPHA:
        case VTFFormat::ATI1N:
        case VTFFormat::ATI2N:
            return 4;
        default:
            return 1;
    }
}

// ATI2N (3DC/2GC) Decompression
// ATI2N stores two channels (X and Y) of a normal map in a compressed format.
// The format is essentially two DXT5 alpha blocks per 4x4 pixel block.
// X channel is stored in the first 8 bytes, Y channel in the second 8 bytes.
// Z is computed from X and Y: Z = sqrt(1 - X*X - Y*Y)
static void DecompressATI2N(const uint8_t* compressedData, uint8_t* rgbaData, int width, int height) {
    int blocksX = (width + 3) / 4;
    int blocksY = (height + 3) / 4;
    
    for (int by = 0; by < blocksY; by++) {
        for (int bx = 0; bx < blocksX; bx++) {
            const uint8_t* block = compressedData + (by * blocksX + bx) * 16;
            
            // Decode X channel (first 8 bytes - like DXT5 alpha)
            uint8_t x0 = block[0];
            uint8_t x1 = block[1];
            uint64_t xIndices = 0;
            for (int i = 0; i < 6; i++) {
                xIndices |= (static_cast<uint64_t>(block[2 + i]) << (i * 8));
            }
            
            // Decode Y channel (second 8 bytes - like DXT5 alpha)
            uint8_t y0 = block[8];
            uint8_t y1 = block[9];
            uint64_t yIndices = 0;
            for (int i = 0; i < 6; i++) {
                yIndices |= (static_cast<uint64_t>(block[10 + i]) << (i * 8));
            }
            
            // Decode the 4x4 block
            for (int py = 0; py < 4; py++) {
                for (int px = 0; px < 4; px++) {
                    int pixelIndex = py * 4 + px;
                    int xIndex = (xIndices >> (pixelIndex * 3)) & 0x7;
                    int yIndex = (yIndices >> (pixelIndex * 3)) & 0x7;
                    
                    int x = bx * 4 + px;
                    int y = by * 4 + py;
                    
                    if (x < width && y < height) {
                        uint8_t* pixel = rgbaData + (y * width + x) * 4;
                        
                        // Decode X channel
                        uint8_t xVal;
                        if (xIndex == 0) {
                            xVal = x0;
                        } else if (xIndex == 1) {
                            xVal = x1;
                        } else if (x0 > x1) {
                            xVal = (uint8_t)(((8 - xIndex) * x0 + (xIndex - 1) * x1) / 7);
                        } else {
                            if (xIndex == 6) xVal = 0;
                            else if (xIndex == 7) xVal = 255;
                            else xVal = (uint8_t)(((6 - xIndex) * x0 + (xIndex - 1) * x1) / 5);
                        }
                        
                        // Decode Y channel
                        uint8_t yVal;
                        if (yIndex == 0) {
                            yVal = y0;
                        } else if (yIndex == 1) {
                            yVal = y1;
                        } else if (y0 > y1) {
                            yVal = (uint8_t)(((8 - yIndex) * y0 + (yIndex - 1) * y1) / 7);
                        } else {
                            if (yIndex == 6) yVal = 0;
                            else if (yIndex == 7) yVal = 255;
                            else yVal = (uint8_t)(((6 - yIndex) * y0 + (yIndex - 1) * y1) / 5);
                        }
                        
                        // ATI2N stores X and Y channels for normal maps.
                        // We need to convert to DXT5nm-compatible format for our shader:
                        //   DXT5nm: X in alpha, Y in green, Z computed in shader
                        //   RNM normal maps: X (tangent space U) and Y (tangent space V)
                        
                        // Store in DXT5nm-compatible layout:
                        //   R = 0 (unused)
                        //   G = Y channel (from ATI2N second block)
                        //   B = 0 (unused)
                        //   A = X channel (from ATI2N first block)
                        //
                        // The shader reads: normalTS.x = nMap.a * 2.0 - 1.0 (alpha = X)
                        //                   normalTS.y = nMap.g * 2.0 - 1.0 (green = Y)
                        //                   normalTS.z = sqrt(1 - x*x - y*y)
                        
                        pixel[0] = 0;     // R = unused
                        pixel[1] = yVal;  // G = Y channel
                        pixel[2] = 0;     // B = unused
                        pixel[3] = xVal;  // A = X channel
                    }
                }
            }
        }
    }
}

static void DecompressDXT1(const uint8_t* compressedData, uint8_t* rgbaData, int width, int height) {
    int blocksX = (width + 3) / 4;
    int blocksY = (height + 3) / 4;
    
    for (int by = 0; by < blocksY; by++) {
        for (int bx = 0; bx < blocksX; bx++) {
            const uint8_t* block = compressedData + (by * blocksX + bx) * 8;
            
            // Read the two 16-bit color values
            uint16_t color0 = block[0] | (block[1] << 8);
            uint16_t color1 = block[2] | (block[3] << 8);
            
            // Convert 565 to 8888
            uint8_t r0 = ((color0 >> 11) & 0x1F) * 255 / 31;
            uint8_t g0 = ((color0 >> 5) & 0x3F) * 255 / 63;
            uint8_t b0 = (color0 & 0x1F) * 255 / 31;
            
            uint8_t r1 = ((color1 >> 11) & 0x1F) * 255 / 31;
            uint8_t g1 = ((color1 >> 5) & 0x3F) * 255 / 63;
            uint8_t b1 = (color1 & 0x1F) * 255 / 31;
            
            // Read the 32-bit indices
            uint32_t indices = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);
            
            // Decode the 4x4 block
            for (int py = 0; py < 4; py++) {
                for (int px = 0; px < 4; px++) {
                    int pixelIndex = py * 4 + px;
                    int index = (indices >> (pixelIndex * 2)) & 0x3;
                    
                    int x = bx * 4 + px;
                    int y = by * 4 + py;
                    
                    if (x < width && y < height) {
                        uint8_t* pixel = rgbaData + (y * width + x) * 4;
                        
                        switch (index) {
                            case 0:
                                pixel[0] = r0; pixel[1] = g0; pixel[2] = b0; pixel[3] = 255;
                                break;
                            case 1:
                                pixel[0] = r1; pixel[1] = g1; pixel[2] = b1; pixel[3] = 255;
                                break;
                            case 2:
                                if (color0 > color1) {
                                    pixel[0] = (2 * r0 + r1) / 3;
                                    pixel[1] = (2 * g0 + g1) / 3;
                                    pixel[2] = (2 * b0 + b1) / 3;
                                    pixel[3] = 255;
                                } else {
                                    pixel[0] = (r0 + r1) / 2;
                                    pixel[1] = (g0 + g1) / 2;
                                    pixel[2] = (b0 + b1) / 2;
                                    pixel[3] = 255;
                                }
                                break;
                            case 3:
                                if (color0 > color1) {
                                    pixel[0] = (r0 + 2 * r1) / 3;
                                    pixel[1] = (g0 + 2 * g1) / 3;
                                    pixel[2] = (b0 + 2 * b1) / 3;
                                    pixel[3] = 255;
                                } else {
                                    pixel[0] = 0; pixel[1] = 0; pixel[2] = 0; pixel[3] = 0;
                                }
                                break;
                        }
                    }
                }
            }
        }
    }
}

static void DecompressDXT3(const uint8_t* compressedData, uint8_t* rgbaData, int width, int height) {
    int blocksX = (width + 3) / 4;
    int blocksY = (height + 3) / 4;
    
    for (int by = 0; by < blocksY; by++) {
        for (int bx = 0; bx < blocksX; bx++) {
            const uint8_t* block = compressedData + (by * blocksX + bx) * 16;
            
            // Decode explicit alpha (8 bytes, 4 bits per pixel)
            uint64_t alphaData = 0;
            for (int i = 0; i < 8; i++) {
                alphaData |= (static_cast<uint64_t>(block[i]) << (i * 8));
            }
            
            // Decode color channel (8 bytes) - same as DXT1
            const uint8_t* colorBlock = block + 8;
            uint16_t color0 = colorBlock[0] | (colorBlock[1] << 8);
            uint16_t color1 = colorBlock[2] | (colorBlock[3] << 8);
            
            uint8_t r0 = ((color0 >> 11) & 0x1F) * 255 / 31;
            uint8_t g0 = ((color0 >> 5) & 0x3F) * 255 / 63;
            uint8_t b0 = (color0 & 0x1F) * 255 / 31;
            
            uint8_t r1 = ((color1 >> 11) & 0x1F) * 255 / 31;
            uint8_t g1 = ((color1 >> 5) & 0x3F) * 255 / 63;
            uint8_t b1 = (color1 & 0x1F) * 255 / 31;
            
            uint32_t colorIndices = colorBlock[4] | (colorBlock[5] << 8) | (colorBlock[6] << 16) | (colorBlock[7] << 24);
            
            // Decode the 4x4 block
            for (int py = 0; py < 4; py++) {
                for (int px = 0; px < 4; px++) {
                    int pixelIndex = py * 4 + px;
                    int colorIndex = (colorIndices >> (pixelIndex * 2)) & 0x3;
                    uint8_t alpha = (uint8_t)((alphaData >> (pixelIndex * 4)) & 0x0F);
                    alpha = alpha * 255 / 15; // Expand 4-bit to 8-bit
                    
                    int x = bx * 4 + px;
                    int y = by * 4 + py;
                    
                    if (x < width && y < height) {
                        uint8_t* pixel = rgbaData + (y * width + x) * 4;
                        
                        // Decode color (same as DXT1)
                        switch (colorIndex) {
                            case 0:
                                pixel[0] = r0; pixel[1] = g0; pixel[2] = b0;
                                break;
                            case 1:
                                pixel[0] = r1; pixel[1] = g1; pixel[2] = b1;
                                break;
                            case 2:
                                pixel[0] = (2 * r0 + r1) / 3;
                                pixel[1] = (2 * g0 + g1) / 3;
                                pixel[2] = (2 * b0 + b1) / 3;
                                break;
                            case 3:
                                pixel[0] = (r0 + 2 * r1) / 3;
                                pixel[1] = (g0 + 2 * g1) / 3;
                                pixel[2] = (b0 + 2 * b1) / 3;
                                break;
                        }
                        
                        pixel[3] = alpha;
                    }
                }
            }
        }
    }
}

static void DecompressDXT5(const uint8_t* compressedData, uint8_t* rgbaData, int width, int height) {
    int blocksX = (width + 3) / 4;
    int blocksY = (height + 3) / 4;
    
    for (int by = 0; by < blocksY; by++) {
        for (int bx = 0; bx < blocksX; bx++) {
            const uint8_t* block = compressedData + (by * blocksX + bx) * 16;
            
            // Decode alpha channel first (8 bytes)
            uint8_t alpha0 = block[0];
            uint8_t alpha1 = block[1];
            uint64_t alphaIndices = 0;
            for (int i = 0; i < 6; i++) {
                alphaIndices |= (static_cast<uint64_t>(block[2 + i]) << (i * 8));
            }
            
            // Decode color channel (8 bytes)
            const uint8_t* colorBlock = block + 8;
            uint16_t color0 = colorBlock[0] | (colorBlock[1] << 8);
            uint16_t color1 = colorBlock[2] | (colorBlock[3] << 8);
            
            uint8_t r0 = ((color0 >> 11) & 0x1F) * 255 / 31;
            uint8_t g0 = ((color0 >> 5) & 0x3F) * 255 / 63;
            uint8_t b0 = (color0 & 0x1F) * 255 / 31;
            
            uint8_t r1 = ((color1 >> 11) & 0x1F) * 255 / 31;
            uint8_t g1 = ((color1 >> 5) & 0x3F) * 255 / 63;
            uint8_t b1 = (color1 & 0x1F) * 255 / 31;
            
            uint32_t colorIndices = colorBlock[4] | (colorBlock[5] << 8) | (colorBlock[6] << 16) | (colorBlock[7] << 24);
            
            // Decode the 4x4 block
            for (int py = 0; py < 4; py++) {
                for (int px = 0; px < 4; px++) {
                    int pixelIndex = py * 4 + px;
                    int colorIndex = (colorIndices >> (pixelIndex * 2)) & 0x3;
                    int alphaIndex = (alphaIndices >> (pixelIndex * 3)) & 0x7;
                    
                    int x = bx * 4 + px;
                    int y = by * 4 + py;
                    
                    if (x < width && y < height) {
                        uint8_t* pixel = rgbaData + (y * width + x) * 4;
                        
                        // Decode color
                        switch (colorIndex) {
                            case 0:
                                pixel[0] = r0; pixel[1] = g0; pixel[2] = b0;
                                break;
                            case 1:
                                pixel[0] = r1; pixel[1] = g1; pixel[2] = b1;
                                break;
                            case 2:
                                pixel[0] = (2 * r0 + r1) / 3;
                                pixel[1] = (2 * g0 + g1) / 3;
                                pixel[2] = (2 * b0 + b1) / 3;
                                break;
                            case 3:
                                pixel[0] = (r0 + 2 * r1) / 3;
                                pixel[1] = (g0 + 2 * g1) / 3;
                                pixel[2] = (b0 + 2 * b1) / 3;
                                break;
                        }
                        
                        // Decode alpha
                        uint8_t alpha;
                        if (alphaIndex == 0) {
                            alpha = alpha0;
                        } else if (alphaIndex == 1) {
                            alpha = alpha1;
                        } else if (alpha0 > alpha1) {
                            alpha = (uint8_t)(((8 - alphaIndex) * alpha0 + (alphaIndex - 1) * alpha1) / 7);
                        } else {
                            if (alphaIndex == 6) alpha = 0;
                            else if (alphaIndex == 7) alpha = 255;
                            else alpha = (uint8_t)(((6 - alphaIndex) * alpha0 + (alphaIndex - 1) * alpha1) / 5);
                        }
                        pixel[3] = alpha;
                    }
                }
            }
        }
    }
}

void VTFLoader::ConvertToRGBA(const uint8_t* src, uint8_t* dst, size_t pixelCount, VTFFormat format) {
    switch (format) {
        case VTFFormat::RGBA8888: {
            // Already RGBA, just copy
            std::memcpy(dst, src, pixelCount * 4);
            break;
        }
        case VTFFormat::BGRA8888: {
            // Convert BGRA to RGBA
            for (size_t i = 0; i < pixelCount; i++) {
                dst[i * 4 + 0] = src[i * 4 + 2]; // R <- B
                dst[i * 4 + 1] = src[i * 4 + 1]; // G <- G
                dst[i * 4 + 2] = src[i * 4 + 0]; // B <- R
                dst[i * 4 + 3] = src[i * 4 + 3]; // A <- A
            }
            break;
        }
        case VTFFormat::RGB888:
        case VTFFormat::BGR888: {
            // Convert RGB/BGR to RGBA with full alpha
            for (size_t i = 0; i < pixelCount; i++) {
                if (format == VTFFormat::RGB888) {
                    dst[i * 4 + 0] = src[i * 3 + 0]; // R
                    dst[i * 4 + 1] = src[i * 3 + 1]; // G
                    dst[i * 4 + 2] = src[i * 3 + 2]; // B
                } else {
                    dst[i * 4 + 0] = src[i * 3 + 2]; // R <- B
                    dst[i * 4 + 1] = src[i * 3 + 1]; // G <- G
                    dst[i * 4 + 2] = src[i * 3 + 0]; // B <- R
                }
                dst[i * 4 + 3] = 0xFF; // A
            }
            break;
        }
        case VTFFormat::I8: {
            // Convert grayscale to RGBA
            for (size_t i = 0; i < pixelCount; i++) {
                uint8_t v = src[i];
                dst[i * 4 + 0] = v;
                dst[i * 4 + 1] = v;
                dst[i * 4 + 2] = v;
                dst[i * 4 + 3] = 0xFF;
            }
            break;
        }
        case VTFFormat::IA88: {
            // Convert grayscale + alpha to RGBA
            for (size_t i = 0; i < pixelCount; i++) {
                uint8_t v = src[i * 2 + 0];
                uint8_t a = src[i * 2 + 1];
                dst[i * 4 + 0] = v;
                dst[i * 4 + 1] = v;
                dst[i * 4 + 2] = v;
                dst[i * 4 + 3] = a;
            }
            break;
        }
        case VTFFormat::RGB565:
        case VTFFormat::BGR565: {
            // Convert RGB565/BGR565 to RGBA
            for (size_t i = 0; i < pixelCount; i++) {
                uint16_t p = src[i * 2 + 0] | (src[i * 2 + 1] << 8);
                if (format == VTFFormat::RGB565) {
                    dst[i * 4 + 0] = ((p >> 11) & 0x1F) << 3;
                    dst[i * 4 + 1] = ((p >> 5) & 0x3F) << 2;
                    dst[i * 4 + 2] = (p & 0x1F) << 3;
                } else {
                    dst[i * 4 + 0] = (p & 0x1F) << 3;
                    dst[i * 4 + 1] = ((p >> 5) & 0x3F) << 2;
                    dst[i * 4 + 2] = ((p >> 11) & 0x1F) << 3;
                }
                dst[i * 4 + 3] = 0xFF;
            }
            break;
        }
        case VTFFormat::BGRA5551: {
            // Convert BGRA5551 to RGBA
            for (size_t i = 0; i < pixelCount; i++) {
                uint16_t p = src[i * 2 + 0] | (src[i * 2 + 1] << 8);
                dst[i * 4 + 0] = ((p >> 10) & 0x1F) << 3;
                dst[i * 4 + 1] = ((p >> 5) & 0x1F) << 3;
                dst[i * 4 + 2] = (p & 0x1F) << 3;
                dst[i * 4 + 3] = (p & 0x1) ? 0xFF : 0x00;
            }
            break;
        }
        case VTFFormat::BGRA4444: {
            // Convert BGRA4444 to RGBA
            for (size_t i = 0; i < pixelCount; i++) {
                uint16_t p = src[i * 2 + 0] | (src[i * 2 + 1] << 8);
                dst[i * 4 + 0] = ((p >> 12) & 0x0F) << 4;
                dst[i * 4 + 1] = ((p >> 8) & 0x0F) << 4;
                dst[i * 4 + 2] = ((p >> 4) & 0x0F) << 4;
                dst[i * 4 + 3] = (p & 0x0F) << 4;
            }
            break;
        }
        default:
            // Unsupported format, clear to black
            std::memset(dst, 0, pixelCount * 4);
            break;
    }
}

const uint8_t* VTFLoader::GetMipData(int mipLevel, size_t* outDataSize) const {
    if (!m_loaded || mipLevel < 0 || mipLevel >= m_mipCount) {
        if (outDataSize) *outDataSize = 0;
        return nullptr;
    }
    
    if (outDataSize) *outDataSize = m_mipData[mipLevel].size();
    return m_mipData[mipLevel].data();
}

} // namespace veex
