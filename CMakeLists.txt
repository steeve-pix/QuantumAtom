cmake_minimum_required(VERSION 3.28)
project(QuantumAtom)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

# Fetch GLFW
include(FetchContent)
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

# Main executable
file(GLOB_RECURSE SRC_FILES "src/*.cpp")
add_executable(QuantumAtom
        ${SRC_FILES}
)

set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")

# GLAD
add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

# GLM
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE external/glm)

target_include_directories(QuantumAtom PRIVATE
        include
)

target_link_libraries(QuantumAtom PRIVATE glfw glad glm opengl32)
#
#if (WIN32)
#    target_link_options(QuantumAtom PRIVATE -static-libgcc -static-libstdc++)
#endif ()