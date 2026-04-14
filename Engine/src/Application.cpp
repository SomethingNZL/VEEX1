#include "veex/Application.h"
#include "veex/Config.h"
#include "veex/Logger.h"
#include "veex/MaterialSystem.h" 
#include "veex/FileSystem.h"
#include "veex/SoundKit.h"
#include "veex/GUI.h"
#include "veex/PauseMenu.h"
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
        Shutdown();  // Fix: Call Shutdown() even on init failure to clean up resources
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
    glClearColor(0.39f, 0.58f, 0.93f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(m_window);

    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    // Load Game Configuration
    std::string fullGameInfoPath = ResolveAssetPath(m_gameInfoPath, m_gameInfo);
    if (!m_gameInfo.LoadFromFile(fullGameInfoPath)) return false;

    // --- FIX: INITIALIZE SOUNDKIT ---
    if (!SoundKit::Get().Initialize(m_gameInfo)) {
        Logger::Error("Application: SoundKit failed to initialize.");
    }

    MaterialSystem::Get().Initialize(m_gameInfo);

    // Initialize GUI system after OpenGL context is created
    GUI::Get().Initialize();
    
    // Set GUI to start hidden
    GUI::Get().SetVisible(false);
    
    // Initialize pause menu with game title from gameinfo
    std::string gameTitle = m_gameInfo.gameName.empty() ? "VEEX Game" : m_gameInfo.gameName;
    InitializePauseMenu(gameTitle);
    
    // Ensure pause menu starts hidden
    HidePauseMenu(m_window);

    m_client = new Client(m_window);
    if (!m_client->Init(m_gameInfo)) return false;

    m_server.Init(m_client->GetCurrentMap().GetParser().GetEntityData(), "entities.binfo", m_gameInfo);

    m_skybox.Initialize(m_server.GetSkyName(), m_gameInfo);
    m_client->SetSpawnPoint(m_server.GetSpawnPoint());

    Logger::Info("Application: VEEX Engine Initialized Successfully.");
    return true;
}

void Application::MainLoop() {
    auto lastTime = std::chrono::high_resolution_clock::now();
    bool fWasPressed = false;
    bool escWasPressed = false;
    bool pauseWasVisible = false;
    
    while (!glfwWindowShouldClose(m_window)) {
        auto now = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        if (frameTime > 0.1f) frameTime = 0.1f;

        glfwPollEvents();

        // Handle Sound Test Key (F)
        if (glfwGetKey(m_window, GLFW_KEY_F) == GLFW_PRESS) {
            if (!fWasPressed) {
                Logger::Info("Input: F key pressed. Triggering test sound...");
                SoundKit::Get().PlayOneShot("sound/test.wav", m_client->GetCamera().GetPosition());
                fWasPressed = true;
            }
        } else {
            fWasPressed = false;
        }

        // Handle Pause Menu (Escape key) with debounce
        bool escIsPressed = (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS);
        if (escIsPressed && !escWasPressed) {
            TogglePauseMenu(m_window);
        }
        escWasPressed = escIsPressed;

        // Track pause menu state transition
        bool pauseIsVisible = IsPauseMenuVisible();
        if (pauseWasVisible && !pauseIsVisible) {
            // Pause menu was just hidden - reset mouse tracking to prevent view jump
            m_client->ResetMouseTracking();
        }
        pauseWasVisible = pauseIsVisible;

        // Update pause menu
        UpdatePauseMenu(frameTime);

        m_server.Tick(m_fixedTick);

        if (m_client) {
            // Only handle mouse look when pause menu is not visible to prevent view direction reset
            if (!IsPauseMenuVisible()) {
                m_client->HandleMouseLook(frameTime);
            }
            
            // Sync audio listener and process spatial/occlusion logic
            SoundKit::Get().Update(
                m_client->GetCamera().GetPosition(), 
                m_client->GetCamera().GetForward(), 
                &m_client->GetCurrentMap()
            );

            m_client->Render(m_server, m_skybox);

            // Render GUI on top of the scene (after main rendering)
            GUI::Get().Render();
        }

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