// veex/Client.cpp
// Game client: owns the renderer, current map, camera, and player controller.
//
// Fixed: the original code called BuildLightmapAtlas() and BuildVertexBuffer()
// a second time after BSP::LoadFromFile had already called both.  That
// caused a second (redundant) GL texture upload and a wasted CPU rebuild.
// LoadFromFile() is now authoritative — Client just calls UploadMap().

#include "veex/Client.h"
#include "veex/Logger.h"
#include "veex/Server.h"
#include "veex/Skybox.h"
#include "veex/GUI.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include "veex/SoundKit.h"

namespace veex {

Client::Client(GLFWwindow* window)
    : m_window(window)
    , m_firstMouse(true)
    , m_lastX(640.0)
    , m_lastY(360.0)
    , m_mouseSensitivity(0.12f)
    , m_feetPos(0.0f)
{}

bool Client::Init(const GameInfo& game)
{
    Logger::Info("[Client] Initializing…");

    if (!m_renderer.Init(game)) {
        Logger::Error("[Client] Renderer initialization failed.");
        return false;
    }

    // LoadFromFile: parses lumps, builds lightmap atlas, builds vertex buffer.
    Logger::Info("[Client] Loading map: maps/background01.bsp");
    if (!m_currentMap.LoadFromFile("maps/background01.bsp", game)) {
        Logger::Error("[Client] Failed to load map.");
        return false;
    }

    // Upload the VBO to the GPU — must happen after LoadFromFile.
    m_renderer.UploadMap(m_currentMap);

    m_player.BuildCollisionMesh(m_currentMap);
    m_camera.SetRotation(-90.0f, 0.0f);

    {
        const auto stats = m_currentMap.GetBatchStats();
        Logger::Info("[Client] Map ready: "
                     + std::to_string(stats.totalVertices) + " vertices, "
                     + std::to_string(stats.totalBatches)  + " draw calls "
                     + "(from " + std::to_string(stats.totalFaces) + " BSP faces).");
    }
    Logger::Info("[Client] Initialized.");
    return true;
}

void Client::SetSpawnPoint(const glm::vec3& spawnFeetPos)
{
    m_feetPos = spawnFeetPos;
    m_camera.SetPosition(m_feetPos + glm::vec3(0.0f, 1.7f, 0.0f));
    Logger::Info("[Client] Spawn set: feet=("
                 + std::to_string(m_feetPos.x) + ", "
                 + std::to_string(m_feetPos.y) + ", "
                 + std::to_string(m_feetPos.z) + ")");
}

void Client::HandleMouseLook(float dt)
{
    double xpos, ypos;
    glfwGetCursorPos(m_window, &xpos, &ypos);

    if (m_firstMouse) {
        m_lastX = xpos; m_lastY = ypos; m_firstMouse = false;
    }

    const float xoffset = static_cast<float>(xpos - m_lastX) * m_mouseSensitivity;
    const float yoffset = static_cast<float>(m_lastY - ypos) * m_mouseSensitivity;
    m_lastX = xpos; m_lastY = ypos;

    static float currentYaw   = -90.0f;
    static float currentPitch =   0.0f;
    currentYaw   += xoffset;
    currentPitch  = glm::clamp(currentPitch + yoffset, -89.0f, 89.0f);
    m_camera.SetRotation(currentYaw, currentPitch);

    // ── Noclip toggle (V key) ─────────────────────────────────────────────────
    static bool vWasPressed = false;
    const bool  vIsPressed  = (glfwGetKey(m_window, GLFW_KEY_V) == GLFW_PRESS);
    if (vIsPressed && !vWasPressed) {
        const bool newState = !m_player.IsNoclip();
        m_player.SetNoclip(newState);
        Logger::Info("[Client] Noclip: " + std::string(newState ? "ON" : "OFF"));
    }
    vWasPressed = vIsPressed;

    // ── Movement ──────────────────────────────────────────────────────────────
    glm::vec3 forward, right;
    if (m_player.IsNoclip()) {
        forward = m_camera.GetForward();
        right   = m_camera.GetRight();
    } else {
        // Flatten to the XZ plane so WASD doesn't climb/descend with pitch.
        forward = glm::normalize(glm::vec3(m_camera.GetForward().x, 0.0f,
                                           m_camera.GetForward().z));
        right   = glm::normalize(glm::vec3(m_camera.GetRight().x,   0.0f,
                                           m_camera.GetRight().z));
    }

    glm::vec3 wishDir(0.0f);
    if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS) wishDir += forward;
    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS) wishDir -= forward;
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS) wishDir -= right;
    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS) wishDir += right;
    if (glm::length(wishDir) > 0.0f) wishDir = glm::normalize(wishDir);

    const float wishSpeed = (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
                            ? 400.0f : 200.0f;
    const bool  jumping   = (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS);

    m_feetPos = m_player.Move(m_feetPos, wishDir, wishSpeed, jumping, dt);
    m_camera.SetPosition(m_feetPos + glm::vec3(0.0f, 1.7f, 0.0f));
    SoundKit::Get().Update(m_camera.GetPosition(), m_camera.GetForward(), &m_currentMap);
}

void Client::Render(const Server&, Skybox& skybox)
{
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);
    m_renderer.Render(width, height, m_camera, m_currentMap, skybox);
}

void Client::ResetMouseTracking()
{
    m_firstMouse = true;
}

void Client::Shutdown()
{
    m_renderer.Shutdown();
    Logger::Info("[Client] Shutdown.");
}

} // namespace veex
