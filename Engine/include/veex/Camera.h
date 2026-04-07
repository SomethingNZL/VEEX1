#pragma once
#include <glm/glm.hpp>

namespace veex {

class Camera {
public:
    Camera();

    void SetPosition(const glm::vec3& position);
    void SetRotation(float yaw, float pitch);
    void ProcessMouseMovement(float xoffset, float yoffset);
    
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    glm::vec3 GetPosition() const { return m_position; }
    
    // Added for Client.cpp math logic
    float GetYaw() const   { return m_yaw; }
    float GetPitch() const { return m_pitch; }

    glm::vec3 GetForward() const;
    glm::vec3 GetRight() const;
    glm::vec3 GetUp() const;

private:
    void UpdateVectors();

    glm::vec3 m_position;
    glm::vec3 m_forward;
    glm::vec3 m_right;
    glm::vec3 m_up;

    float m_yaw;
    float m_pitch;
    float m_fov;
};

} // namespace veex