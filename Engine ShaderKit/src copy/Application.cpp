#include "veex/Application.h"
#include "veex/Config.h"
#include "veex/Logger.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <chrono>

namespace veex {

Application::Application(const std::string& gameInfoPath, const std::string& gameInfoBinPath)
    : m_gameInfoPath(gameInfoPath), m_gameInfoBinPath(gameInfoBinPath) {
}

int Application::Run() {
    if (!Initialize()) return -1;
    MainLoop();
    Shutdown();
    return 0;
}

bool Application::Initialize() {
    printf("\n--- INITIALIZING ---\n");

    if (!glfwInit()) {
        Logger::Error("Application: glfwInit() failed.");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    m_window = glfwCreateWindow(1280, 720, "VEEX Engine", NULL, NULL);
    if (!m_window) {
        Logger::Error("Application: Window creation failed.");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);

    if (!gladLoadGL(glfwGetProcAddress)) {
        Logger::Error("Application: GLAD loader failed.");
        return false;
    }

    Logger::Info("Application: OpenGL 3.3 context active.");
    glEnable(GL_DEPTH_TEST);

    // --- Load game info ---
    m_gameInfo.LoadFromFile(m_gameInfoPath);
    if (!m_gameInfoBinPath.empty())
        m_gameInfoBin.LoadFromFile(m_gameInfoBinPath);

    // --- Server init ---
    if (!m_server.Init()) {
        Logger::Error("Application: Server init failed.");
        return false;
    }

    // --- Client init ---
    m_client = new Client(m_window);
    if (!m_client->Init(m_gameInfo)) {
        Logger::Error("Application: Client init failed.");
        return false;
    }

    Logger::Info("Application: Initialization complete.");
    return true;
}

void Application::MainLoop() {
    printf("--- ENTERING MAIN LOOP ---\n");

    using clock = std::chrono::high_resolution_clock;
    auto lastTime = clock::now();
    double accumulator = 0.0;

    while (!glfwWindowShouldClose(m_window)) {
        auto now = clock::now();
        double frameTime = std::chrono::duration<double>(now - lastTime).count();
        lastTime = now;

        // Guard against spiral of death if a frame takes too long
        if (frameTime > 0.25) frameTime = 0.25;
        accumulator += frameTime;

        glfwPollEvents();

        if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(m_window, true);

        // Fixed-step server ticks (60 Hz)
        while (accumulator >= m_fixedTick) {
            m_server.Tick(m_fixedTick);
            accumulator -= m_fixedTick;
        }

        // Client renders as fast as possible
        if (m_client) {
            m_client->HandleMouseLook(static_cast<float>(frameTime));
            m_client->Render();
        }

        glfwSwapBuffers(m_window);
    }
}

void Application::Shutdown() {
    printf("--- SHUTTING DOWN ---\n");

    m_server.Shutdown();

    if (m_client) {
        m_client->Shutdown();
        delete m_client;
        m_client = nullptr;
    }

    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

} // namespace veex
