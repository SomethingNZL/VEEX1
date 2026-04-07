#pragma once

#include <string>

namespace veex {

class Texture {
public:
    Texture();
    ~Texture();

    // Loading data into the GPU
    bool LoadFromFile(const std::string& path);

    // Bind to a specific texture unit (defaults to 0)
    // Marked 'const' so it can be called on 'const Texture' objects in the Renderer
    void Bind(unsigned int unit = 0) const;

    // Unbinds the current texture
    void Unbind() const; 

    // Helper to check the texture ID
    unsigned int GetID() const { return m_id; }

private:
    unsigned int m_id = 0;
    int m_width = 0, m_height = 0;
};

} // namespace veex