// veex/NanoGUIAdapter.cpp
// Adapter layer to bridge between custom GUI system and NanoGUI

#include "veex/NanoGUIAdapter.h"
#include "veex/GUI.h"
#include "veex/Logger.h"
#include <nanogui/nanogui.h>
#include <GLFW/glfw3.h>

namespace veex {

NanoGUIAdapter::NanoGUIAdapter() {}

NanoGUIAdapter::~NanoGUIAdapter() {
    Shutdown();
}

bool NanoGUIAdapter::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Get the current GLFW window
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) {
        Logger::Error("[NanoGUIAdapter] No GLFW context available for NanoGUI initialization");
        return false;
    }

    // Initialize NanoGUI with the existing GLFW window
    try {
        // NanoGUI will take ownership of the OpenGL context
        // We need to be careful not to conflict with our existing OpenGL usage
        m_screen = std::make_unique<nanogui::Screen>();
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        
        // Initialize the screen with the existing window
        m_screen->initialize(window, true);
        m_screen->setSize(width, height);
        
        // Create a custom theme if needed
        m_theme = std::make_unique<nanogui::Theme>(m_screen->nvgContext());
        m_screen->setTheme(m_theme.get());
        
        m_initialized = true;
        Logger::Info("[NanoGUIAdapter] NanoGUI initialized successfully");
        
        return true;
    } catch (const std::exception& e) {
        Logger::Error("[NanoGUIAdapter] Failed to initialize NanoGUI: " + std::string(e.what()));
        return false;
    }
}

void NanoGUIAdapter::Shutdown() {
    if (!m_initialized) {
        return;
    }

    try {
        m_screen.reset();
        m_theme.reset();
        m_initialized = false;
        Logger::Info("[NanoGUIAdapter] NanoGUI shutdown complete");
    } catch (const std::exception& e) {
        Logger::Error("[NanoGUIAdapter] Error during shutdown: " + std::string(e.what()));
    }
}

void NanoGUIAdapter::DrawAll() {
    if (!m_initialized || !m_visible || !m_screen) {
        return;
    }

    try {
        m_screen->drawContents();
        m_screen->drawWidgets();
    } catch (const std::exception& e) {
        Logger::Error("[NanoGUIAdapter] Error during drawing: " + std::string(e.what()));
    }
}

bool NanoGUIAdapter::ShouldClose() const {
    return !m_initialized || !m_screen;
}

void NanoGUIAdapter::SetSize(int width, int height) {
    if (m_screen) {
        m_screen->setSize(width, height);
    }
}

void NanoGUIAdapter::SetTitle(const std::string& title) {
    if (m_screen) {
        m_screen->caption() = title;
    }
}

bool NanoGUIAdapter::HandleMouseClick(float x, float y, int button) {
    if (!m_initialized || !m_screen) {
        return false;
    }

    try {
        // NanoGUI uses its own coordinate system
        // We need to convert from our coordinates to NanoGUI coordinates
        int width, height;
        glfwGetWindowSize(m_screen->glfwWindow(), &width, &height);
        
        // Convert coordinates if needed
        double cursorX, cursorY;
        glfwGetCursorPos(m_screen->glfwWindow(), &cursorX, &cursorY);
        
        // Let NanoGUI handle the mouse button event
        return m_screen->mouseButtonCallbackEvent(button, GLFW_PRESS, 0);
    } catch (const std::exception& e) {
        Logger::Error("[NanoGUIAdapter] Error handling mouse click: " + std::string(e.what()));
        return false;
    }
}

bool NanoGUIAdapter::HandleMouseMove(float x, float y) {
    if (!m_initialized || !m_screen) {
        return false;
    }

    try {
        double cursorX, cursorY;
        glfwGetCursorPos(m_screen->glfwWindow(), &cursorX, &cursorY);
        
        m_cursorX = static_cast<float>(cursorX);
        m_cursorY = static_cast<float>(cursorY);
        
        return m_screen->cursorPosCallbackEvent(cursorX, cursorY);
    } catch (const std::exception& e) {
        Logger::Error("[NanoGUIAdapter] Error handling mouse move: " + std::string(e.what()));
        return false;
    }
}

bool NanoGUIAdapter::HandleMouseScroll(float x, float y) {
    if (!m_initialized || !m_screen) {
        return false;
    }

    try {
        return m_screen->scrollCallbackEvent(x, y);
    } catch (const std::exception& e) {
        Logger::Error("[NanoGUIAdapter] Error handling mouse scroll: " + std::string(e.what()));
        return false;
    }
}

bool NanoGUIAdapter::HandleKeyDown(int key, int scancode, int action, int mods) {
    if (!m_initialized || !m_screen) {
        return false;
    }

    try {
        return m_screen->keyCallbackEvent(key, scancode, action, mods);
    } catch (const std::exception& e) {
        Logger::Error("[NanoGUIAdapter] Error handling key down: " + std::string(e.what()));
        return false;
    }
}

bool NanoGUIAdapter::HandleCharInput(unsigned int codepoint) {
    if (!m_initialized || !m_screen) {
        return false;
    }

    try {
        return m_screen->charCallbackEvent(codepoint);
    } catch (const std::exception& e) {
        Logger::Error("[NanoGUIAdapter] Error handling char input: " + std::string(e.what()));
        return false;
    }
}

std::unique_ptr<GUIElement> NanoGUIAdapter::CreatePanel() {
    // For now, return a basic GUIElement
    // In a full implementation, this would create a NanoGUI widget
    return std::make_unique<GUIElement>(GUIElementType::Panel);
}

std::unique_ptr<GUIElement> NanoGUIAdapter::CreateButton(const std::string& text) {
    // For now, return a basic GUIElement
    // In a full implementation, this would create a NanoGUI Button widget
    auto button = std::make_unique<GUIElement>(GUIElementType::Button);
    button->text = text;
    return button;
}

std::unique_ptr<GUIElement> NanoGUIAdapter::CreateLabel(const std::string& text) {
    // For now, return a basic GUIElement
    // In a full implementation, this would create a NanoGUI Label widget
    auto label = std::make_unique<GUIElement>(GUIElementType::Label);
    label->text = text;
    return label;
}

std::unique_ptr<GUIElement> NanoGUIAdapter::CreateImage(const std::string& texturePath) {
    // For now, return a basic GUIElement
    // In a full implementation, this would create a NanoGUI ImageView widget
    auto image = std::make_unique<GUIElement>(GUIElementType::Image);
    image->texturePath = texturePath;
    return image;
}

std::unique_ptr<GUIElement> NanoGUIAdapter::CreateTextBox(const std::string& placeholder) {
    // For now, return a basic GUIElement
    // In a full implementation, this would create a NanoGUI TextBox widget
    auto textbox = std::make_unique<GUIElement>(GUIElementType::TextBox);
    textbox->placeholder = placeholder;
    return textbox;
}

std::unique_ptr<GUIElement> NanoGUIAdapter::CreateSlider(float min, float max) {
    // For now, return a basic GUIElement
    // In a full implementation, this would create a NanoGUI Slider widget
    auto slider = std::make_unique<GUIElement>(GUIElementType::Slider);
    slider->minValue = min;
    slider->maxValue = max;
    return slider;
}

std::unique_ptr<GUIElement> NanoGUIAdapter::CreateCheckBox() {
    // For now, return a basic GUIElement
    // In a full implementation, this would create a NanoGUI CheckBox widget
    auto checkbox = std::make_unique<GUIElement>(GUIElementType::CheckBox);
    return checkbox;
}

std::unique_ptr<GUIElement> NanoGUIAdapter::CreateProgressBar() {
    // For now, return a basic GUIElement
    // In a full implementation, this would create a NanoGUI ProgressBar widget
    auto progress = std::make_unique<GUIElement>(GUIElementType::ProgressBar);
    return progress;
}

void NanoGUIAdapter::SetVisible(bool visible) {
    m_visible = visible;
    if (m_screen) {
        m_screen->setVisible(visible);
    }
}

bool NanoGUIAdapter::IsVisible() const {
    return m_visible;
}

void NanoGUIAdapter::SetCursorPosition(float x, float y) {
    m_cursorX = x;
    m_cursorY = y;
}

float NanoGUIAdapter::GetCursorX() const {
    return m_cursorX;
}

float NanoGUIAdapter::GetCursorY() const {
    return m_cursorY;
}

GUIElement* NanoGUIAdapter::GetFocusedElement() const {
    // For now, return nullptr
    // In a full implementation, this would return the focused NanoGUI widget
    return nullptr;
}

void NanoGUIAdapter::SetTheme(nanogui::Theme* theme) {
    if (m_screen) {
        m_screen->setTheme(theme);
    }
}

nanogui::Theme* NanoGUIAdapter::GetTheme() const {
    if (m_screen) {
        return m_screen->theme();
    }
    return nullptr;
}

nanogui::Screen* NanoGUIAdapter::GetScreen() const {
    return m_screen.get();
}

} // namespace veex