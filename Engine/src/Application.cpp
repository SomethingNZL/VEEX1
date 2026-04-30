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
    Logger::Info("Application: Starting initialization...");
    if (!glfwInit()) {
        Logger::Error("Application: glfwInit failed");
        return false;
    }
    Logger::Info("Application: glfwInit succeeded");

    // Try Core 3.3 first
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    
    Logger::Info("Application: Creating window with Core 3.3...");
    m_window = glfwCreateWindow(1280, 720, "VEEX Engine", NULL, NULL);
    
    if (!m_window) {
        Logger::Warn("Application: Core 3.3 failed, trying default hints...");
        // Reset all hints to defaults and try again
        glfwDefaultWindowHints();
        m_window = glfwCreateWindow(1280, 720, "VEEX Engine", NULL, NULL);
    }
    
    if (!m_window) {
        Logger::Error("Application: glfwCreateWindow failed with all profiles");
        return false;
    }
    
    if (!m_window) {
        Logger::Error("Application: glfwCreateWindow failed");
        return false;
    }
    Logger::Info("Application: Window created successfully");

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

    Logger::Info("Initializing MaterialSystem...");
    MaterialSystem::Get().Initialize(m_gameInfo);
    Logger::Info("MaterialSystem initialized.");

    // Initialize GUI system after OpenGL context is created
    Logger::Info("Initializing GUI...");
    GUI::Get().Initialize();
    Logger::Info("GUI initialized.");
    
    // Set GUI to start hidden
    GUI::Get().SetVisible(false);
    
    // Initialize pause menu FIRST (before main menu)
    std::string gameTitle = m_gameInfo.gameName.empty() ? "VEEX Game" : m_gameInfo.gameName;
    InitializePauseMenu(gameTitle);
    
    // Ensure pause menu starts hidden
    HidePauseMenu(m_window);
    
    // Initialize main menu
    MainMenu::Get().Initialize(m_gameInfo);
    
    // Load background map for main menu (use maps/ prefix as per search paths)
    LoadBackgroundMap("maps/background01.bsp");
    
    // Show the main menu (this will set GUI visible to true)
    MainMenu::Get().Show(m_window);
    
    // Show mouse cursor for main menu
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    
    // Set up chapter select callback
    MainMenu::Get().SetChapterSelectCallback([this](const std::string& mapFile) {
        LoadMap(mapFile);
    });

    // Don't initialize client yet - wait for chapter selection
    m_client = nullptr;

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

        // Handle main menu vs game input
        if (m_showMainMenu) {
            HandleMainMenuInput();
        } else {
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
        }

        glfwSwapBuffers(m_window);
    }
}

void Application::Shutdown() {
    Logger::Info("Application: Shutting down...");
    MainMenu::Get().Shutdown();
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

void Application::LoadMap(const std::string& mapFile) {
    Logger::Info("Application: Loading map: " + mapFile);
    
    // Resolve the map file path using filesystem tools
    std::string resolvedMapPath = ResolveAssetPath(mapFile, m_gameInfo);
    if (resolvedMapPath.empty()) {
        Logger::Error("Application: Failed to resolve map path: " + mapFile);
        return;
    }
    
    Logger::Info("Application: Resolved map path to: " + resolvedMapPath);
    
    // Initialize client with the selected map
    m_client = new Client(m_window);
    if (!m_client->Init(m_gameInfo, resolvedMapPath)) {
        Logger::Error("Application: Failed to initialize client with map: " + mapFile);
        return;
    }
    
    // Initialize server and skybox
    m_server.Init(m_client->GetCurrentMap().GetParser().GetEntityData(), "entities.binfo", m_gameInfo);
    m_skybox.Initialize(m_server.GetSkyName(), m_gameInfo);
    m_client->SetSpawnPoint(m_server.GetSpawnPoint());
    
    // Hide main menu and show game
    m_showMainMenu = false;
    MainMenu::Get().Hide(m_window);
    
    // Hide and lock mouse cursor for gameplay
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    Logger::Info("Application: Map loaded successfully: " + mapFile);
}

void Application::LoadBackgroundMap(const std::string& mapFile) {
    Logger::Info("Application: Loading background map for main menu: " + mapFile);
    
    // Try to load the background map
    std::string mapPath = ResolveAssetPath(mapFile, m_gameInfo);
    if (mapPath.empty()) {
        Logger::Warn("Application: Background map not found: " + mapFile);
        return;
    }
    
    // Create a temporary client just to load and render the background
    // We don't initialize the full game systems - just load the BSP for rendering
    m_client = new Client(m_window);
    if (!m_client->Init(m_gameInfo, mapPath)) {
        Logger::Warn("Application: Failed to load background map: " + mapFile);
        delete m_client;
        m_client = nullptr;
        return;
    }
    
    Logger::Info("Application: Background map loaded: " + mapFile);
}

void Application::HandleMainMenuInput() {
    // Start ImGui frame for main menu
    GUI::Get().NewFrame();
    
    // Update main menu
    MainMenu::Get().Update(0.016f); // 60 FPS update
    
    // Render main menu (blank background + UI)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black background for main menu
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Render main menu UI (this will call ImGui functions)
    MainMenu::Get().Render();
    
    // Render ImGui draw data
    GUI::Get().Render();
}

} // namespace veex