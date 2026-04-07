#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace veex {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct RenderBatch {
    uint32_t offset;
    uint32_t count;
    int textureID;
};

} // namespace veex