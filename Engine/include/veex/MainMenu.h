#pragma once
// veex/MainMenu.h
// Main menu system for chapter/map selection using Dear ImGui.
// Displays a blank background with UI overlay for selecting chapters.

#include "veex/GameInfo.h"
#include <string>
#include <vector>
#include <functional>

// Forward declaration for GLFWwindow
struct GLFWwindow;

namespace veex {

// Chapter information structure
struct ChapterInfo {
    std::string name;
    std::string mapFile;
};

// Callback type for chapter selection
using ChapterSelectCallback = std::function<void(const std::string& mapFile)>;

class MainMenu {
public:
    static MainMenu& Get() {
        static MainMenu instance;
        return instance;
    }

    // Initialize the main menu system
    void Initialize(const GameInfo& gameInfo);

    // Shutdown the main menu system
    void Shutdown();

    // Show/hide/toggle the main menu
    void Show(GLFWwindow* window = nullptr);
    void Hide(GLFWwindow* window = nullptr);
    void Toggle(GLFWwindow* window = nullptr);

    // Check if main menu is visible
    bool IsVisible() const { return m_visible; }

    // Update and render
    void Update(float deltaTime);
    void Render();

    // Handle input (returns true if input was consumed)
    bool HandleMouseClick(float x, float y, int button);
    bool HandleMouseMove(float x, float y);
    bool HandleKeyDown(int key, int scancode, int action, int mods);
    bool HandleCharInput(unsigned int codepoint);

    // Set callback for chapter selection
    void SetChapterSelectCallback(ChapterSelectCallback callback);

    // Parse MAPorder.ORDE file
    bool ParseMapOrderFile(const std::string& path);

    // Get list of chapters
    const std::vector<ChapterInfo>& GetChapters() const { return m_chapters; }

    // Check if a chapter is selected
    bool HasChapterSelected() const { return m_chapterSelected; }

    // Get selected chapter index
    int GetSelectedChapterIndex() const { return m_selectedChapterIndex; }

    // Reset chapter selection
    void ResetChapterSelection();

private:
    MainMenu();
    ~MainMenu();
    MainMenu(const MainMenu&) = delete;
    MainMenu& operator=(const MainMenu&) = delete;

    // Render the ImGui menu
    void RenderImGuiMenu();

    // Handle chapter button click
    void OnChapterSelected(int chapterIndex);

    // Clear existing chapter buttons
    void ClearChapterButtons();

    bool m_visible = false;
    bool m_initialized = false;
    bool m_chapterSelected = false;
    int m_selectedChapterIndex = -1;

    std::vector<ChapterInfo> m_chapters;

    ChapterSelectCallback m_chapterSelectCallback = nullptr;
};

} // namespace veex