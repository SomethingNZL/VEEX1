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
set(MINIAUDIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio")

# glad - bundled in third_party
add_library(glad STATIC
    "${GLAD_DIR}/src/gl.c"
)
target_include_directories(glad PUBLIC
    "${GLAD_DIR}/include"
)

set_source_files_properties("${GLAD_DIR}/src/gl.c" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

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
    src/SoundKit.cpp
    src/AmbientGeneric.cpp
    src/miniaudio_impl.cpp  # <--- Add implementation file here
    "${MIKKT_DIR}/mikktspace.c"
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

target_link_libraries(${ENGINE_NAME} PRIVATE glad glfw OpenGL::GL glm::glm)

target_compile_definitions(${ENGINE_NAME} PRIVATE GLFW_INCLUDE_NONE)
target_compile_definitions(glad PUBLIC GLFW_INCLUDE_NONE)

add_custom_command(TARGET ${ENGINE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:${ENGINE_NAME}>/shaders"
    COMMENT "Copying shaders to output directory"
)