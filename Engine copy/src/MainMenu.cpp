#include "veex/MainMenu.h"
#include "veex/FileSystem.h"
#include "veex/Logger.h"
#include "veex/Config.h"
#include "veex/GUI.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "imgui.h"

namespace veex {

MainMenu::MainMenu() {
}

MainMenu::~MainMenu() {
    Shutdown();
}

void MainMenu::Initialize(const GameInfo& gameInfo) {
    if (m_initialized) {
        Logger::Warn("MainMenu: Already initialized.");
        return;
    }

    m_gameInfo = gameInfo;

    if (!ParseMapOrderFile("")) {
        Logger::Warn("MainMenu: Failed to parse map files.");
    }

    m_initialized = true;
    Logger::Info("MainMenu: Initialized successfully.");
}

void MainMenu::Shutdown() {
    if (!m_initialized) return;

    m_chapters.clear();
    m_visible = false;
    m_initialized = false;
    m_chapterSelected = false;
    m_selectedChapterIndex = -1;

    Logger::Info("MainMenu: Shutdown complete.");
}

void MainMenu::Show(GLFWwindow* window) {
    m_visible = true;
    GUI::Get().SetVisible(true);
    Logger::Info("MainMenu: Shown.");
}

void MainMenu::Hide(GLFWwindow* window) {
    m_visible = false;
    GUI::Get().SetVisible(false);
    Logger::Info("MainMenu: Hidden.");
}

void MainMenu::Toggle(GLFWwindow* window) {
    if (m_visible) {
        Hide(window);
    } else {
        Show(window);
    }
}

void MainMenu::Update(float deltaTime) {
    // ImGui handles its own update in NewFrame
}

void MainMenu::Render() {
    if (!m_visible) return;
    
    // Create the main menu UI using Dear ImGui
    RenderImGuiMenu();
}

void MainMenu::RenderImGuiMenu() {
    // Get screen dimensions for positioning
    ImGuiIO& io = ImGui::GetIO();
    float screenWidth = io.DisplaySize.x;
    float screenHeight = io.DisplaySize.y;
    
    // Set up a full-screen overlay for the main menu
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(screenWidth, screenHeight));
    
    // Create a window with no title bar, no resize, no move, etc.
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | 
                             ImGuiWindowFlags_NoResize | 
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    
    ImGui::Begin("Main Menu", nullptr, flags);
    
    // Semi-transparent dark background
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    drawList->AddRectFilled(ImVec2(0, 0), ImVec2(screenWidth, screenHeight), 
                           IM_COL32(0, 0, 0, 200));
    
    // Title - centered at top (use gameinfo title if available)
    std::string gameTitle = m_gameInfo.title.empty() ? m_gameInfo.gameName : m_gameInfo.title;
    if (gameTitle.empty()) gameTitle = "VEEX ENGINE";
    float titleWidth = 400.0f;
    float titleX = (screenWidth - titleWidth) / 2.0f;
    ImGui::SetCursorPos(ImVec2(titleX, 80.0f));
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", gameTitle.c_str());
    
    // Subtitle (use developer from gameinfo if available)
    std::string subtitle = m_gameInfo.developer.empty() ? "A Modern Game Engine" : "Developed by " + m_gameInfo.developer;
    float subtitleWidth = 250.0f;
    float subtitleX = (screenWidth - subtitleWidth) / 2.0f;
    ImGui::SetCursorPos(ImVec2(subtitleX, 140.0f));
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", subtitle.c_str());
    
    // Menu container - centered
    float menuWidth = 300.0f;
    float menuX = (screenWidth - menuWidth) / 2.0f;
    ImGui::SetCursorPos(ImVec2(menuX, 220.0f));
    
    ImGui::BeginChild("MenuContainer", ImVec2(menuWidth, 400.0f), false, 
                      ImGuiWindowFlags_NoBackground);
    
    // Play button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    
    if (ImGui::Button("PLAY GAME", ImVec2(menuWidth, 50.0f))) {
        Logger::Info("MainMenu: Play Game clicked!");
        if (!m_chapters.empty()) {
            OnChapterSelected(0);
        }
    }
    
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    
    // Chapter selection if available
    if (!m_chapters.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "SELECT CHAPTER");
        ImGui::Spacing();
        
        for (size_t i = 0; i < m_chapters.size(); ++i) {
            const auto& chapter = m_chapters[i];
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.08f, 0.08f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            
            std::string buttonLabel = chapter.title.empty() ? chapter.name : chapter.title;
            if (ImGui::Button(buttonLabel.c_str(), ImVec2(menuWidth, 40.0f))) {
                Logger::Info("MainMenu: Chapter button " + std::to_string(i) + " clicked!");
                OnChapterSelected(static_cast<int>(i));
            }
            
            // Tooltip with extra info
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Map: %s", chapter.mapFile.c_str());
                if (!chapter.developer.empty()) {
                    ImGui::Text("Developer: %s", chapter.developer.c_str());
                }
                if (!chapter.description.empty()) {
                    ImGui::Text("Game: %s", chapter.description.c_str());
                }
                ImGui::EndTooltip();
            }
            
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
        }
    } else {
        // Demo chapter button for testing
        ImGui::Spacing();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.08f, 0.08f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        
        if (ImGui::Button("DEMO CHAPTER", ImVec2(menuWidth, 40.0f))) {
            Logger::Info("MainMenu: Demo chapter clicked!");
            OnChapterSelected(0);
        }
        
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
    }
    
    ImGui::EndChild();
    
    // Bottom buttons
    float bottomY = screenHeight - 120.0f;
    float bottomMenuWidth = 400.0f;
    float bottomMenuX = (screenWidth - bottomMenuWidth) / 2.0f;
    ImGui::SetCursorPos(ImVec2(bottomMenuX, bottomY));
    
    ImGui::BeginChild("BottomButtons", ImVec2(bottomMenuWidth, 60.0f), false,
                      ImGuiWindowFlags_NoBackground);
    
    // Options button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.08f, 0.08f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    
    if (ImGui::Button("OPTIONS", ImVec2(180.0f, 40.0f))) {
        Logger::Info("MainMenu: Options clicked!");
    }
    
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    
    ImGui::SameLine();
    
    // Quit button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.05f, 0.05f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.15f, 0.15f, 1.0f));
    
    if (ImGui::Button("QUIT", ImVec2(180.0f, 40.0f))) {
        Logger::Info("MainMenu: Quit clicked!");
        GLFWwindow* window = glfwGetCurrentContext();
        if (window) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }
    
    ImGui::PopStyleColor(3);
    
    ImGui::EndChild();
    
    ImGui::End();
}

bool MainMenu::ParseMapOrderFile(const std::string& path) {
    m_chapters.clear();

    // Use enginePath from gameinfo as the base directory (absolute path to where gameinfo.txt lives)
    std::string basePath = m_gameInfo.enginePath;
    if (basePath.empty()) {
        Logger::Error("MainMenu: GameInfo enginePath is empty, cannot locate maps.");
        return false;
    }

    // ── Step 1: Try to parse MAPorder.ORDE ──────────────────────────────────────
    std::string orderFilePath = basePath + "MAPorder.ORDE";
    std::ifstream orderFile(orderFilePath);
    
    if (orderFile.is_open()) {
        Logger::Info("MainMenu: Reading MAPorder.ORDE from " + orderFilePath);
        
        std::string line;
        std::string currentChapterName;
        bool foundAnyMaps = false;
        
        while (std::getline(orderFile, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            if (line.empty()) continue;
            
            // Check if this is a chapter header (ends with ':')
            if (!line.empty() && line.back() == ':') {
                currentChapterName = line.substr(0, line.size() - 1);
            } else {
                // This is a map file entry
                ChapterInfo chapter;
                chapter.mapFile = line;
                chapter.name = currentChapterName.empty() ? line : currentChapterName;
                chapter.title = currentChapterName.empty() ? line : currentChapterName;
                chapter.developer = m_gameInfo.developer;
                chapter.description = m_gameInfo.title;
                m_chapters.push_back(chapter);
                foundAnyMaps = true;
            }
        }
        
        if (foundAnyMaps) {
            Logger::Info("MainMenu: Loaded " + std::to_string(m_chapters.size()) + " chapters from MAPorder.ORDE");
            return true;
        }
        
        Logger::Warn("MainMenu: MAPorder.ORDE was empty or had no valid entries.");
    } else {
        Logger::Info("MainMenu: No MAPorder.ORDE found at " + orderFilePath + ", scanning maps directory.");
    }

    // ── Step 2: Fallback — scan maps/ directory for .bsp files ──────────────────
    std::string mapsPath = basePath + "maps";
    
    // Normalize path separator
    if (!mapsPath.empty() && mapsPath.back() != std::filesystem::path::preferred_separator) {
        mapsPath += std::filesystem::path::preferred_separator;
    }
    
    try {
        if (!std::filesystem::exists(mapsPath)) {
            Logger::Error("MainMenu: Maps directory does not exist: " + mapsPath);
            return false;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(mapsPath)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                // Only add .bsp files (Source engine map files)
                if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".bsp") {
                    ChapterInfo chapter;
                    chapter.name = filename;
                    chapter.mapFile = filename;
                    chapter.title = filename;
                    chapter.developer = m_gameInfo.developer;
                    chapter.description = m_gameInfo.title;
                    m_chapters.push_back(chapter);
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        Logger::Error("MainMenu: Failed to list map files in " + mapsPath + ": " + e.what());
        return false;
    }

    Logger::Info("MainMenu: Found " + std::to_string(m_chapters.size()) + " map files in " + mapsPath);
    return !m_chapters.empty();
}

void MainMenu::OnChapterSelected(int chapterIndex) {
    if (chapterIndex < 0 || chapterIndex >= static_cast<int>(m_chapters.size())) {
        Logger::Error("MainMenu: Invalid chapter index: " + std::to_string(chapterIndex));
        return;
    }

    const auto& chapter = m_chapters[chapterIndex];
    m_selectedChapterIndex = chapterIndex;
    m_chapterSelected = true;

    Logger::Info("MainMenu: Chapter selected: " + chapter.name + " -> " + chapter.mapFile);

    // Call the chapter select callback if set
    if (m_chapterSelectCallback) {
        m_chapterSelectCallback(chapter.mapFile);
    }
}

void MainMenu::ResetChapterSelection() {
    m_chapterSelected = false;
    m_selectedChapterIndex = -1;
}

bool MainMenu::HandleMouseClick(float x, float y, int button) {
    if (!m_visible) return false;
    return GUI::Get().HandleMouseClick(x, y, button);
}

bool MainMenu::HandleMouseMove(float x, float y) {
    if (!m_visible) return false;
    return GUI::Get().HandleMouseMove(x, y);
}

bool MainMenu::HandleKeyDown(int key, int scancode, int action, int mods) {
    if (!m_visible) return false;
    return GUI::Get().HandleKeyDown(key, scancode, action, mods);
}

bool MainMenu::HandleCharInput(unsigned int codepoint) {
    if (!m_visible) return false;
    return GUI::Get().HandleCharInput(codepoint);
}

void MainMenu::SetChapterSelectCallback(ChapterSelectCallback callback) {
    m_chapterSelectCallback = callback;
}

} // namespace veex
