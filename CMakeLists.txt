cmake_minimum_required(VERSION 3.15)
project(VEEXEngine VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Don't override CMAKE_TOOLCHAIN_FILE here — pass it on the command line instead:
#   cmake -DCMAKE_TOOLCHAIN_FILE=mingw_toolchain.cmake -B build_win

add_subdirectory(Engine)
add_subdirectory(Game)
