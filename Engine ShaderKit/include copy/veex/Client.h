#pragma once
#include "veex/GLHeaders.h"
#include "veex/Renderer.h"
#include "veex/Camera.h"
#include "veex/BSP.h"
#include "veex/GameInfo.h"
#include "veex/PlayerController.h"
#include <glm/glm.hpp>

// Forward declaration for GLFW
struct GLFWwindow;

namespace veex {

// Forward declarations to avoid circular dependencies
class Server;
class Skybox;

class Client {
public:
    explicit Client(::GLFWwindow* window);

    bool Init(const GameInfo& game);
    void SetSpawnPoint(const glm::vec3& spawnFeetPos);
    void Render(const Server& server, Skybox& skybox);
    void Shutdown();
    void HandleMouseLook(float dt);

    // Getters
    Camera&       GetCamera()           { return m_camera; }
    const Camera& GetCamera()     const { return m_camera; }
    BSP&          GetCurrentMap()       { return m_currentMap; }
    const BSP&    GetCurrentMap() const { return m_currentMap; }

private:
    ::GLFWwindow* m_window = nullptr;
    Renderer         m_renderer;
    Camera           m_camera;
    BSP              m_currentMap;
    PlayerController m_player;

    glm::vec3 m_feetPos = glm::vec3(0.0f);

    // Input and State tracking
    bool   m_firstMouse       = true;
    bool   m_isFirstFrame     = true; 
    float  m_mouseSensitivity = 0.15f; 
    double m_lastX            = 640.0;
    double m_lastY            = 360.0;
};

} // namespace veex