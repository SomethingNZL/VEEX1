#include "veex/Camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace veex {

Camera::Camera() : m_position(0.0f), m_yaw(-90.0f), m_pitch(0.0f), m_fov(45.0f) {
    UpdateVectors();
}

void Camera::SetPosition(const glm::vec3& position) {
    m_position = position;
}

void Camera::SetRotation(float yaw, float pitch) {
    m_yaw = yaw;
    m_pitch = pitch;
    UpdateVectors();
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset) {
    m_yaw += xoffset;
    m_pitch += yoffset;

    if (m_pitch > 89.0f) m_pitch = 89.0f;
    if (m_pitch < -89.0f) m_pitch = -89.0f;

    UpdateVectors();
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_forward, m_up);
}

glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(m_fov), aspectRatio, 0.1f, 10000.0f);
}

// --- LINKER FIX: IMPLEMENTING THE GETTERS ---
glm::vec3 Camera::GetForward() const { return m_forward; }
glm::vec3 Camera::GetRight() const { return m_right; }
glm::vec3 Camera::GetUp() const { return m_up; }

void Camera::UpdateVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    
    m_forward = glm::normalize(front);
    m_right = glm::normalize(glm::cross(m_forward, glm::vec3(0.0f, 1.0f, 0.0f))); 
    m_up    = glm::normalize(glm::cross(m_right, m_forward));
}

} // namespace veex