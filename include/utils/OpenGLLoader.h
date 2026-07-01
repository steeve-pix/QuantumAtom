#pragma once

#ifdef QUANTUMATOM_USE_GLAD2
#include <glad/gl.h>
#else
#include <glad/glad.h>
#endif

#include <GLFW/glfw3.h>

inline bool loadOpenGLFunctions() {
#ifdef QUANTUMATOM_USE_GLAD2
    return gladLoadGL(glfwGetProcAddress) != 0;
#else
    return gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0;
#endif
}
