#include "utils/ShaderLoader.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
    std::string shaderLog(GLuint object, bool program) {
        // Shader and program objects use different OpenGL log APIs, but the
        // caller wants the same plain string either way.
        GLint length = 0;
        if (program) {
            glGetProgramiv(object, GL_INFO_LOG_LENGTH, &length);
        } else {
            glGetShaderiv(object, GL_INFO_LOG_LENGTH, &length);
        }

        if (length <= 1) return {};

        std::vector<char> buffer(static_cast<size_t>(length), '\0');
        if (program) {
            glGetProgramInfoLog(object, length, nullptr, buffer.data());
        } else {
            glGetShaderInfoLog(object, length, nullptr, buffer.data());
        }
        return std::string(buffer.data());
    }
}

namespace ShaderLoader {
    std::string loadTextFile(std::string_view relativePath) {
        const std::filesystem::path rel(relativePath);
        // Support running from the source tree, build tree, installed tree, and
        // unpacked release archives where shaders live next to the executable.
        const std::array<std::filesystem::path, 5> candidates = {
            rel,
            std::filesystem::path("shaders") / rel.filename(),
            std::filesystem::path("../shaders") / rel.filename(),
            std::filesystem::path("../../shaders") / rel.filename(),
            std::filesystem::path("../../../shaders") / rel.filename()
        };

        for (const auto &path: candidates) {
            std::ifstream file(path, std::ios::in | std::ios::binary);
            if (!file) continue;

            std::ostringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }

        throw std::runtime_error("Unable to load shader file: " + std::string(relativePath));
    }

    GLuint compile(GLenum type, std::string_view source, std::string_view debugName) {
        GLuint shader = glCreateShader(type);
        const char *sourcePtr = source.data();
        const GLint sourceLength = static_cast<GLint>(source.size());
        glShaderSource(shader, 1, &sourcePtr, &sourceLength);
        glCompileShader(shader);

        GLint ok = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (ok != GL_TRUE) {
            // Delete failed objects immediately so repeated hot-fix attempts do
            // not leak driver resources.
            const std::string log = shaderLog(shader, false);
            glDeleteShader(shader);
            throw std::runtime_error("Shader compile failed for " + std::string(debugName) + ":\n" + log);
        }

        return shader;
    }

    GLuint createProgramFromFiles(std::string_view vertexPath, std::string_view fragmentPath) {
        const std::string vertexSource = loadTextFile(vertexPath);
        const std::string fragmentSource = loadTextFile(fragmentPath);

        const GLuint vertex = compile(GL_VERTEX_SHADER, vertexSource, vertexPath);
        const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentSource, fragmentPath);

        GLuint program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);

        // Once linked, the program owns the compiled shader code.
        glDeleteShader(vertex);
        glDeleteShader(fragment);

        GLint ok = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (ok != GL_TRUE) {
            const std::string log = shaderLog(program, true);
            glDeleteProgram(program);
            throw std::runtime_error("Shader link failed for " + std::string(vertexPath) +
                                     " + " + std::string(fragmentPath) + ":\n" + log);
        }

        return program;
    }
}
