#include "veex/GUI.h"
#include "veex/Logger.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>

// Dear ImGui includes
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

namespace veex {

GUI::GUI() {
}

GUI::~GUI() {
}

GUI& GUI::Get() {
    static GUI instance;
    return instance;
}

bool GUI::Initialize() {
    if (m_initialized) {
        Logger::Warn("GUI: Already initialized.");
        return false;
    }

    // Get GLFW window from global context (will be set by Application)
    m_window = glfwGetCurrentContext();
    if (!m_window) {
        Logger::Error("GUI: No OpenGL context available for initialization.");
        return false;
    }

    Logger::Info("[GUI] Initializing Dear ImGui...");

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 150"); // OpenGL 3.3 Core Profile

    // Let ImGui build the font atlas - the backend will handle texture creation
    // when we call NewFrame() for the first time

    m_initialized = true;
    Logger::Info("[GUI] Dear ImGui initialization complete!");
    return true;
}

void GUI::Shutdown() {
    if (!m_initialized) return;

    Logger::Info("[GUI] Shutting down Dear ImGui...");

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_initialized = false;
}

void GUI::NewFrame() {
    if (!m_initialized || !m_visible) return;
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GUI::Render() {
    if (!m_initialized || !m_visible) return;

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GUI::SetVisible(bool visible) {
    m_visible = visible;
    Logger::Info("[GUI] SetVisible called: " + std::to_string(visible));
}

bool GUI::HandleMouseClick(float x, float y, int button) {
    if (!m_initialized || !m_visible) return false;
    // ImGui handles mouse clicks through GLFW callbacks
    return ImGui::GetIO().WantCaptureMouse;
}

bool GUI::HandleMouseMove(float x, float y) {
    if (!m_initialized || !m_visible) return false;
    return ImGui::GetIO().WantCaptureMouse;
}

bool GUI::HandleKeyDown(int key, int scancode, int action, int mods) {
    if (!m_initialized || !m_visible) return false;
    ImGuiIO& io = ImGui::GetIO();
    // ImGui backend handles key events, but we can pass them through
    // The backend will handle this via GLFW callbacks
    return io.WantCaptureKeyboard;
}

bool GUI::HandleCharInput(unsigned int codepoint) {
    if (!m_initialized || !m_visible) return false;
    ImGui::GetIO().AddInputCharacter(codepoint);
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool GUI::HandleMouseButton(int button, int action, int mods) {
    if (!m_initialized || !m_visible) return false;
    ImGui::GetIO().MouseDown[button] = (action == GLFW_PRESS);
    return ImGui::GetIO().WantCaptureMouse;
}

bool GUI::HandleScroll(float xoffset, float yoffset) {
    if (!m_initialized || !m_visible) return false;
    ImGui::GetIO().MouseWheelH += xoffset;
    ImGui::GetIO().MouseWheel += yoffset;
    return ImGui::GetIO().WantCaptureMouse;
}

void GUI::OnWindowResize(int width, int height) {
    if (!m_initialized) return;
    ImGui::GetIO().DisplaySize = ImVec2((float)width, (float)height);
}

} // namespace veex