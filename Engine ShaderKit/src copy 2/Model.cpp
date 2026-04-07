#include "veex/Model.h"
#include "veex/Logger.h"
#include <glad/gl.h>

namespace veex {

Model::Model() = default;

Model::~Model() {
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_EBO) glDeleteBuffers(1, &m_EBO);
}

bool Model::LoadFromFile(const std::string& path) {
    // TODO: Use tinygltf for GLB loading
    // Stub: simple quad
    m_vertices = {
        {{-0.5f, -0.5f, 0.0f}, {0,0,1}, {0,0}},
        {{ 0.5f, -0.5f, 0.0f}, {0,0,1}, {1,0}},
        {{ 0.5f,  0.5f, 0.0f}, {0,0,1}, {1,1}},
        {{-0.5f,  0.5f, 0.0f}, {0,0,1}, {0,1}}
    };
    m_indices = {0,1,2, 2,3,0};

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(ModelVertex), m_vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), m_indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, texCoord));
    glEnableVertexAttribArray(2);

    Logger::Info("Model loaded (stub): " + path);
    return true;
}

void Model::Draw() const {
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)m_indices.size(), GL_UNSIGNED_INT, 0);
}

} // namespace veex
