#include "veex/Texture.h"
#include <glad/gl.h>
#include "veex/Logger.h"
#include "veex/VTFLoader.h"
#include "veex/GLHeaders.h"
#include "stb/stb_image.h"

namespace veex {

Texture::Texture() : m_id(0) {}

Texture::~Texture() {
    if (m_id != 0) glDeleteTextures(1, &m_id);
}

bool Texture::CreateFallbackTexture() {
    // Purple/Black checkerboard
    unsigned char fallback[] = {
        255, 0, 255, 255,   0, 0, 0, 255,
        0, 0, 0, 255,       255, 0, 255, 255
    };

    if (m_id == 0) glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallback);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    m_width = 2;
    m_height = 2;
    return false;
}

bool Texture::LoadFromFile(const std::string& path) {
    // Check file extension to determine loader
    std::string ext = "";
    size_t dotPos = path.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = path.substr(dotPos);
        // Convert to lowercase
        for (auto& c : ext) c = static_cast<char>(std::tolower(c));
    }
    
    // Try VTF loader for .vtf files
    if (ext == ".vtf") {
        VTFLoader loader;
        if (!loader.LoadFromFile(path)) {
            Logger::Error("Failed to load VTF texture: " + path + " - Using fallback.");
            return CreateFallbackTexture();
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
                    Logger::Error("VTF loader returned invalid compressed data: " + path + " - Using fallback.");
                    return CreateFallbackTexture();
                }
                
                if (m_id == 0) glGenTextures(1, &m_id);
                glBindTexture(GL_TEXTURE_2D, m_id);
                
                // Use glCompressedTexImage2D for compressed formats
                glCompressedTexImage2D(GL_TEXTURE_2D, 0, glInternalFormat, width, height, 0, dataSize, compressedData);
                
                // DO NOT call glGenerateMipmap for compressed textures - it causes GL_INVALID_OPERATION
                // VTF files already contain mipmaps, so we should upload them manually if needed
                // For now, set the min filter to GL_LINEAR to avoid mipmap issues
                
                Logger::Info("Uploaded compressed DXT texture: " + path + " (format: " + std::to_string(glInternalFormat) + ", size: " + std::to_string(dataSize) + " bytes)");
            } else {
                // DXT not supported, fall back to decompressed RGBA
                Logger::Warn("DXT compression not supported, falling back to decompressed RGBA for: " + path);
                const uint8_t* data = loader.GetRGBAData(&width, &height);
                if (!data) {
                    Logger::Error("VTF loader returned no data: " + path + " - Using fallback.");
                    return CreateFallbackTexture();
                }
                
                if (m_id == 0) glGenTextures(1, &m_id);
                glBindTexture(GL_TEXTURE_2D, m_id);
                
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                
                // VTFLoader always returns RGBA8888 data (4 bytes per pixel)
                // So we always use GL_RGBA for both internal and format parameters
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);
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
            // NOTE: For compressed textures without manually uploaded mipmaps,
            // we must use GL_LINEAR instead of GL_LINEAR_MIPMAP_LINEAR to avoid black textures
            if (flags & static_cast<uint32_t>(VTFFlags::POINTSAMPLE)) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            } else {
                // Use GL_LINEAR for min filter since we don't have mipmaps for compressed textures
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }
            
            glBindTexture(GL_TEXTURE_2D, 0);
            m_width = width;
            m_height = height;
            return true;
        } else {
            // Use uncompressed texture upload
            const uint8_t* data = loader.GetRGBAData(&width, &height);
            if (!data) {
                Logger::Error("VTF loader returned no data: " + path + " - Using fallback.");
                return CreateFallbackTexture();
            }
            
            if (m_id == 0) glGenTextures(1, &m_id);
            glBindTexture(GL_TEXTURE_2D, m_id);
            
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            
            // VTFLoader always returns RGBA8888 data (4 bytes per pixel)
            // So we always use GL_RGBA for both internal and format parameters
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
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
            } else {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }
            
            glBindTexture(GL_TEXTURE_2D, 0);
            m_width = width;
            m_height = height;
            return true;
        }
    }
    
    // Standard image loading via stb_image (PNG, JPG, BMP, TGA, etc.)
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true); 
    
    // Force 4 channels (RGBA)
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

    if (!data) {
        Logger::Error("Failed to load texture: " + path + " - Using fallback.");
        return CreateFallbackTexture();
    }

    if (m_id == 0) glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    m_width = width;
    m_height = height;
    return true;
}

void Texture::Bind(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

void Texture::Unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace veex