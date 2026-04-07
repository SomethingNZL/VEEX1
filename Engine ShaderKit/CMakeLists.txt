cmake_minimum_required(VERSION 3.15)
project(VEEXEngine_Core LANGUAGES C CXX)

set(ENGINE_NAME VEEXEngine)

# Find dependencies
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm CONFIG REQUIRED)

# Paths to third_party modules
set(GLAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad")
set(MIKKT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/MikkTSpace")

# glad - bundled in third_party
add_library(glad STATIC
    "${GLAD_DIR}/src/gl.c"
)
target_include_directories(glad PUBLIC
    "${GLAD_DIR}/include"
)

# --- FIX START: Skip PCH for C files ---
set_source_files_properties("${GLAD_DIR}/src/gl.c" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
# --- FIX END ---

add_executable(${ENGINE_NAME}
    src/main.cpp
    src/Application.cpp
    src/Server.cpp
    src/Client.cpp
    src/Config.cpp
    src/KeyValues.cpp
    Src/FileSystem.cpp
    src/GameInfo.cpp
    src/Shader.cpp
    src/Camera.cpp
    src/Renderer.cpp
    src/BSP.cpp
    src/Model.cpp
    src/stb_impl.cpp
    src/Texture.cpp
    Src/BSPParser.cpp
    Src/MaterialSystem.cpp
    src/EntityParser.cpp
    src/Skybox.cpp
    src/PlayerController.cpp
    src/PropStaticEntity.cpp
    Src/BinfoParser.cpp
    "${MIKKT_DIR}/mikktspace.c"
)

# --- FIX START: Skip PCH for MikkTSpace C file ---
set_source_files_properties("${MIKKT_DIR}/mikktspace.c" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
# --- FIX END ---

# Use quoted paths to handle the space in "VEEX 1"
target_include_directories(${ENGINE_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
    "${GLAD_DIR}/include"
    "${MIKKT_DIR}"
)

target_precompile_headers(${ENGINE_NAME} PRIVATE
    <iostream>
    <string>
    <vector>
    <unordered_map>
    <chrono>
)

target_link_libraries(${ENGINE_NAME} PRIVATE glad glfw OpenGL::GL glm::glm)

target_compile_definitions(${ENGINE_NAME} PRIVATE GLFW_INCLUDE_NONE)
target_compile_definitions(glad PUBLIC GLFW_INCLUDE_NONE)

add_custom_command(TARGET ${ENGINE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:${ENGINE_NAME}>/shaders"
    COMMENT "Copying shaders to output directory"
)

if(APPLE)
    target_compile_definitions(${ENGINE_NAME} PRIVATE PLATFORM_MACOS GL_SILENCE_DEPRECATION)
endif()