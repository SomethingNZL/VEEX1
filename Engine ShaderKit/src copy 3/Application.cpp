#include "veex/Application.h"
#include "veex/Config.h"
#include "veex/Logger.h"
#include "veex/MaterialSystem.h" 
#include "veex/FileSystem.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <chrono>

namespace veex {

Application::Application(const std::string& gameInfoPath, const std::string& gameInfoBinPath)
    : m_gameInfoPath(gameInfoPath), m_gameInfoBinPath(gameInfoBinPath), m_window(nullptr), m_client(nullptr) {
}

int Application::Run() {
    if (!Initialize()) {
        Logger::Error("Application: Failed to initialize.");
        return -1;
    }
    MainLoop();
    Shutdown();
    return 0;
}

bool Application::Initialize() {
    if (!glfwInit()) return false;

    // Standard Core 3.3 for macOS
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    m_window = glfwCreateWindow(1280, 720, "VEEX Engine", NULL, NULL);
    if (!m_window) return false;

    glfwMakeContextCurrent(m_window);
    
    // CRITICAL MAC FIX: Load GLAD first
    if (!gladLoadGL(glfwGetProcAddress)) return false;

    // CRITICAL MAC FIX: Force a viewport sync immediately after context creation
    int fbW, fbH;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    // CRITICAL MAC FIX: Clear the screen to blue ONCE during init and swap
    // This forces the Window Manager to acknowledge the OpenGL surface
    glClearColor(0.39f, 0.58f, 0.93f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(m_window);

    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    // Load Game Configuration
    std::string fullGameInfoPath = ResolveAssetPath(m_gameInfoPath, m_gameInfo);
    if (!m_gameInfo.LoadFromFile(fullGameInfoPath)) return false;

    MaterialSystem::Get().Initialize(m_gameInfo);

    m_client = new Client(m_window);
    if (!m_client->Init(m_gameInfo)) return false;

    // Fix: Using virtual path "entities.binfo" and passing m_gameInfo to Init
    // This allows the Server to pass m_gameInfo into BinfoParser::LoadFromFile
    m_server.Init(m_client->GetCurrentMap().GetParser().GetEntityData(), "entities.binfo", m_gameInfo);

    m_skybox.Initialize(m_server.GetSkyName(), m_gameInfo);
    m_client->SetSpawnPoint(m_server.GetSpawnPoint());

    Logger::Info("Application: VEEX Engine Initialized Successfully.");
    return true;
}

void Application::MainLoop() {
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    while (!glfwWindowShouldClose(m_window)) {
        auto now = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        if (frameTime > 0.1f) frameTime = 0.1f;

        glfwPollEvents();

        m_server.Tick(m_fixedTick);

        if (m_client) {
            m_client->HandleMouseLook(frameTime);
            // This calls Renderer::Render which does the glClear and drawing
            m_client->Render(m_server, m_skybox); 
        }

        // Final hand-off to the OS
        glfwSwapBuffers(m_window);
    }
}

void Application::Shutdown() {
    Logger::Info("Application: Shutting down...");
    MaterialSystem::Get().Shutdown();
    m_skybox.Shutdown();
    m_server.Shutdown();
    if (m_client) {
        m_client->Shutdown();
        delete m_client;
        m_client = nullptr;
    }
    if (m_window) {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
}

} // namespace veex