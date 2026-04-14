// veex/PauseMenu.cpp
// Pause menu implementation using the internal GUI system.

#include "veex/GUI.h"
#include "veex/Logger.h"
#include "veex/GameInfo.h"
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

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
        
        // Create the pause menu root panel
        auto root = GUI::Get().CreatePanel();
        root->id = "pause_menu_root";
        root->SetSize(800, 600);
        root->SetAnchor(GUIAnchor::MiddleCenter);
        root->style.backgroundColor = GUIColor(0.0f, 0.0f, 0.0f, 0.8f); // Dark transparent background
        root->style.borderColor = GUIColor(1.0f, 0.4f, 0.0f, 1.0f);   // HL2 orange border
        root->style.borderWidth = 4.0f;
        root->style.paddingLeft = 40.0f;
        root->style.paddingRight = 40.0f;
        root->style.paddingTop = 40.0f;
        root->style.paddingBottom = 40.0f;
        
        // Create title (using game title from gameinfo.txt)
        auto title = GUI::Get().CreateLabel(m_gameTitle);
        title->style.fontSize = 48.0f;
        title->style.fontBold = true;
        title->style.textColor = GUIColor::Orange();
        title->style.textAlign = GUIAlign::TopCenter;
        title->SetSize(720, 60);
        title->SetAnchor(GUIAnchor::TopCenter);
        
        // Create subtitle
        auto subtitle = GUI::Get().CreateLabel("PAUSED");
        subtitle->style.fontSize = 24.0f;
        subtitle->style.textColor = GUIColor::White();
        subtitle->style.textAlign = GUIAlign::TopCenter;
        subtitle->SetSize(720, 30);
        subtitle->SetAnchor(GUIAnchor::TopCenter);
        subtitle->SetPosition(0, 70);
        
        // Create main menu container (flex column)
        auto menuContainer = GUI::Get().CreatePanel();
        menuContainer->style.isFlexContainer = true;
        menuContainer->style.flexHorizontal = false;
        menuContainer->style.flexSpacing = 20.0f;
        menuContainer->style.flexAlign = GUIAlign::MiddleCenter;
        menuContainer->SetSize(400, 300);
        menuContainer->SetAnchor(GUIAnchor::MiddleCenter);
        
        // Create menu buttons
        auto resumeButton = CreateMenuButton("RESUME GAME", []() {
            Logger::Info("[PauseMenu] Resume Game clicked");
            PauseMenu::Get().Hide();
        });
        
        auto saveButton = CreateMenuButton("SAVE GAME", []() {
            Logger::Info("[PauseMenu] Save Game clicked");
        });
        
        auto loadButton = CreateMenuButton("LOAD GAME", []() {
            Logger::Info("[PauseMenu] Load Game clicked");
        });
        
        auto optionsButton = CreateMenuButton("OPTIONS", []() {
            Logger::Info("[PauseMenu] Options clicked");
        });
        
        auto quitButton = CreateMenuButton("QUIT TO WINDOWS", []() {
            Logger::Info("[PauseMenu] Quit to Windows clicked");
            // In a real implementation, this would exit the game
        });
        
        // Add buttons to container
        menuContainer->AddChild(std::move(resumeButton));
        menuContainer->AddChild(std::move(saveButton));
        menuContainer->AddChild(std::move(loadButton));
        menuContainer->AddChild(std::move(optionsButton));
        menuContainer->AddChild(std::move(quitButton));
        
        // Create bottom hint
        auto hint = GUI::Get().CreateLabel("Press ESC to resume");
        hint->style.fontSize = 14.0f;
        hint->style.textColor = GUIColor(0.7f, 0.7f, 0.7f, 1.0f);
        hint->style.textAlign = GUIAlign::BottomCenter;
        hint->SetSize(720, 20);
        hint->SetAnchor(GUIAnchor::BottomCenter);
        hint->SetPosition(0, -50);
        
        // Add all elements to root
        root->AddChild(std::move(title));
        root->AddChild(std::move(subtitle));
        root->AddChild(std::move(menuContainer));
        root->AddChild(std::move(hint));
        
        // Set as GUI root
        GUI::Get().SetRoot(std::move(root));
        
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
        
        GUI::Get().SetVisible(true);
        m_visible = true;
    }
    
    void Hide(GLFWwindow* window = nullptr) {
        Logger::Info("[PauseMenu] Hiding pause menu");
        GUI::Get().SetVisible(false);
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
        if (m_visible) {
            GUI::Get().Update(deltaTime);
        }
    }
    
    void Render() {
        if (m_visible) {
            GUI::Get().Render();
        }
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
    
    std::unique_ptr<GUIElement> CreateMenuButton(const std::string& text, std::function<void()> onClick) {
        auto button = GUI::Get().CreateButton(text);
        button->style.fontSize = 20.0f;
        button->style.fontBold = true;
        button->style.backgroundColor = GUIColor(0.1f, 0.1f, 0.1f, 1.0f);
        button->style.textColor = GUIColor::White();
        button->style.hoverColor = GUIColor(0.2f, 0.2f, 0.2f, 1.0f);
        button->style.borderColor = GUIColor(1.0f, 0.4f, 0.0f, 1.0f);
        button->style.borderWidth = 2.0f;
        button->style.paddingLeft = 20.0f;
        button->style.paddingRight = 20.0f;
        button->style.paddingTop = 10.0f;
        button->style.paddingBottom = 10.0f;
        button->SetSize(300, 40);
        
        // Set up click handler
        button->onClick = [onClick](GUIElement* element, GUIEventType type) {
            if (type == GUIEventType::OnClick) {
                onClick();
            }
        };
        
        return button;
    }
    
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

void ShowPauseMenu(GLFWwindow* window = nullptr) {
    PauseMenu::Get().Show(window);
}

void HidePauseMenu(GLFWwindow* window = nullptr) {
    PauseMenu::Get().Hide(window);
}

void TogglePauseMenu(GLFWwindow* window = nullptr) {
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