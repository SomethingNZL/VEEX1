#include "veex/TextureAtlas.h"
#include "veex/Logger.h"
#include "veex/GLHeaders.h"
#include <algorithm>
#include <cstring>

namespace veex {

// Simple DXT1 decompressor for fallback on macOS
// Converts DXT1 compressed data to RGBA
static void DecompressDXT1(const uint8_t* input, uint8_t* output, int width, int height) {
    for (int y = 0; y < height; y += 4) {
        for (int x = 0; x < width; x += 4) {
            // DXT1 uses 8 bytes per 4x4 block
            const uint16_t* block = reinterpret_cast<const uint16_t*>(input);
            uint16_t c0 = block[0];
            uint16_t c1 = block[1];
            uint32_t bits = reinterpret_cast<const uint32_t*>(block)[1];
            
            // Extract RGB colors
            int r0 = (c0 >> 11) & 0x1F;
            int g0 = (c0 >> 5) & 0x3F;
            int b0 = c0 & 0x1F;
            int r1 = (c1 >> 11) & 0x1F;
            int g1 = (c1 >> 5) & 0x3F;
            int b1 = c1 & 0x1F;
            
            // Convert to 8-bit
            uint8_t R0 = (r0 * 255) / 31;
            uint8_t G0 = (g0 * 255) / 63;
            uint8_t B0 = (b0 * 255) / 31;
            uint8_t R1 = (r1 * 255) / 31;
            uint8_t G1 = (g1 * 255) / 63;
            uint8_t B1 = (b1 * 255) / 31;
            
            // Process each pixel in the 4x4 block
            for (int py = 0; py < 4 && (y + py) < height; py++) {
                for (int px = 0; px < 4 && (x + px) < width; px++) {
                    int idx = ((y + py) * width + (x + px)) * 4;
                    int code = (bits >> (2 * (py * 4 + px))) & 0x03;
                    
                    if (c0 > c1 || code < 3) {
                        switch (code) {
                            case 0: output[idx] = R0; output[idx+1] = G0; output[idx+2] = B0; output[idx+3] = 255; break;
                            case 1: output[idx] = R1; output[idx+1] = G1; output[idx+2] = B1; output[idx+3] = 255; break;
                            case 2: output[idx] = (R0 + R1) / 2; output[idx+1] = (G0 + G1) / 2; output[idx+2] = (B0 + B1) / 2; output[idx+3] = 255; break;
                            case 3: output[idx] = 0; output[idx+1] = 0; output[idx+2] = 0; output[idx+3] = 0; break;
                        }
                    } else {
                        switch (code) {
                            case 0: output[idx] = R0; output[idx+1] = G0; output[idx+2] = B0; output[idx+3] = 255; break;
                            case 1: output[idx] = R1; output[idx+1] = G1; output[idx+2] = B1; output[idx+3] = 255; break;
                            case 2: output[idx] = (R0 * 2 + R1) / 3; output[idx+1] = (G0 * 2 + G1) / 3; output[idx+2] = (B0 * 2 + B1) / 3; output[idx+3] = 255; break;
                            case 3: output[idx] = (R0 + R1 * 2) / 3; output[idx+1] = (G0 + G1 * 2) / 3; output[idx+2] = (B0 + B1 * 2) / 3; output[idx+3] = 255; break;
                        }
                    }
                }
            }
            input += 8;
        }
    }
}

TextureAtlas::TextureAtlas() {}

TextureAtlas::~TextureAtlas() {
    Shutdown();
}

bool TextureAtlas::IsDXTSupported() {
    // Use the existing helper from GLHeaders.h
    return ::IsDXTSupported();
}

GLenum TextureAtlas::DetermineFormat() {
    // Always use RGBA8
    return GL_RGBA8;
}

GLenum TextureAtlas::GetUploadFormat(GLenum sourceFormat) const {
    if (!IsDXTSupported()) {
        return GL_RGBA; // Fallback to RGBA for upload
    }
    
    // The upload format must match the atlas's internal format
    // For DXT1 atlases, we need to use GL_COMPRESSED_RGB_S3TC_DXT1_EXT
    // For DXT5 atlases, we need to use GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
    
    // If the atlas is DXT1, we need to handle RGBA DXT1 data carefully
    if (m_internalFormat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT) {
        if (sourceFormat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT) {
            return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        }
        if (sourceFormat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) {
            // RGBA DXT1 data can be uploaded to RGB DXT1 atlas
            // The alpha information will be ignored, but the RGB data is compatible
            return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; // Use the source format
        }
        // If source is not DXT1, we can't upload it to a DXT1 atlas
        Logger::Warn("Cannot upload format " + std::to_string(sourceFormat) + 
                     " to DXT1 atlas");
        return m_internalFormat; // Return atlas format anyway
    }
    
    // If the atlas is DXT5, accept DXT5 sources
    if (m_internalFormat == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
        if (sourceFormat == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
            return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        }
        // For DXT1 sources, we can upload to DXT5 atlas (DXT5 is superset)
        if (sourceFormat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ||
            sourceFormat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT) {
            return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; // Upload as DXT5
        }
        Logger::Warn("Cannot upload format " + std::to_string(sourceFormat) + 
                     " to DXT5 atlas");
        return m_internalFormat;
    }
    
    // Default fallback
    switch (sourceFormat) {
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        case GL_RGBA:
        case GL_SRGB8_ALPHA8:
            return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        case GL_RGB:
        case GL_SRGB8:
            return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        default:
            return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    }
}

bool TextureAtlas::Initialize(int width, int height) {
    if (m_initialized) {
        Logger::Warn("TextureAtlas::Initialize called on already initialized atlas");
        return true;
    }

    // Ensure dimensions are multiples of 4
    m_width = ((width + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    m_height = ((height + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;

    Logger::Info("Initializing TextureAtlas: " + std::to_string(m_width) + "x" + 
                 std::to_string(m_height));

    // Determine format based on platform capabilities and requested format
    m_internalFormat = DetermineFormat();
    
    const char* formatStr = (m_internalFormat == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) 
                            ? "DXT5 (compressed)" 
                            : (m_internalFormat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT)
                            ? "DXT1 (compressed)"
                            : "RGBA8 (uncompressed)";
    Logger::Info("TextureAtlas using format: " + std::string(formatStr));

    // Create the OpenGL texture
    if (!CreateGLTexture()) {
        Logger::Error("Failed to create TextureAtlas OpenGL texture");
        return false;
    }

    m_initialized = true;
    m_nextID = 1;
    m_allocations.clear();
    m_allocationIndex.clear();

    Logger::Info("TextureAtlas initialized successfully");
    return true;
}

bool TextureAtlas::CreateGLTexture() {
    glGenTextures(1, &m_atlasTexture);
    if (!m_atlasTexture) {
        Logger::Error("Failed to generate TextureAtlas texture ID");
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, m_atlasTexture);

    // Uncompressed format (RGBA8)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        m_internalFormat,
        m_width,
        m_height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr  // NULL = allocate storage without data
    );

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        Logger::Error("glTexImage2D failed with error: " + std::to_string(err));
        glDeleteTextures(1, &m_atlasTexture);
        m_atlasTexture = 0;
        return false;
    }

    // Set texture parameters
    // Use GL_LINEAR for min filter since we're not using mipmaps with atlases
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    Logger::Info("TextureAtlas GL texture created: ID=" + std::to_string(m_atlasTexture));
    return true;
}

void TextureAtlas::Shutdown() {
    if (m_atlasTexture != 0) {
        glDeleteTextures(1, &m_atlasTexture);
        m_atlasTexture = 0;
    }
    m_allocations.clear();
    m_allocationIndex.clear();
    m_initialized = false;
    Logger::Info("TextureAtlas shutdown complete");
}

int TextureAtlas::GenerateAllocationID() {
    return m_nextID++;
}

bool TextureAtlas::FindAllocationSpot(int width, int height, int& outX, int& outY) {
    // First-fit algorithm: find the first free space that fits
    // We scan left-to-right, top-to-bottom

    // Align dimensions to block size
    int alignedWidth = ((width + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    int alignedHeight = ((height + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;

    // Add padding
    int requiredWidth = alignedWidth + PADDING_PIXELS;
    int requiredHeight = alignedHeight + PADDING_PIXELS;

    // Simple approach: try positions in a grid pattern
    // For better performance, a skyline algorithm could be used
    for (int y = 0; y + requiredHeight <= m_height; y += BLOCK_SIZE) {
        for (int x = 0; x + requiredWidth <= m_width; x += BLOCK_SIZE) {
            bool fits = true;

            // Check if this position overlaps with any existing allocation
            for (const auto& alloc : m_allocations) {
                if (alloc.free) continue;

                // Check for intersection
                bool overlapX = (x < alloc.x + alloc.width + PADDING_PIXELS) && 
                               (x + requiredWidth > alloc.x - PADDING_PIXELS);
                bool overlapY = (y < alloc.y + alloc.height + PADDING_PIXELS) && 
                               (y + requiredHeight > alloc.y - PADDING_PIXELS);

                if (overlapX && overlapY) {
                    fits = false;
                    break;
                }
            }

            if (fits) {
                outX = x;
                outY = y;
                return true;
            }
        }
    }

    return false;
}

int TextureAtlas::AllocateTexture(int width, int height,
                                  const uint8_t* data, size_t dataSize,
                                  GLenum format,
                                  int inputWidth, int inputHeight) {
    if (!m_initialized) {
        Logger::Error("TextureAtlas::AllocateTexture called on uninitialized atlas");
        return -1;
    }

    if (!data || dataSize == 0) {
        Logger::Error("TextureAtlas::AllocateTexture called with null or empty data");
        return -1;
    }

    // Use input dimensions if provided, otherwise use the passed width/height
    int actualInputWidth = (inputWidth > 0) ? inputWidth : width;
    int actualInputHeight = (inputHeight > 0) ? inputHeight : height;

    // Align to block size
    int alignedWidth = ((width + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    int alignedHeight = ((height + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;

    // Find a spot in the atlas
    int x, y;
    if (!FindAllocationSpot(alignedWidth, alignedHeight, x, y)) {
        Logger::Error("TextureAtlas: No space available for texture " + 
                     std::to_string(width) + "x" + std::to_string(height));
        return -1;
    }

    // Create allocation record
    Allocation alloc;
    alloc.id = GenerateAllocationID();
    alloc.x = x;
    alloc.y = y;
    alloc.width = alignedWidth;
    alloc.height = alignedHeight;
    alloc.inputWidth = actualInputWidth;
    alloc.inputHeight = actualInputHeight;
    alloc.free = false;

    m_allocations.push_back(alloc);
    m_allocationIndex[alloc.id] = m_allocations.size() - 1;

    // Upload data to the atlas
    UploadData(x, y, alignedWidth, alignedHeight, data, dataSize, format);

    Logger::Info("TextureAtlas: Allocated texture " + std::to_string(alloc.id) + 
                 " at (" + std::to_string(x) + ", " + std::to_string(y) + ") " +
                 "size " + std::to_string(alignedWidth) + "x" + std::to_string(alignedHeight));

    return alloc.id;
}

void TextureAtlas::UploadData(int x, int y, int width, int height,
                             const uint8_t* data, size_t dataSize, GLenum format) {
    glBindTexture(GL_TEXTURE_2D, m_atlasTexture);

    // Check if we're using a compressed internal format
    bool isCompressed = (m_internalFormat == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT ||
                        m_internalFormat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ||
                        m_internalFormat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);

    // Check if source data is compressed
    bool sourceIsCompressed = (format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT ||
                               format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ||
                               format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);

    GLenum err = GL_NO_ERROR;

    if (isCompressed && sourceIsCompressed) {
        // Both atlas and source are compressed - use compressed upload
        GLenum uploadFormat = format;
        if (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT && 
            m_internalFormat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT) {
            uploadFormat = format;
        }
        
        glCompressedTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            x, y,
            width, height,
            uploadFormat,
            static_cast<GLsizei>(dataSize),
            data
        );
        
        err = glGetError();
        if (err != GL_NO_ERROR) {
            Logger::Error("TextureAtlas: Compressed upload failed with error " + std::to_string(err));
            
            // On macOS, glCompressedTexSubImage2D fails with 1280/1282 for DXT
            // We can't upload RGBA to compressed storage, so skip this texture
            // The texture will be blank/black but won't crash
            Logger::Warn("Skipping texture - cannot upload compressed data to compressed atlas on macOS");
            glBindTexture(GL_TEXTURE_2D, 0);
            return;
        }
    } else {
        // Atlas is RGBA8 - always upload as uncompressed RGBA
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        
        // If source is compressed, we need to decompress it first
        if (sourceIsCompressed) {
            // Decompress DXT data to RGBA
            size_t decompressedSize = width * height * 4;
            uint8_t* decompressedData = new uint8_t[decompressedSize];
            DecompressDXT1(data, decompressedData, width, height);

            glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                x, y,
                width, height,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                decompressedData
            );
            GLenum err = glGetError();

            delete[] decompressedData;
        } else {
            glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                x, y,
                width, height,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                data
            );
            err = glGetError();
        }
    }

    if (err != GL_NO_ERROR) {
        Logger::Error("TextureAtlas: Upload failed with error " + std::to_string(err));
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void TextureAtlas::Free(int allocationID) {
    auto it = m_allocationIndex.find(allocationID);
    if (it == m_allocationIndex.end()) {
        Logger::Warn("TextureAtlas::Free called with invalid allocation ID: " + 
                    std::to_string(allocationID));
        return;
    }

    m_allocations[it->second].free = true;
    m_allocationIndex.erase(it);

    Logger::Info("TextureAtlas: Freed allocation " + std::to_string(allocationID));
}

glm::vec4 TextureAtlas::GetUVCrop(int allocationID) const {
    auto it = m_allocationIndex.find(allocationID);
    if (it == m_allocationIndex.end()) {
        Logger::Error("TextureAtlas::GetUVCrop called with invalid allocation ID: " + 
                     std::to_string(allocationID));
        return glm::vec4(0.0f);
    }

    const Allocation& alloc = m_allocations[it->second];

    // Calculate normalized coordinates
    float uOffset = static_cast<float>(alloc.x) / static_cast<float>(m_width);
    float vOffset = static_cast<float>(alloc.y) / static_cast<float>(m_height);
    float uScale = static_cast<float>(alloc.inputWidth) / static_cast<float>(m_width);
    float vScale = static_cast<float>(alloc.inputHeight) / static_cast<float>(m_height);

    return glm::vec4(uOffset, vOffset, uScale, vScale);
}

bool TextureAtlas::GetAllocationSize(int allocationID, int& width, int& height) const {
    auto it = m_allocationIndex.find(allocationID);
    if (it == m_allocationIndex.end()) {
        return false;
    }

    const Allocation& alloc = m_allocations[it->second];
    width = alloc.inputWidth;
    height = alloc.inputHeight;
    return true;
}

size_t TextureAtlas::GetUsedMemory() const {
    size_t used = 0;
    for (const auto& alloc : m_allocations) {
        if (!alloc.free) {
            if (m_internalFormat == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
                // DXT5: (width/4) * (height/4) * 16
                size_t blocksX = (alloc.width + BLOCK_SIZE - 1) / BLOCK_SIZE;
                size_t blocksY = (alloc.height + BLOCK_SIZE - 1) / BLOCK_SIZE;
                used += blocksX * blocksY * 16;
            } else {
                // RGBA8: width * height * 4
                used += alloc.width * alloc.height * 4;
            }
        }
    }
    return used;
}

size_t TextureAtlas::GetTotalMemory() const {
    if (m_internalFormat == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
        size_t blocksX = m_width / BLOCK_SIZE;
        size_t blocksY = m_height / BLOCK_SIZE;
        return blocksX * blocksY * 16;
    } else {
        return m_width * m_height * 4;
    }
}

float TextureAtlas::GetUsagePercentage() const {
    size_t total = GetTotalMemory();
    if (total == 0) return 0.0f;
    return (static_cast<float>(GetUsedMemory()) / static_cast<float>(total)) * 100.0f;
}

void TextureAtlas::ExtractRGBA8Data(uint8_t* outData) const {
    if (!m_initialized || m_atlasTexture == 0) {
        return;
    }
    
    // Bind the atlas texture
    glBindTexture(GL_TEXTURE_2D, m_atlasTexture);
    
    // Get pixels from OpenGL
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, outData);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace veex