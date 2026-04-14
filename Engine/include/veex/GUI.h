#pragma once
// veex/GUI.h
// Internal HTML/CSS GUI system with OpenGL rendering backend.
// Supports a simplified subset of HTML/CSS for creating UI elements
// like the HL2-style pause menu without external dependencies.

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace veex {

// Forward declarations
class Shader;
class Texture;
class GameInfo;

// ── GUI Color ────────────────────────────────────────────────────────────────
struct GUIColor {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    
    GUIColor() = default;
    GUIColor(float rr, float gg, float bb, float aa = 1.0f) 
        : r(rr), g(gg), b(bb), a(aa) {}
    
    static GUIColor FromHex(const std::string& hex);
    static GUIColor Transparent() { return GUIColor(0, 0, 0, 0); }
    static GUIColor White() { return GUIColor(1, 1, 1, 1); }
    static GUIColor Black() { return GUIColor(0, 0, 0, 1); }
    static GUIColor Orange() { return GUIColor(1.0f, 0.4f, 0.0f, 1.0f); }  // HL2 orange
};

// ── GUI Rect ─────────────────────────────────────────────────────────────────
struct GUIRect {
    float x = 0, y = 0, w = 0, h = 0;
    
    float left() const { return x; }
    float right() const { return x + w; }
    float top() const { return y; }
    float bottom() const { return y + h; }
    float centerX() const { return x + w / 2.0f; }
    float centerY() const { return y + h / 2.0f; }
    
    bool Contains(float px, float py) const {
        return px >= left() && px <= right() && py >= top() && py <= bottom();
    }
};

// ── GUI Alignment ────────────────────────────────────────────────────────────
enum class GUIAlign {
    TopLeft, TopCenter, TopRight,
    MiddleLeft, MiddleCenter, MiddleRight,
    BottomLeft, BottomCenter, BottomRight
};

// ── GUI Anchor ───────────────────────────────────────────────────────────────
enum class GUIAnchor {
    TopLeft, TopCenter, TopRight,
    MiddleLeft, MiddleCenter, MiddleRight,
    BottomLeft, BottomCenter, BottomRight
};

// ── GUI Element Type ─────────────────────────────────────────────────────────
enum class GUIElementType {
    Panel,      // Container
    Button,     // Clickable button
    Label,      // Text label
    Image,      // Image/texture display
    TextBox,    // Text input
    Slider,     // Slider control
    CheckBox,   // Checkbox
    ProgressBar // Progress bar
};

// ── GUI Event Types ──────────────────────────────────────────────────────────
enum class GUIEventType {
    OnClick,
    OnHover,
    OnLeave,
    OnFocus,
    OnBlur,
    OnKeyDown,
    OnKeyUp,
    OnTextChanged
};

// ── GUI Event Callback ───────────────────────────────────────────────────────
using GUICallback = std::function<void(class GUIElement*, GUIEventType)>;

// ── GUI Style ────────────────────────────────────────────────────────────────
struct GUIStyle {
    // Colors
    GUIColor backgroundColor = GUIColor::Transparent();
    GUIColor borderColor = GUIColor::Transparent();
    GUIColor textColor = GUIColor::White();
    GUIColor hoverColor = GUIColor::Transparent();
    
    // Borders
    float borderWidth = 0.0f;
    float borderRadius = 0.0f;
    
    // Padding & Margin
    float paddingLeft = 0, paddingRight = 0, paddingTop = 0, paddingBottom = 0;
    float marginLeft = 0, marginRight = 0, marginTop = 0, marginBottom = 0;
    
    // Font
    float fontSize = 14.0f;
    std::string fontFamily = "Arial";
    bool fontBold = false;
    
    // Text alignment
    GUIAlign textAlign = GUIAlign::MiddleCenter;
    
    // Layout
    bool visible = true;
    bool enabled = true;
    
    // Flex layout
    bool isFlexContainer = false;
    bool flexHorizontal = true;  // true = row, false = column
    float flexSpacing = 4.0f;
    GUIAlign flexAlign = GUIAlign::MiddleCenter;
};

// ── GUI Element ──────────────────────────────────────────────────────────────
class GUIElement {
public:
    GUIElement(GUIElementType type = GUIElementType::Panel);
    virtual ~GUIElement();
    
    // Identity
    std::string id;
    std::string tag;  // For HTML-like selection
    GUIElementType type;
    
    // Hierarchy
    GUIElement* parent = nullptr;
    std::vector<std::unique_ptr<GUIElement>> children;
    
    // Layout
    GUIRect rect;
    GUIAnchor anchor = GUIAnchor::TopLeft;
    
    // Style
    GUIStyle style;
    
    // Content
    std::string text;
    std::string texturePath;
    std::shared_ptr<Texture> texture;
    
    // State
    bool hovered = false;
    bool focused = false;
    bool pressed = false;
    
    // For sliders/progress bars
    float value = 0.0f;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    
    // For text boxes
    std::string placeholder;
    bool isMultiline = false;
    
    // Events
    GUICallback onClick;
    GUICallback onHover;
    GUICallback onLeave;
    GUICallback onFocus;
    GUICallback onBlur;
    GUICallback onTextChanged;
    
    // Methods
    GUIElement* AddChild(std::unique_ptr<GUIElement> child);
    GUIElement* FindById(const std::string& id);
    GUIElement* FindByTag(const std::string& tag);
    
    void SetPosition(float x, float y);
    void SetSize(float w, float h);
    void SetAnchor(GUIAnchor a);
    void SetText(const std::string& t);
    void SetVisible(bool v);
    void SetEnabled(bool e);
    
    virtual void Update(float deltaTime);
    virtual void Layout(const GUIRect& parentRect);
    virtual bool HandleMouseClick(float x, float y);
    virtual bool HandleMouseMove(float x, float y);
    virtual bool HandleKeyDown(int key, int scancode, int action, int mods);
    virtual bool HandleCharInput(unsigned int codepoint);
    
    // Get element bounds in screen space
    GUIRect GetScreenBounds() const;
    
protected:
    void LayoutFlex(const GUIRect& parentRect);
    void LayoutStandard(const GUIRect& parentRect);
};

// ── GUI System ───────────────────────────────────────────────────────────────
class GUI {
public:
    static GUI& Get() {
        static GUI instance;
        return instance;
    }
    
    void Initialize();
    void Shutdown();
    
    // Root panel management
    GUIElement* GetRoot() { return m_root.get(); }
    void SetRoot(std::unique_ptr<GUIElement> root);
    
    // Render
    void Render();
    
    // Update
    void Update(float deltaTime);
    
    // Input handling
    bool HandleMouseClick(float x, float y, int button);
    bool HandleMouseMove(float x, float y);
    bool HandleMouseScroll(float x, float y);
    bool HandleKeyDown(int key, int scancode, int action, int mods);
    bool HandleCharInput(unsigned int codepoint);
    
    // Element creation helpers
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
    bool IsVisible() const { return m_visible; }
    
    // Cursor position tracking
    void SetCursorPosition(float x, float y) { m_cursorX = x; m_cursorY = y; }
    float GetCursorX() const { return m_cursorX; }
    float GetCursorY() const { return m_cursorY; }
    
    // Focused element
    GUIElement* GetFocusedElement() const { return m_focusedElement; }
    
    // Parse HTML/CSS
    void LoadFromHTML(const std::string& html, const std::string& css = "");
    
private:
    GUI();
    ~GUI();
    GUI(const GUI&) = delete;
    GUI& operator=(const GUI&) = delete;
    
    std::unique_ptr<GUIElement> m_root;
    GUIElement* m_focusedElement = nullptr;
    GUIElement* m_hoveredElement = nullptr;
    
    float m_cursorX = 0, m_cursorY = 0;
    bool m_visible = true;
    
    // Rendering
    void RenderElement(GUIElement* element);
    void RenderQuad(const GUIRect& rect, const GUIColor& color);
    void RenderText(const std::string& text, const GUIRect& rect, const GUIStyle& style);
    void RenderBorder(const GUIRect& rect, const GUIStyle& style);
    
    // HTML/CSS Parser (not yet implemented)
    // std::unique_ptr<HTMLParser> m_htmlParser;
    // std::unique_ptr<CSSParser> m_cssParser;
    
    // OpenGL resources
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    std::unique_ptr<Shader> m_shader;
};

} // namespace veex