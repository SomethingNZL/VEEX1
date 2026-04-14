#pragma once
// veex/NanoGUIAdapter.h
// Adapter layer to bridge between custom GUI system and NanoGUI
// This allows us to use NanoGUI while maintaining compatibility with existing code

#include <memory>
#include <functional>
#include <string>
#include <vector>

// Forward declarations for GLFWwindow
struct GLFWwindow;

namespace nanogui {
    class Screen;
    class Window;
    class Button;
    class Label;
    class TextBox;
    class Slider;
    class CheckBox;
    class ProgressBar;
    class Popup;
    class PopupButton;
    class ComboBox;
    class ImagePanel;
    class ImageView;
    class VScrollPanel;
    class ColorWheel;
    class ColorPicker;
    class Graph;
    class StackedWidget;
    class TabHeader;
    class TabWidget;
    class GLCanvas;
    class Theme;
}

namespace veex {

// Forward declarations
class GUIElement;
class GUIStyle;
class GUIColor;
class GUIRect;

// NanoGUI adapter class
class NanoGUIAdapter {
public:
    static NanoGUIAdapter& Get() {
        static NanoGUIAdapter instance;
        return instance;
    }

    // Initialize NanoGUI
    bool Initialize();
    
    // Shutdown NanoGUI
    void Shutdown();
    
    // Main loop integration
    void DrawAll();
    bool ShouldClose() const;
    
    // Window management
    void SetSize(int width, int height);
    void SetTitle(const std::string& title);
    
    // Input handling
    bool HandleMouseClick(float x, float y, int button);
    bool HandleMouseMove(float x, float y);
    bool HandleMouseScroll(float x, float y);
    bool HandleKeyDown(int key, int scancode, int action, int mods);
    bool HandleCharInput(unsigned int codepoint);
    
    // Element creation (mirrors existing GUI system)
    std::unique_ptr<GUIElement> CreatePanel();
    std::unique_ptr<GUIElement> CreateButton(const std::string& text = "");
    std::unique_ptr<GUIElement> CreateLabel(const std::string& text = "");
    std::unique_ptr<GUIElement> CreateImage(const std::string& texturePath = "");
    std::unique_ptr<GUIElement> CreateTextBox(const std::string& placeholder = "");
    std::unique_ptr<GUIElement> CreateSlider(float min = 0, float max = 1);
    std::unique_ptr<GUIElement> CreateCheckBox();
    std::unique_ptr<GUIElement> CreateProgressBar();
    
    // Visibility
    void SetVisible(bool visible);
    bool IsVisible() const;
    
    // Cursor position tracking
    void SetCursorPosition(float x, float y);
    float GetCursorX() const;
    float GetCursorY() const;
    
    // Focused element
    GUIElement* GetFocusedElement() const;
    
    // Theme management
    void SetTheme(nanogui::Theme* theme);
    nanogui::Theme* GetTheme() const;
    
    // Get the main screen
    nanogui::Screen* GetScreen() const;

private:
    NanoGUIAdapter();
    ~NanoGUIAdapter();
    NanoGUIAdapter(const NanoGUIAdapter&) = delete;
    NanoGUIAdapter& operator=(const NanoGUIAdapter&) = delete;
    
    // Internal NanoGUI objects
    std::unique_ptr<nanogui::Screen> m_screen;
    std::unique_ptr<nanogui::Theme> m_theme;
    
    bool m_initialized = false;
    bool m_visible = true;
    float m_cursorX = 0, m_cursorY = 0;
    
    // Root panel for the main menu
    std::unique_ptr<GUIElement> m_rootPanel;
};

} // namespace veex