# Third-Party Generated Sources

`third_party/glad` contains the generated GLAD OpenGL loader used when `external/glad` is not present in a clean checkout.

The generated files mirror the existing project loader layout:

- `include/glad/glad.h`
- `include/KHR/khrplatform.h`
- `src/glad.c`

GLFW, GLM, Dear ImGui, and stb are resolved by CMake through vcpkg/local sources/FetchContent where possible.
