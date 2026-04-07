#include "veex/Client.h"
#include "veex/Logger.h"
#include "veex/Server.h"
#include "veex/Skybox.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

namespace veex {

Client::Client(GLFWwindow* window) 
    : m_window(window), m_firstMouse(true), m_lastX(640.0), m_lastY(360.0), 
      m_mouseSensitivity(0.12f), m_feetPos(0.0f) {}

bool Client::Init(const GameInfo& game) {
    if (!m_renderer.Init(game)) return false;
    if (!m_currentMap.LoadFromFile("maps/background01.bsp", game)) return false;

    m_currentMap.BuildVertexBuffer();
    m_renderer.UploadMap(m_currentMap);
    m_player.BuildCollisionMesh(m_currentMap);
    
    m_camera.SetRotation(-90.0f, 0.0f);
    Logger::Info("Client: Initialized and Map Uploaded.");
    return true;
}

void Client::SetSpawnPoint(const glm::vec3& spawnFeetPos) {
    m_feetPos = spawnFeetPos;
    m_camera.SetPosition(m_feetPos + glm::vec3(0.0f, 1.7f, 0.0f));
}

void Client::HandleMouseLook(float dt) {
    double xpos, ypos;
    glfwGetCursorPos(m_window, &xpos, &ypos);
    
    if (m_firstMouse) { 
        m_lastX = xpos; 
        m_lastY = ypos; 
        m_firstMouse = false; 
    }

    float xoffset = static_cast<float>(xpos - m_lastX) * m_mouseSensitivity;
    float yoffset = static_cast<float>(m_lastY - ypos) * m_mouseSensitivity;
    m_lastX = xpos; 
    m_lastY = ypos;

    static float currentYaw = -90.0f, currentPitch = 0.0f;
    currentYaw += xoffset; 
    currentPitch += yoffset;
    
    currentPitch = glm::clamp(currentPitch, -89.0f, 89.0f);
    m_camera.SetRotation(currentYaw, currentPitch);

    // Get flat movement vectors (ignore Y for walking)
    glm::vec3 forward = glm::normalize(glm::vec3(m_camera.GetForward().x, 0, m_camera.GetForward().z));
    glm::vec3 right = glm::normalize(glm::vec3(m_camera.GetRight().x, 0, m_camera.GetRight().z));

    glm::vec3 wishDir(0.0f);
    if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS) wishDir += forward;
    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS) wishDir -= forward;
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS) wishDir -= right;
    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS) wishDir += right;

    // Normalize wishDir so diagonal movement isn't faster
    if (glm::length(wishDir) > 0.0f) {
        wishDir = glm::normalize(wishDir);
    }

    float wishSpeed = (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 400.0f : 200.0f;
    bool jumping = (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS);

    // MOVE CALL: Pass normalized direction, speed, and delta time.
    // Inside m_player.Move, ensure it does: position += wishDir * wishSpeed * dt
    m_feetPos = m_player.Move(m_feetPos, wishDir, wishSpeed, jumping, dt);
    
    // Update camera height relative to feet
    m_camera.SetPosition(m_feetPos + glm::vec3(0.0f, 1.7f, 0.0f));
}

void Client::Render(const Server&, Skybox& skybox) {
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);
    m_renderer.Render(width, height, m_camera, m_currentMap, skybox);
}

void Client::Shutdown() { 
    m_renderer.Shutdown(); 
}

} // namespace veex