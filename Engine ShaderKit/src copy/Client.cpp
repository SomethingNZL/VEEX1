#include "veex/Client.h"
#include "veex/Logger.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

namespace veex {

Client::Client(GLFWwindow* window) : m_window(window) {
    m_lastX = 640.0;
    m_lastY = 360.0;
    m_firstMouse = true;
}

bool Client::Init(const GameInfo& game) {
    if (!m_window) return false;
    
    if (!m_renderer.Init()) {
        Logger::Error("Client: Renderer Init Failed");
        return false;
    }

    // --- PATCHED: Pass 'game' to the loader so FileSystem knows where to look ---
    std::string mapPath = "maps/test.vbsp"; 
    m_currentMap.LoadFromFile(mapPath, game); 

    // Initial Camera Setup: Backed up and elevated
    m_camera.SetPosition(glm::vec3(0.0f, 2.0f, 10.0f)); 
    
    // Lock the cursor
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    Logger::Info("Client: Initialized and assets loaded.");
    return true;
}

void Client::HandleMouseLook(float dt) {
    if (!m_window) return;

    // --- MOUSE LOOK ---
    double xpos, ypos;
    glfwGetCursorPos(m_window, &xpos, &ypos);

    if (m_firstMouse) {
        m_lastX = xpos;
        m_lastY = ypos;
        m_firstMouse = false;
    }

    float xoffset = (float)(xpos - m_lastX);
    float yoffset = (float)(m_lastY - ypos); 
    m_lastX = xpos;
    m_lastY = ypos;

    float sensitivity = 0.1f;
    m_camera.ProcessMouseMovement(xoffset * sensitivity, yoffset * sensitivity);
    
    // --- MANUAL WASD MOVEMENT ---
    float speed = 5.0f * dt;
    glm::vec3 pos = m_camera.GetPosition();
    glm::vec3 forward = m_camera.GetForward(); 
    glm::vec3 right = m_camera.GetRight();

    if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS) pos += forward * speed;
    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS) pos -= forward * speed;
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS) pos -= right * speed;
    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS) pos += right * speed;

    m_camera.SetPosition(pos);
}

void Client::Render() {
    m_renderer.BeginFrame(m_camera, m_window);
    m_renderer.Draw(m_currentMap, m_camera);
}

void Client::Shutdown() {
    Logger::Info("Client: Shutting down...");
    m_renderer.Shutdown();
}

} // namespace veex