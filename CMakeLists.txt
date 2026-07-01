cmake_minimum_required(VERSION 3.28)
project(QuantumAtom)

# Set C++ standard to C++23 for modern features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable experimental features in GLM (required for some gtx extensions)
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

# --- External Dependency: GLFW ---
# We fetch GLFW directly from GitHub to ensure compatibility
include(FetchContent)
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

# --- Internal Dependency: Dear ImGui ---
# List of source files for the ImGui library and its GLFW/OpenGL2 backends
set(IMGUI_SOURCES
        external/imgui/imgui.cpp
        external/imgui/imgui_demo.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/backends/imgui_impl_glfw.cpp
        external/imgui/backends/imgui_impl_opengl2.cpp
)

# --- Main Executable ---
# Automatically find all .cpp files in the src/ directory
file(GLOB_RECURSE SRC_FILES "src/*.cpp")
add_executable(QuantumAtom
        ${SRC_FILES}
        ${IMGUI_SOURCES}
)

# --- Include Directories ---
# Configure search paths for headers (both internal and external)
target_include_directories(QuantumAtom PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external
        include
)

# Static linking for MinGW to avoid dependency on external DLLs
set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")

# --- Library: GLAD ---
# GLAD handles OpenGL function loading
add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

# --- Library: GLM ---
# Header-only mathematics library
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE external)

# --- Linking ---
# Link all dependencies to the main executable
target_link_libraries(QuantumAtom PRIVATE glfw glad glm opengl32)

# --- Runtime Assets ---
# Keep shader files beside the executable so runtime loading works from build directories.
add_custom_command(TARGET QuantumAtom POST_BUILD
add_custom_target(copy_runtime_assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:QuantumAtom>/shaders
)
add_dependencies(QuantumAtom copy_runtime_assets)
