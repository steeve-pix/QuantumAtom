#pragma once

#include "utils/OpenGLLoader.h"
#include <string>
#include <string_view>

namespace ShaderLoader {
    // Search common build/install shader locations and return the file contents.
    std::string loadTextFile(std::string_view relativePath);

    // Compile one GLSL stage and throw a runtime_error containing the driver log
    // if compilation fails.
    GLuint compile(GLenum type, std::string_view source, std::string_view debugName);

    // Load, compile, link, and return a complete vertex+fragment shader program.
    GLuint createProgramFromFiles(std::string_view vertexPath, std::string_view fragmentPath);
}
