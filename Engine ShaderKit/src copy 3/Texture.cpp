#include "veex/Texture.h"
#include <glad/gl.h>
#include "veex/Logger.h"
#include "stb/stb_image.h"

namespace veex {

Texture::Texture() : m_id(0) {}

Texture::~Texture() {
    if (m_id != 0) glDeleteTextures(1, &m_id);
}

bool Texture::LoadFromFile(const std::string& path) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true); 
    
    // Force 4 channels (RGBA)
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

    if (!data) {
        Logger::Error("Failed to load texture: " + path + " - Using fallback.");
        
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
        return false; 
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