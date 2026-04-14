#pragma once
#include "veex/Config.h"
#include "veex/GameInfo.h"
#include <string>
#include <functional>
#include <vector>
#include <memory>

struct GLFWwindow;

namespace veex {

/**
 * GUI system using Dear ImGui (MIT License)
 * Provides a complete GUI framework for menus, buttons, text rendering, etc.
 */
class GUI {
public:
    static GUI& Get();
    
    bool Initialize();
    void Shutdown();
    void Render();
    
    // Window management
    void NewFrame();
    void SetVisible(bool visible);
    bool IsVisible() const { return m_visible; }
    
    // Input handling
    bool HandleMouseClick(float x, float y, int button);
    bool HandleMouseMove(float x, float y);
    bool HandleKeyDown(int key, int scancode, int action, int mods);
    bool HandleCharInput(unsigned int codepoint);
    bool HandleMouseButton(int button, int action, int mods);
    bool HandleScroll(float xoffset, float yoffset);
    
    // Window size change
    void OnWindowResize(int width, int height);
    
private:
    GUI();
    ~GUI();
    GUI(const GUI&) = delete;
    GUI& operator=(const GUI&) = delete;
    
    bool m_initialized = false;
    bool m_visible = true;
    GLFWwindow* m_window = nullptr;
};

} // namespace veex