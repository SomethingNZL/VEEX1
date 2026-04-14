cmake_minimum_required(VERSION 3.15)
project(VEEXEngine_Core LANGUAGES C CXX)

set(ENGINE_NAME VEEXEngine)

# Require C++20 for std::span (used by VTFParser), concepts, etc.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find dependencies
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)

# Paths to third_party modules
set(GLAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad")
set(MIKKT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/MikkTSpace")
set(MINIAUDIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio")
set(VTFPARSER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/VTFParser")
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")

# glad - bundled in third_party
add_library(glad STATIC
    "${GLAD_DIR}/src/gl.c"
)
target_include_directories(glad PUBLIC
    "${GLAD_DIR}/include"
)

set_source_files_properties("${GLAD_DIR}/src/gl.c" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# VTFParser - VTF texture format parser
add_library(VTFParser STATIC
    "${VTFPARSER_DIR}/src/vtf.cpp"
    "${VTFPARSER_DIR}/src/vtf.hpp"
    "${VTFPARSER_DIR}/src/errors.hpp"
    "${VTFPARSER_DIR}/src/helpers/check-bounds.hpp"
    "${VTFPARSER_DIR}/src/file-format-objects/header.hpp"
    "${VTFPARSER_DIR}/src/file-format-objects/enums.hpp"
)
target_include_directories(VTFParser PUBLIC
    "${VTFPARSER_DIR}"
)
set_target_properties(VTFParser PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_executable(${ENGINE_NAME}
    src/main.cpp
    src/Application.cpp
    src/Server.cpp
    src/Client.cpp
    src/Config.cpp
    src/KeyValues.cpp
    src/FileSystem.cpp
    src/GameInfo.cpp
    src/VPK.cpp
    src/Shader.cpp
    src/Camera.cpp
    src/Renderer.cpp
    src/BSP.cpp
    src/Model.cpp
    src/stb_impl.cpp
    src/Texture.cpp
    src/VTFLoader.cpp     # VTF texture format support
    src/VMTLoader.cpp     # VMT material format support
    src/BSPParser.cpp
    src/MaterialSystem.cpp
    src/EntityParser.cpp
    src/Skybox.cpp
    src/PlayerController.cpp
    src/PropStaticEntity.cpp
    src/BinfoParser.cpp
    src/SoundKit.cpp
    src/AmbientGeneric.cpp
    src/miniaudio_impl.cpp  # <--- Add implementation file here
    src/TileRenderer.cpp    # <--- Add TileRenderer implementation
    src/LightmapSystem.cpp  # <--- Add LightmapSystem implementation
    src/GUI.cpp             # <--- Add GUI system implementation
    src/PauseMenu.cpp       # <--- Add pause menu implementation
    src/MainMenu.cpp        # <--- Add main menu implementation
    src/MDL.cpp             # <--- Add MDL model format support
    src/PropDynamicEntity.cpp # <--- Add Dynamic MDL entity support
    src/ReflectionProbe.cpp # <--- Add Reflection Probe system
    "${MIKKT_DIR}/mikktspace.c"
    # Dear ImGui - GUI framework (MIT License)
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

# Skip PCH for C and implementation files
set_source_files_properties(
    "${MIKKT_DIR}/mikktspace.c" 
    "src/miniaudio_impl.cpp" # <--- Skip PCH for miniaudio
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

# Use quoted paths to handle the space in "VEEX 1"
target_include_directories(${ENGINE_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
    "${GLAD_DIR}/include"
    "${MIKKT_DIR}"
    "${MINIAUDIO_DIR}" # <--- Include miniaudio directory
    "${VTFPARSER_DIR}" # <--- Include VTFParser directory
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb" # <--- Include stb directory for stb_truetype
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui" # <--- Include Dear ImGui directory
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends" # <--- Include ImGui backends
)

target_precompile_headers(${ENGINE_NAME} PRIVATE
    <iostream>
    <string>
    <vector>
    <unordered_map>
    <chrono>
)

# Platform-specific linking for Audio
if(APPLE)
    target_link_libraries(${ENGINE_NAME} PRIVATE "-framework CoreAudio" "-framework CoreFoundation" "-framework AudioUnit")
    target_compile_definitions(${ENGINE_NAME} PRIVATE PLATFORM_MACOS GL_SILENCE_DEPRECATION)
elseif(WIN32)
    # Windows typically doesn't need extra libs for miniaudio (it uses WASAPI via headers),
    # but some setups require winmm.lib or ole32.lib.
    target_link_libraries(${ENGINE_NAME} PRIVATE winmm ole32)
endif()

target_link_libraries(${ENGINE_NAME} PRIVATE VTFParser glad glfw OpenGL::GL glm::glm)

target_compile_definitions(${ENGINE_NAME} PRIVATE GLFW_INCLUDE_NONE)
target_compile_definitions(glad PUBLIC GLFW_INCLUDE_NONE)

add_custom_command(TARGET ${ENGINE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:${ENGINE_NAME}>/shaders"
    COMMENT "Copying shaders to output directory"
)

# Ensure glm is included
find_package(glm REQUIRED)

# Add glm include directory
target_include_directories(${ENGINE_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm"
)

# Test executable for GameInfo and VPK testing
add_executable(GameInfoTest
    src/test_gameinfo.cpp
    src/GameInfo.cpp
    src/FileSystem.cpp
    src/VPK.cpp
    src/KeyValues.cpp
    src/Config.cpp
)

target_include_directories(GameInfoTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
    "${GLAD_DIR}/include"
    "${MIKKT_DIR}"
    "${MINIAUDIO_DIR}"
    "${VTFPARSER_DIR}"
)

target_link_libraries(GameInfoTest PRIVATE
    VTFParser glad glfw OpenGL::GL glm::glm
)

if(APPLE)
    target_link_libraries(GameInfoTest PRIVATE "-framework CoreAudio" "-framework CoreFoundation" "-framework AudioUnit")
    target_compile_definitions(GameInfoTest PRIVATE PLATFORM_MACOS GL_SILENCE_DEPRECATION)
elseif(WIN32)
    target_link_libraries(GameInfoTest PRIVATE winmm ole32)
endif()
