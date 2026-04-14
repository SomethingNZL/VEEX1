// veex/GUI.cpp
// Internal HTML/CSS GUI system implementation.

#include "veex/GUI.h"
#include "veex/Shader.h"
#include "veex/Texture.h"
#include "veex/Logger.h"
#include "veex/Config.h"
#include "veex/FileSystem.h"
#include "veex/GameInfo.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace veex {

// ── GUIColor ─────────────────────────────────────────────────────────────────

GUIColor GUIColor::FromHex(const std::string& hex) {
    std::string h = hex;
    if (h.empty()) return White();
    
    // Remove # prefix if present
    if (h[0] == '#') h = h.substr(1);
    
    // Parse hex color
    unsigned int r = 0, g = 0, b = 0, a = 255;
    
    if (h.length() == 6) {
        std::stringstream ss;
        ss << std::hex << h.substr(0, 2); ss >> r;
        ss.clear(); ss << std::hex << h.substr(2, 2); ss >> g;
        ss.clear(); ss << std::hex << h.substr(4, 2); ss >> b;
    } else if (h.length() == 8) {
        std::stringstream ss;
        ss << std::hex << h.substr(0, 2); ss >> r;
        ss.clear(); ss << std::hex << h.substr(2, 2); ss >> g;
        ss.clear(); ss << std::hex << h.substr(4, 2); ss >> b;
        ss.clear(); ss << std::hex << h.substr(6, 2); ss >> a;
    }
    
    return GUIColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

// ── GUIElement ───────────────────────────────────────────────────────────────

GUIElement::GUIElement(GUIElementType t) : type(t) {}

GUIElement::~GUIElement() {
    children.clear();
}

GUIElement* GUIElement::AddChild(std::unique_ptr<GUIElement> child) {
    child->parent = this;
    GUIElement* ptr = child.get();
    children.push_back(std::move(child));
    return ptr;
}

GUIElement* GUIElement::FindById(const std::string& searchId) {
    if (id == searchId) return this;
    for (auto& child : children) {
        if (GUIElement* found = child->FindById(searchId))
            return found;
    }
    return nullptr;
}

GUIElement* GUIElement::FindByTag(const std::string& searchTag) {
    if (tag == searchTag) return this;
    for (auto& child : children) {
        if (GUIElement* found = child->FindByTag(searchTag))
            return found;
    }
    return nullptr;
}

void GUIElement::SetPosition(float x, float y) {
    rect.x = x;
    rect.y = y;
}

void GUIElement::SetSize(float w, float h) {
    rect.w = w;
    rect.h = h;
}

void GUIElement::SetAnchor(GUIAnchor a) {
    anchor = a;
}

void GUIElement::SetText(const std::string& t) {
    text = t;
}

void GUIElement::SetVisible(bool v) {
    style.visible = v;
}

void GUIElement::SetEnabled(bool e) {
    style.enabled = e;
}

GUIRect GUIElement::GetScreenBounds() const {
    if (!parent) return rect;
    GUIRect parentBounds = parent->GetScreenBounds();
    return GUIRect{
        parentBounds.x + rect.x,
        parentBounds.y + rect.y,
        rect.w,
        rect.h
    };
}

void GUIElement::Update(float deltaTime) {
    (void)deltaTime;
    for (auto& child : children) {
        child->Update(deltaTime);
    }
}

void GUIElement::Layout(const GUIRect& parentRect) {
    if (!style.visible) {
        rect.w = 0;
        rect.h = 0;
        return;
    }
    
    // Apply anchor
    switch (anchor) {
        case GUIAnchor::TopCenter:
            rect.x = parentRect.w / 2.0f - rect.w / 2.0f;
            break;
        case GUIAnchor::TopRight:
            rect.x = parentRect.w - rect.w;
            break;
        case GUIAnchor::MiddleLeft:
            rect.y = parentRect.h / 2.0f - rect.h / 2.0f;
            break;
        case GUIAnchor::MiddleCenter:
            rect.x = parentRect.w / 2.0f - rect.w / 2.0f;
            rect.y = parentRect.h / 2.0f - rect.h / 2.0f;
            break;
        case GUIAnchor::MiddleRight:
            rect.x = parentRect.w - rect.w;
            rect.y = parentRect.h / 2.0f - rect.h / 2.0f;
            break;
        case GUIAnchor::BottomLeft:
            rect.y = parentRect.h - rect.h;
            break;
        case GUIAnchor::BottomCenter:
            rect.x = parentRect.w / 2.0f - rect.w / 2.0f;
            rect.y = parentRect.h - rect.h;
            break;
        case GUIAnchor::BottomRight:
            rect.x = parentRect.w - rect.w;
            rect.y = parentRect.h - rect.h;
            break;
        case GUIAnchor::TopLeft:
        default:
            break;
    }
    
    // Apply margins
    rect.x += style.marginLeft;
    rect.y += style.marginTop;
    rect.w -= style.marginLeft + style.marginRight;
    rect.h -= style.marginTop + style.marginBottom;
    
    // Layout children
    GUIRect childParentRect{
        style.paddingLeft,
        style.paddingTop,
        rect.w - style.paddingLeft - style.paddingRight,
        rect.h - style.paddingTop - style.paddingBottom
    };
    
    if (style.isFlexContainer) {
        LayoutFlex(childParentRect);
    } else {
        LayoutStandard(childParentRect);
    }
}

void GUIElement::LayoutFlex(const GUIRect& parentRect) {
    float x = parentRect.x;
    float y = parentRect.y;
    
    for (auto& child : children) {
        if (!child->style.visible) continue;
        
        // Layout child first to get its size
        child->Layout(parentRect);
        
        // Position child
        if (style.flexHorizontal) {
            child->rect.x = x;
            x += child->rect.w + style.flexSpacing;
        } else {
            child->rect.y = y;
            y += child->rect.h + style.flexSpacing;
        }
    }
}

void GUIElement::LayoutStandard(const GUIRect& parentRect) {
    for (auto& child : children) {
        if (!child->style.visible) continue;
        child->Layout(parentRect);
    }
}

bool GUIElement::HandleMouseClick(float x, float y) {
    if (!style.visible || !style.enabled) return false;
    
    // Check children first (reverse order for z-order)
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if ((*it)->HandleMouseClick(x, y)) return true;
    }
    
    // Check self
    if (rect.Contains(x, y)) {
        if (type == GUIElementType::Button && onClick) {
            onClick(this, GUIEventType::OnClick);
            return true;
        }
        return true;
    }
    
    return false;
}

bool GUIElement::HandleMouseMove(float x, float y) {
    if (!style.visible) return false;
    
    bool wasHovered = hovered;
    hovered = rect.Contains(x, y);
    
    if (hovered && !wasHovered && onHover) {
        onHover(this, GUIEventType::OnHover);
    } else if (!hovered && wasHovered && onLeave) {
        onLeave(this, GUIEventType::OnLeave);
    }
    
    // Check children
    for (auto& child : children) {
        if (child->HandleMouseMove(x, y)) return true;
    }
    
    return hovered;
}

bool GUIElement::HandleKeyDown(int key, int scancode, int action, int mods) {
    if (!style.visible || !style.enabled) return false;
    
    // Note: onKeyDown callback is not a member, using focused state
    if (focused) {
        return true;
    }
    
    for (auto& child : children) {
        if (child->HandleKeyDown(key, scancode, action, mods)) return true;
    }
    
    return false;
}

bool GUIElement::HandleCharInput(unsigned int codepoint) {
    if (!style.visible || !style.enabled) return false;
    
    if (focused && onTextChanged) {
        onTextChanged(this, GUIEventType::OnTextChanged);
        return true;
    }
    
    for (auto& child : children) {
        if (child->HandleCharInput(codepoint)) return true;
    }
    
    return false;
}

// ── GUI System ───────────────────────────────────────────────────────────────

GUI::GUI() {}

GUI::~GUI() {
    Shutdown();
}

void GUI::Initialize() {
    // Check if GLAD is loaded before calling OpenGL functions
    if (glad_glGenVertexArrays == nullptr || glad_glGenBuffers == nullptr) {
        Logger::Error("[GUI] OpenGL function pointers not loaded - cannot initialize GUI");
        return;
    }

    // Create root panel
    m_root = std::make_unique<GUIElement>(GUIElementType::Panel);
    m_root->id = "root";
    
    // Initialize OpenGL resources for rendering
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    
    Logger::Info("[GUI] Initializing OpenGL resources...");
    
    // Create the GUI shader from files
    GameInfo dummyGameInfo; // Use default paths
    std::string vertPath = ResolveAssetPath("shaders/gui.vert", dummyGameInfo);
    std::string fragPath = ResolveAssetPath("shaders/gui.frag", dummyGameInfo);
    
    Logger::Info("[GUI] Looking for shader files at: " + vertPath);
    
    m_shader = std::make_unique<Shader>();
    if (!m_shader->LoadFromFiles(vertPath, fragPath)) {
        Logger::Error("[GUI] Failed to compile GUI shader! Paths: " + vertPath + " and " + fragPath);
    } else {
        Logger::Info("[GUI] GUI shader compiled successfully!");
    }
    
    Logger::Info("[GUI] Initialization complete");
}

void GUI::Shutdown() {
    // Add NULL checks to prevent crashes if OpenGL context is invalid
    if (m_vao && glad_glDeleteVertexArrays) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo && glad_glDeleteBuffers) glDeleteBuffers(1, &m_vbo);
    m_vao = 0;
    m_vbo = 0;
    m_root.reset();
    Logger::Info("[GUI] Shutdown");
}

void GUI::SetRoot(std::unique_ptr<GUIElement> root) {
    m_root = std::move(root);
}

void GUI::Update(float deltaTime) {
    if (!m_visible || !m_root) return;
    m_root->Update(deltaTime);
}

void GUI::Render() {
    if (!m_visible || !m_root) return;
    
    // Get window size for orthographic projection
    int width, height;
    GLFWwindow* currentContext = glfwGetCurrentContext();
    if (!currentContext) {
        Logger::Error("[GUI] No current GLFW context available for rendering!");
        return;
    }
    glfwGetWindowSize(currentContext, &width, &height);
    
    Logger::Info("[GUI] Rendering with window size: " + std::to_string(width) + "x" + std::to_string(height));
    
    // Check if OpenGL context is valid
    if (!glad_glGenVertexArrays || !glad_glGenBuffers || !glad_glBindVertexArray) {
        Logger::Error("[GUI] OpenGL context not properly initialized!");
        return;
    }
    
    // Query current OpenGL state that we'll modify
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    
    // Disable depth test so GUI renders on top of everything
    glDisable(GL_DEPTH_TEST);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Bind the GUI shader
    if (m_shader && m_shader->IsValid()) {
        m_shader->Bind();
        
        // Set up orthographic projection matrix
        glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f);
        m_shader->SetUniform("u_Projection", glm::value_ptr(projection));
        
        // Call Layout before rendering to ensure proper positioning
        GUIRect screenRect{0, 0, (float)width, (float)height};
        m_root->Layout(screenRect);
        
        // Render the element tree
        RenderElement(m_root.get());
        
        m_shader->Unbind();
        Logger::Info("[GUI] Successfully rendered GUI elements");
    } else {
        Logger::Error("[GUI] Shader not available for rendering!");
    }
    
    // Restore OpenGL state
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
    
    if (blendEnabled) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
    
    // Restore previous shader program
    if (currentProgram != 0) {
        glUseProgram(currentProgram);
    }
}

void GUI::RenderElement(GUIElement* element) {
    if (!element || !element->style.visible) return;
    
    // Render background
    if (element->style.backgroundColor.a > 0) {
        RenderQuad(element->rect, element->style.backgroundColor);
    }
    
    // Render border
    if (element->style.borderWidth > 0 && element->style.borderColor.a > 0) {
        RenderBorder(element->rect, element->style);
    }
    
    // Render text
    if (!element->text.empty()) {
        RenderText(element->text, element->rect, element->style);
    }
    
    // Render children
    for (auto& child : element->children) {
        RenderElement(child.get());
    }
}

void GUI::RenderQuad(const GUIRect& rect, const GUIColor& color) {
    // Simple quad rendering with OpenGL
    // In a full implementation, we'd batch these for efficiency
    
    // Check if VAO and VBO are valid
    if (m_vao == 0 || m_vbo == 0) {
        Logger::Error("[GUI] VAO or VBO not initialized in RenderQuad!");
        return;
    }
    
    float vertices[] = {
        // positions          // colors
        rect.x, rect.y,       color.r, color.g, color.b, color.a,
        rect.x + rect.w, rect.y, color.r, color.g, color.b, color.a,
        rect.x + rect.w, rect.y + rect.h, color.r, color.g, color.b, color.a,
        rect.x, rect.y + rect.h, color.r, color.g, color.b, color.a
    };
    
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    
    // Set up vertex attributes
    // Position (2 floats)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    // Color (4 floats)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    
    // Draw quad
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    glBindVertexArray(0);
}

void GUI::RenderBorder(const GUIRect& rect, const GUIStyle& style) {
    // Render border as 4 quads (top, bottom, left, right)
    float bw = style.borderWidth;
    
    // Top border
    RenderQuad(GUIRect{rect.x, rect.y, rect.w, bw}, style.borderColor);
    // Bottom border
    RenderQuad(GUIRect{rect.x, rect.y + rect.h - bw, rect.w, bw}, style.borderColor);
    // Left border
    RenderQuad(GUIRect{rect.x, rect.y, bw, rect.h}, style.borderColor);
    // Right border
    RenderQuad(GUIRect{rect.x + rect.w - bw, rect.y, bw, rect.h}, style.borderColor);
}

void GUI::RenderText(const std::string& text, const GUIRect& rect, const GUIStyle& style) {
    // Text rendering would go here
    // In a full implementation, we'd use a font system to render text
    // For now, this is a placeholder
    
    (void)text;
    (void)rect;
    (void)style;
    
    // Simple debug text rendering could use a bitmap font
    // or we could integrate with a text rendering library
}

bool GUI::HandleMouseClick(float x, float y, int button) {
    if (!m_visible || !m_root) return false;
    
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        return m_root->HandleMouseClick(x, y);
    }
    
    return false;
}

bool GUI::HandleMouseMove(float x, float y) {
    if (!m_visible || !m_root) return false;
    
    m_cursorX = x;
    m_cursorY = y;
    
    return m_root->HandleMouseMove(x, y);
}

bool GUI::HandleMouseScroll(float x, float y) {
    (void)x;
    (void)y;
    return false;
}

bool GUI::HandleKeyDown(int key, int scancode, int action, int mods) {
    if (!m_visible || !m_root) return false;
    return m_root->HandleKeyDown(key, scancode, action, mods);
}

bool GUI::HandleCharInput(unsigned int codepoint) {
    if (!m_visible || !m_root) return false;
    return m_root->HandleCharInput(codepoint);
}

void GUI::SetVisible(bool visible) {
    Logger::Info("[GUI] SetVisible called: " + std::string(visible ? "true" : "false"));
    m_visible = visible;
}

// ── Element Creation Helpers ─────────────────────────────────────────────────

std::unique_ptr<GUIElement> GUI::CreatePanel() {
    return std::make_unique<GUIElement>(GUIElementType::Panel);
}

std::unique_ptr<GUIElement> GUI::CreateButton(const std::string& text) {
    auto button = std::make_unique<GUIElement>(GUIElementType::Button);
    button->text = text;
    button->style.backgroundColor = GUIColor(0.2f, 0.2f, 0.2f, 1.0f);
    button->style.textColor = GUIColor::White();
    button->style.hoverColor = GUIColor(0.3f, 0.3f, 0.3f, 1.0f);
    return button;
}

std::unique_ptr<GUIElement> GUI::CreateLabel(const std::string& text) {
    auto label = std::make_unique<GUIElement>(GUIElementType::Label);
    label->text = text;
    label->style.backgroundColor = GUIColor::Transparent();
    label->style.textColor = GUIColor::White();
    return label;
}

std::unique_ptr<GUIElement> GUI::CreateImage(const std::string& texturePath) {
    auto image = std::make_unique<GUIElement>(GUIElementType::Image);
    image->texturePath = texturePath;
    return image;
}

std::unique_ptr<GUIElement> GUI::CreateTextBox(const std::string& placeholder) {
    auto textbox = std::make_unique<GUIElement>(GUIElementType::TextBox);
    textbox->placeholder = placeholder;
    textbox->style.backgroundColor = GUIColor(0.1f, 0.1f, 0.1f, 1.0f);
    textbox->style.textColor = GUIColor::White();
    textbox->style.borderColor = GUIColor(0.3f, 0.3f, 0.3f, 1.0f);
    textbox->style.borderWidth = 1.0f;
    return textbox;
}

std::unique_ptr<GUIElement> GUI::CreateSlider(float min, float max) {
    auto slider = std::make_unique<GUIElement>(GUIElementType::Slider);
    slider->minValue = min;
    slider->maxValue = max;
    slider->style.backgroundColor = GUIColor(0.2f, 0.2f, 0.2f, 1.0f);
    return slider;
}

std::unique_ptr<GUIElement> GUI::CreateCheckBox() {
    auto checkbox = std::make_unique<GUIElement>(GUIElementType::CheckBox);
    checkbox->style.backgroundColor = GUIColor(0.2f, 0.2f, 0.2f, 1.0f);
    checkbox->style.borderColor = GUIColor(0.5f, 0.5f, 0.5f, 1.0f);
    checkbox->style.borderWidth = 1.0f;
    return checkbox;
}

std::unique_ptr<GUIElement> GUI::CreateProgressBar() {
    auto progress = std::make_unique<GUIElement>(GUIElementType::ProgressBar);
    progress->style.backgroundColor = GUIColor(0.2f, 0.2f, 0.2f, 1.0f);
    return progress;
}

void GUI::LoadFromHTML(const std::string& html, const std::string& css) {
    // HTML/CSS parsing would go here
    // For now, this is a placeholder
    
    (void)html;
    (void)css;
    
    Logger::Info("[GUI] LoadFromHTML called (not yet implemented)");
}

} // namespace veex