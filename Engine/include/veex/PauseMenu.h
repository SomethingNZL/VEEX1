#pragma once
// veex/PauseMenu.h
// Pause menu using the internal GUI system.

#include <string>

// Forward declaration for GLFWwindow
struct GLFWwindow;

namespace veex {

// Initialize the pause menu system with game title from gameinfo.txt
void InitializePauseMenu(const std::string& gameTitle = "VEEX Game");

// Initialize the pause menu system (legacy function)
void InitializePauseMenu();

// Show/hide/toggle the pause menu
void ShowPauseMenu(GLFWwindow* window = nullptr);
void HidePauseMenu(GLFWwindow* window = nullptr);
void TogglePauseMenu(GLFWwindow* window = nullptr);

// Check if pause menu is visible
bool IsPauseMenuVisible();

// Update and render
void UpdatePauseMenu(float deltaTime);
void RenderPauseMenu();

// Handle input (returns true if input was consumed)
bool HandlePauseMenuInput(int key, int scancode, int action, int mods);

} // namespace veex
