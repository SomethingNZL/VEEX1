# MinGW-w64 Cross-Compilation Toolchain for Windows 10/11
# Use with: cmake -DCMAKE_TOOLCHAIN_FILE=mingw_toolchain.cmake ..

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Compilers
set(CMAKE_C_COMPILER   /opt/local/bin/x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER /opt/local/bin/x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  /opt/local/bin/x86_64-w64-mingw32-windres)
set(CMAKE_AR           /opt/local/bin/x86_64-w64-mingw32-ar)
set(CMAKE_RANLIB       /opt/local/bin/x86_64-w64-mingw32-ranlib)
set(CMAKE_STRIP        /opt/local/bin/x86_64-w64-mingw32-strip)

# Target environment
set(CMAKE_FIND_ROOT_PATH /opt/local/x86_64-w64-mingw32)

# Vcpkg integration for Windows dependencies (GLFW3, OpenSSL, glm)
set(VCPKG_TARGET_TRIPLET x64-mingw-dynamic CACHE STRING "")
set(VCPKG_APPLOCAL_DEPS OFF CACHE BOOL "" FORCE)

# Auto-detect vcpkg root
if(DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT "$ENV{VCPKG_ROOT}")
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(VCPKG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../vcpkg")
elseif(EXISTS "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(VCPKG_ROOT "$ENV{HOME}/vcpkg")
endif()

if(DEFINED VCPKG_ROOT AND EXISTS "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    # Add vcpkg installed dir to the search root so find_package can locate libs
    list(APPEND CMAKE_FIND_ROOT_PATH "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}")
    include("${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

# Search behaviour: only search in the target environment for libs/includes,
# but allow programs (like windres) from the host.
# NOTE: "BOTH" is used for library/include so vcpkg paths are also searched.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
