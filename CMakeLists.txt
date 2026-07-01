cmake_minimum_required(VERSION 3.24)

project(QuantumAtom
    VERSION 0.3.0
    DESCRIPTION "Real-time hydrogen-like atomic orbital visualizer"
    HOMEPAGE_URL "https://github.com/steeve-pix/Atom"
    LANGUAGES C CXX
)

include(GNUInstallDirs)
include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(QUANTUMATOM_WITH_DEBUG_SYMBOLS "Emit debug symbols for optimized builds" ON)
option(QUANTUMATOM_STATIC_MINGW_RUNTIME "Statically link libgcc/libstdc++ when building with MinGW" ON)
option(QUANTUMATOM_FETCH_DEPS "Fetch dependencies that are not found by find_package/vcpkg" ON)

set(QUANTUMATOM_MAX_N "8" CACHE STRING "Maximum supported principal quantum number")
set(QUANTUMATOM_DEFAULT_POINTS "120000" CACHE STRING "Default point-cloud sample count")

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/QuantumAtomVersion.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/QuantumAtomVersion.h
    @ONLY
)

# If users configure with a vcpkg toolchain, these find_package calls pick up
# vcpkg packages. Otherwise the project falls back to bundled/FetchContent deps.
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG QUIET)

if(NOT glfw3_FOUND)
    if(NOT QUANTUMATOM_FETCH_DEPS)
        message(FATAL_ERROR "glfw3 was not found. Reconfigure with QUANTUMATOM_FETCH_DEPS=ON or a vcpkg toolchain.")
    endif()

    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

find_package(glm CONFIG QUIET)
if(NOT TARGET glm::glm)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/glm/glm.hpp)
        add_library(glm_external INTERFACE)
        target_include_directories(glm_external INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/external)
        add_library(glm::glm ALIAS glm_external)
    else()
        if(NOT QUANTUMATOM_FETCH_DEPS)
            message(FATAL_ERROR "glm was not found. Reconfigure with QUANTUMATOM_FETCH_DEPS=ON or provide external/glm.")
        endif()

        FetchContent_Declare(
            glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG 1.0.3
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(glm)
        if(TARGET glm AND NOT TARGET glm::glm)
            add_library(glm::glm ALIAS glm)
        endif()
    endif()
endif()

find_path(STB_INCLUDE_DIR stb_image_write.h)
if(NOT STB_INCLUDE_DIR)
    if(NOT QUANTUMATOM_FETCH_DEPS)
        message(FATAL_ERROR "stb_image_write.h was not found. Reconfigure with QUANTUMATOM_FETCH_DEPS=ON.")
    endif()

    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(stb)
    set(STB_INCLUDE_DIR ${stb_SOURCE_DIR})
endif()

find_package(glad CONFIG QUIET)
set(QUANTUMATOM_GLAD_TARGET "")
if(TARGET glad::glad)
    set(QUANTUMATOM_GLAD_TARGET glad::glad)
elseif(TARGET glad)
    set(QUANTUMATOM_GLAD_TARGET glad)
else()
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/src/glad.c)
        set(QUANTUMATOM_GLAD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/glad)
    elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/src/glad.c)
        set(QUANTUMATOM_GLAD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad)
    else()
        message(FATAL_ERROR "GLAD was not found. Provide a vcpkg glad package, external/glad, or third_party/glad.")
    endif()

    add_library(glad_loader STATIC ${QUANTUMATOM_GLAD_DIR}/src/glad.c)
    target_include_directories(glad_loader PUBLIC ${QUANTUMATOM_GLAD_DIR}/include)
    target_link_libraries(glad_loader PUBLIC ${CMAKE_DL_LIBS})
    set_target_properties(glad_loader PROPERTIES FOLDER "Dependencies")
    set(QUANTUMATOM_GLAD_TARGET glad_loader)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.cpp)
    set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
else()
    if(NOT QUANTUMATOM_FETCH_DEPS)
        message(FATAL_ERROR "Dear ImGui was not found. Reconfigure with QUANTUMATOM_FETCH_DEPS=ON or provide external/imgui.")
    endif()

    FetchContent_Declare(
        imgui_source
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.9b
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui_source)
    set(IMGUI_DIR ${imgui_source_SOURCE_DIR})
endif()

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
set_target_properties(imgui PROPERTIES FOLDER "Dependencies")

set(QUANTUMATOM_SOURCES
    src/main.cpp
    src/Camera.cpp
    src/QuantumSimulation.cpp
    src/ShaderLoader.cpp
    src/core/AppConfig.cpp
    src/core/Engine.cpp
)

add_executable(QuantumAtom ${QUANTUMATOM_SOURCES})
target_include_directories(QuantumAtom PRIVATE
    include
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_include_directories(QuantumAtom SYSTEM PRIVATE ${STB_INCLUDE_DIR})
target_compile_definitions(QuantumAtom PRIVATE
    QUANTUMATOM_MAX_N=${QUANTUMATOM_MAX_N}
    QUANTUMATOM_DEFAULT_POINT_COUNT=${QUANTUMATOM_DEFAULT_POINTS}
)
target_link_libraries(QuantumAtom PRIVATE
    glfw
    ${QUANTUMATOM_GLAD_TARGET}
    glm::glm
    imgui
    OpenGL::GL
)

if(MSVC)
    target_compile_options(QuantumAtom PRIVATE /W4 /permissive-)
    if(QUANTUMATOM_WITH_DEBUG_SYMBOLS)
        target_compile_options(QuantumAtom PRIVATE $<$<CONFIG:Release>:/Zi>)
        target_link_options(QuantumAtom PRIVATE $<$<CONFIG:Release>:/DEBUG>)
    endif()
else()
    target_compile_options(QuantumAtom PRIVATE -Wall -Wextra -Wpedantic)
    if(QUANTUMATOM_WITH_DEBUG_SYMBOLS)
        target_compile_options(QuantumAtom PRIVATE $<$<CONFIG:Release>:-g>)
    endif()
endif()

if(MINGW AND QUANTUMATOM_STATIC_MINGW_RUNTIME)
    target_link_options(QuantumAtom PRIVATE -static -static-libgcc -static-libstdc++)
endif()

add_custom_target(copy_runtime_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:QuantumAtom>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/config
            $<TARGET_FILE_DIR:QuantumAtom>/config
    COMMENT "Copying shaders and default configuration"
)
add_dependencies(QuantumAtom copy_runtime_assets)

install(TARGETS QuantumAtom
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)
install(DIRECTORY shaders DESTINATION .)
install(DIRECTORY config DESTINATION .)
install(DIRECTORY samples DESTINATION .)
install(DIRECTORY docs DESTINATION .)
install(FILES README.md LICENSE DESTINATION .)

set(CPACK_PACKAGE_NAME "QuantumAtom")
set(CPACK_PACKAGE_VENDOR "steeve-pix")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_HOMEPAGE_URL ${PROJECT_HOMEPAGE_URL})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
set(CPACK_GENERATOR "ZIP;TGZ")
include(CPack)
