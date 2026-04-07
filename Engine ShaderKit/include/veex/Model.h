#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace veex {

struct ModelVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

class Model {
public:
    Model();
    ~Model();

    bool LoadFromFile(const std::string& path);
    void Draw() const;

private:
    unsigned int m_VAO = 0, m_VBO = 0, m_EBO = 0;
    std::vector<ModelVertex> m_vertices;
    std::vector<unsigned int> m_indices;
};

} // namespace veex