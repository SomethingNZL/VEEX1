#pragma once
#include "veex/Config.h"
#include "veex/Client.h"
#include "veex/Server.h"
#include "veex/Skybox.h"
#include "veex/MainMenu.h"
#include <string>

struct GLFWwindow;

namespace veex {

class Application {
public:
    // Match the constructor exactly to what main.cpp uses
    Application(const std::string& gameInfoPath, const std::string& gameInfoBinPath = "");
    ~Application() = default;

    int Run();

private:
    bool Initialize();
    void MainLoop();
    void Shutdown();
    void LoadMap(const std::string& mapFile);
    void LoadBackgroundMap(const std::string& mapFile);
    void HandleMainMenuInput();

    std::string m_gameInfoPath;
    std::string m_gameInfoBinPath;

    GLFWwindow* m_window;
    GameInfo m_gameInfo;
    
    Client* m_client;
    Server m_server;
    Skybox m_skybox;

    bool m_showMainMenu = true;  // Start with main menu visible

    float m_fixedTick = 0.015f; 
    bool m_escWasPressed = false;
};

} // namespace veex