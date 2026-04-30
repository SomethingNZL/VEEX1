// veex/PauseMenu.cpp
// Pause menu implementation using Dear ImGui.

#include "veex/PauseMenu.h"
#include "veex/Logger.h"
#include "veex/GameInfo.h"
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include "imgui.h"

namespace veex {

// ── Pause Menu ───────────────────────────────────────────────────────────
class PauseMenu {
public:
    static PauseMenu& Get() {
        static PauseMenu instance;
        return instance;
    }
    
    void Initialize(const std::string& gameTitle = "VEEX Game") {
        if (m_initialized) return;
        
        m_gameTitle = gameTitle;
        m_initialized = true;
        Logger::Info("[PauseMenu] Initialized");
    }
    
    void Show(GLFWwindow* window = nullptr) {
        if (!m_initialized) Initialize();
        Logger::Info("[PauseMenu] Showing pause menu");
        
        // Save current mouse position before enabling cursor
        if (window) {
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            m_lastMouseX = mouseX;
            m_lastMouseY = mouseY;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        
        m_visible = true;
    }
    
    void Hide(GLFWwindow* window = nullptr) {
        Logger::Info("[PauseMenu] Hiding pause menu");
        m_visible = false;
        
        // Disable cursor when pause menu is hidden
        if (window) {
            // Restore mouse position to prevent view jump
            glfwSetCursorPos(window, m_lastMouseX, m_lastMouseY);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    
    bool IsVisible() const {
        return m_visible;
    }
    
    void Toggle(GLFWwindow* window = nullptr) {
        Logger::Info("[PauseMenu] Toggling pause menu (current state: " + std::string(IsVisible() ? "visible" : "hidden") + ")");
        if (IsVisible()) {
            Hide(window);
        } else {
            Show(window);
        }
    }
    
    void Update(float deltaTime) {
        // ImGui handles its own update
    }
    
    void Render() {
        if (!m_visible) return;
        
        RenderImGuiMenu();
    }
    
    void RenderImGuiMenu() {
        ImGuiIO& io = ImGui::GetIO();
        float screenWidth = io.DisplaySize.x;
        float screenHeight = io.DisplaySize.y;
        
        // Semi-transparent dark overlay
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        drawList->AddRectFilled(ImVec2(0, 0), ImVec2(screenWidth, screenHeight), 
                               IM_COL32(0, 0, 0, 200));
        
        // Pause menu window
        float windowWidth = 500.0f;
        float windowHeight = 400.0f;
        float windowX = (screenWidth - windowWidth) / 2.0f;
        float windowY = (screenHeight - windowHeight) / 2.0f;
        
        ImGui::SetNextWindowPos(ImVec2(windowX, windowY));
        ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | 
                                 ImGuiWindowFlags_NoResize | 
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoSavedSettings;
        
        ImGui::Begin("Pause Menu", nullptr, flags);
        
        // Title - game title
        ImGui::SetCursorPos(ImVec2(20, 20));
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.0f, 1.0f), "%s", m_gameTitle.c_str());
        
        // Subtitle
        ImGui::SetCursorPos(ImVec2(20, 60));
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "PAUSED");
        
        // Menu buttons
        ImGui::SetCursorPos(ImVec2(50, 120));
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.4f, 0.0f, 1.0f));
        
        if (ImGui::Button("RESUME GAME", ImVec2(400, 50))) {
            Logger::Info("[PauseMenu] Resume Game clicked");
            Hide();
        }
        
        ImGui::Spacing();
        
        if (ImGui::Button("SAVE GAME", ImVec2(400, 50))) {
            Logger::Info("[PauseMenu] Save Game clicked");
        }
        
        ImGui::Spacing();
        
        if (ImGui::Button("LOAD GAME", ImVec2(400, 50))) {
            Logger::Info("[PauseMenu] Load Game clicked");
        }
        
        ImGui::Spacing();
        
        if (ImGui::Button("OPTIONS", ImVec2(400, 50))) {
            Logger::Info("[PauseMenu] Options clicked");
        }
        
        ImGui::Spacing();
        
        if (ImGui::Button("QUIT TO WINDOWS", ImVec2(400, 50))) {
            Logger::Info("[PauseMenu] Quit to Windows clicked");
        }
        
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
        
        // Bottom hint
        ImGui::SetCursorPos(ImVec2(20, windowHeight - 40));
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Press ESC to resume");
        
        ImGui::End();
    }
    
    // Input handling
    bool HandleKeyPress(int key, int scancode, int action, int mods, GLFWwindow* window = nullptr) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            Toggle(window);
            return true;
        }
        return false;
    }
    
private:
    PauseMenu() = default;
    ~PauseMenu() = default;
    PauseMenu(const PauseMenu&) = delete;
    PauseMenu& operator=(const PauseMenu&) = delete;
    
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    
    bool m_initialized = false;
    bool m_visible = false;
    std::string m_gameTitle = "VEEX Game";
};

// ── Global Functions ─────────────────────────────────────────────────────────

void InitializePauseMenu(const std::string& gameTitle) {
    PauseMenu::Get().Initialize(gameTitle);
}

void InitializePauseMenu() {
    PauseMenu::Get().Initialize();
}

void ShowPauseMenu(GLFWwindow* window) {
    PauseMenu::Get().Show(window);
}

void HidePauseMenu(GLFWwindow* window) {
    PauseMenu::Get().Hide(window);
}

void TogglePauseMenu(GLFWwindow* window) {
    PauseMenu::Get().Toggle(window);
}

bool IsPauseMenuVisible() {
    return PauseMenu::Get().IsVisible();
}

void UpdatePauseMenu(float deltaTime) {
    PauseMenu::Get().Update(deltaTime);
}

void RenderPauseMenu() {
    PauseMenu::Get().Render();
}

bool HandlePauseMenuInput(int key, int scancode, int action, int mods) {
    return PauseMenu::Get().HandleKeyPress(key, scancode, action, mods);
}

} // namespace veex