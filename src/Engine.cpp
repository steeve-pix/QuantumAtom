#include "Engine.h"
#include "QuantumSimulation.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl2.h>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>

#ifndef PI
#define PI 3.141592653589793238462643383279502884f
#endif

// This sets up the Engine when the program starts.
Engine::Engine(int width, int height, const std::string &title)
    : m_width(width), m_height(height), m_title(title), m_dis(0.0f, 1.0f) {
    std::random_device rd;
    m_gen = std::mt19937(rd()); 

    initGlfwWindow(); 
    initOpenGL(); 
    initImGui(); 
    setupCallbacks(); 
    regenerateCloud(); 
}

// This cleans up and closes everything when the program ends.
Engine::~Engine() {
    if (m_shaderProgram) {
        glDeleteProgram(m_shaderProgram);
    }
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

// This function calculates what color a dot should be based on its density.
// It creates a "fire" look where higher density is brighter/hotter.
glm::vec4 Engine::heatmapFire(float value) {
    float clamp_v = std::clamp(value, 0.0f, 1.0f);
    float t = std::pow(clamp_v, 0.35f);

    const int num_stops = 7;
    static const glm::vec3 stops[num_stops] = {
        {0.280f, 0.000f, 0.550f},
        {0.450f, 0.000f, 0.650f},
        {0.800f, 0.000f, 0.550f},
        {0.950f, 0.050f, 0.050f},
        {1.000f, 0.500f, 0.000f},
        {1.000f, 0.900f, 0.000f},
        {1.000f, 1.000f, 0.850f}
    };

    float scaled_v = t * (num_stops - 1);
    int i = static_cast<int>(scaled_v);
    int next_i = std::min(i + 1, num_stops - 1);
    float local_t = scaled_v - static_cast<float>(i);

    glm::vec3 color = glm::mix(stops[i], stops[next_i], local_t);

    float alpha = std::pow(t, 0.60f) * 0.95f;

    if (t < 0.03f) {
        alpha *= (t / 0.03f); 
    }

    return glm::vec4(color, alpha);
}

// This is the core logic that creates the cloud of dots.
// It uses "rejection sampling": it picks a random spot and checks the math
// to see if an electron is likely to be there. If yes, it adds a dot.
void Engine::regenerateCloud() {
    cloudPoints.clear();
    cloudPoints.reserve(maxPoints);

    const float maxR = 6.0f * static_cast<float>(state.n * state.n) * 1.5f;

    // First, find the highest possible density to use as a baseline.
    float maxTestDensity = 0.0f;
    for (int i = 0; i < 1000; ++i) {
        float testR = m_dis(m_gen) * maxR;
        float testTh = std::acos(2.0f * m_dis(m_gen) - 1.0f);
        float testPh = 2.0f * PI * m_dis(m_gen);

        maxTestDensity = std::max(maxTestDensity, QuantumSimulation::computeProbability(testR, testTh, testPh, state));
    }
    if (maxTestDensity <= 1e-7f) maxTestDensity = 1.0f;

    // Keep trying random spots until we have enough dots.
    int maxAttempts = maxPoints * 25; 
    int attempts = 0;

    while (static_cast<int>(cloudPoints.size()) < maxPoints && attempts < maxAttempts) {
        attempts++;
        float u = m_dis(m_gen);
        float r = maxR * u;

        float theta = std::acos(2.0f * m_dis(m_gen) - 1.0f);
        float phi = 2.0f * PI * m_dis(m_gen);

        float density = QuantumSimulation::computeProbability(r, theta, phi, state);

        float volumeCorrection = r * r;
        float adjustedDensity = density * volumeCorrection;

        // If the math says this spot is likely, keep it!
        if (m_dis(m_gen) * (maxTestDensity * maxR * maxR) < adjustedDensity) {
            glm::vec3 pos(r * std::sin(theta) * std::cos(phi),
                          r * std::cos(theta),
                          r * std::sin(theta) * std::sin(phi));

            float individualSpeed = 0.3f + (m_dis(m_gen) * 2.2f);
            cloudPoints.push_back({pos, glm::vec3(0.0f), density, individualSpeed});
        }
    }

    // If we couldn't find enough spots, fill the rest with empty dots.
    while (static_cast<int>(cloudPoints.size()) < maxPoints) {
        cloudPoints.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f});
    }
}

// Resets everything to the initial Hydrogen state.
void Engine::resetSimulation() {
    state.n = 1;
    state.l = 0;
    state.m = 0;
    clipEnabled = false;
    camera.targetYaw = -40.0f;
    camera.targetPitch = 25.0f;
    camera.targetDistance = 380.0f;
    camera.destinationTargetPos = glm::vec3(0.0f);
    electronAngle = 0.0f;
    regenerateCloud();
    std::cout << "[System] Simulation environment successfully reset." << std::endl;
}

// Re-creates just one dot in the cloud.
void Engine::regenerateSinglePoint(CloudPoint &p) {
    const float maxR = 12.0f * static_cast<float>(state.n * state.n);
    float r = m_dis(m_gen) * maxR * 0.98f;
    float theta = std::acos(2.0f * m_dis(m_gen) - 1.0f);
    float phi = 2.0f * PI * m_dis(m_gen);
    p.pos = glm::vec3(r * std::sin(theta) * std::cos(phi), r * std::sin(theta) * std::sin(phi), r * std::cos(theta));
    p.vel = glm::vec3(0.0f);
    p.brightness = QuantumSimulation::computeProbability(r, theta, phi, state);
}

// Updates movement over time, like the rotating electron tracker.
void Engine::updatePhysics(float deltaTime) {
    float orbitSpeed = 4.5f / static_cast<float>(state.n * state.n);
    electronAngle += orbitSpeed * deltaTime;

    if (electronAngle > 2.0f * PI) electronAngle = std::fmod(electronAngle, 2.0f * PI);
}

// Helper function to compile the code that runs on the graphics card.
GLuint Engine::compileShader(GLenum type, const std::string &source) {
    GLuint shader = glCreateShader(type);
    const char *src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "[Shader Error] Compilation failed:\n" << infoLog << std::endl;
    }
    return shader;
}

// Sets up the shaders to make the dots look like soft, glowing spheres.
void Engine::initShaders() {
    std::string vertexSource =
            "#version 120\n"
            "void main() {\n"
            "    gl_FrontColor = gl_Color;\n"
            "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
            "}\n";

    std::string fragmentSource =
            "#version 120\n"
            "void main() {\n"
            "    vec2 circCoord = gl_PointCoord - vec2(0.5);\n"
            "    float distSq = dot(circCoord, circCoord);\n"
            "    if (distSq > 0.25) discard;\n" 
            "    \n"
            "    float alphaIntensity = smoothstep(0.25, 0.0, distSq);\n"
            "    gl_FragColor = vec4(gl_Color.rgb, gl_Color.a * alphaIntensity);\n"
            "}\n";

    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vs);
    glAttachShader(m_shaderProgram, fs);
    glLinkProgram(m_shaderProgram);
}

// Draws the menus and information on the screen using ImGui.
void Engine::renderUI() {
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiIO &io = ImGui::GetIO();

    double currentTime = glfwGetTime();
    m_frameCount++;
    if (currentTime - m_lastFpsUpdateTime >= 0.5) {
        m_fps = static_cast<float>(m_frameCount) / static_cast<float>(currentTime - m_lastFpsUpdateTime);
        m_frameTimeMs = 1000.0f / m_fps;
        m_frameCount = 0;
        m_lastFpsUpdateTime = currentTime;
    }

    float hudMargin = 15.0f;
    ImVec2 hudPos = ImVec2(io.DisplaySize.x - hudMargin, hudMargin);
    ImVec2 hudPivot = ImVec2(1.0f, 0.0f); 
    ImGui::SetNextWindowPos(hudPos, ImGuiCond_Always, hudPivot);
    ImGui::SetNextWindowBgAlpha(0.55f); 

    ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("Performance & Math HUD", nullptr, hudFlags)) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "SYSTEM METRICS");
        ImGui::Separator();
        ImGui::Text("Performance: %.1f ms", m_frameTimeMs);
        ImGui::Text("Frame Time:  %.2f FPS", m_fps);
        ImGui::Text("Cloud Density: %d / %d Points", static_cast<int>(cloudPoints.size()), maxPoints);

        ImGui::Spacing();
        ImGui::Spacing();
    }
    ImGui::End();


    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(420, 680), ImGuiCond_Once);
    ImGui::Begin("Quantum Configuration & Information", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "QUANTUM NUMBERS CONTROL");
    ImGui::Separator();

    bool stateChanged = false;

    ImGui::SliderInt("Principle (n)", &state.n, 1, 6);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        stateChanged = true;
        if (state.l >= state.n) state.l = state.n - 1;
        state.m = glm::clamp(state.m, -state.l, state.l);
    }
    ImGui::TextDisabled("Defines energy shell and size limit boundaries.");

    ImGui::SliderInt("Azimuthal (l)", &state.l, 0, state.n - 1);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        stateChanged = true;
        state.m = glm::clamp(state.m, -state.l, state.l);
    }
    ImGui::TextDisabled("Defines the subshell shape layout geometry (s, p, d, f).");

    ImGui::SliderInt("Magnetic (m)", &state.m, -state.l, state.l);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        stateChanged = true;
    }
    ImGui::TextDisabled("Defines spatial orientation axes constraints.");

    ImGui::Spacing();
    ImGui::Checkbox("Enable Cross-Section Clip", &clipEnabled);
    if (stateChanged) regenerateCloud(); 

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "WHAT ARE YOU LOOKING AT?");
    ImGui::Separator();

    ImGui::BeginChild("TheoryScroll", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    ImGui::SeparatorText("The Quantum Probability Cloud");
    ImGui::TextWrapped(
        "This particle cloud is a 3D solution to the time-independent Schrodinger Equation "
        "for a Hydrogen-like atom."
    );
    ImGui::Spacing();

    ImGui::BulletText("Wave Function (Psi):");
    ImGui::TextWrapped(
        "Electrons do not travel in planetary rings. They exist as a standing wave of probability. "
        "The denser the cloud points are in a region, the higher the mathematical likelihood "
        "of finding the electron there upon measurement."
    );
    ImGui::Spacing();

    ImGui::BulletText("Hydrogen-Like Analytical Solution:");
    ImGui::TextWrapped(
        "By simulating a single-electron system, we omit chaotic electron-electron repulsions. "
        "This isolates pure electrostatic attraction, rendering an exact mathematical representation "
        "of atomic geometry."
    );

    ImGui::Spacing();

    ImGui::SeparatorText("The Classical Simulation Tracker");
    ImGui::TextWrapped(
        "The bright traveling sphere represents a localized simulation snapshot tracker showing "
        "the dynamic orbital path tracking across calculated classical kinetic constraints."
    );
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "Bridging Two Frameworks:");
    ImGui::TextWrapped(
        "Because the Heisenberg Uncertainty Principle forbids an exact path for quantum waves, "
        "this sphere is an analytical overlay. It maps classical momentum, velocity gradients, "
        "and potential energy fields as a localized point riding through the probability cloud, "
        "making the abstract math intuitive to human sight."
    );

    ImGui::EndChild();
    ImGui::End();

    float margin = 15.0f;
    ImVec2 window_pos = ImVec2(static_cast<float>(m_width) - margin, static_cast<float>(m_height) - margin);
    ImVec2 window_pos_pivot = ImVec2(1.0f, 1.0f);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    ImGui::SetNextWindowBgAlpha(0.35f); 

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
                                    ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoFocusOnAppearing |
                                    ImGuiWindowFlags_NoNav |
                                    ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("Credits Overlay", nullptr, window_flags)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
        ImGui::Text("Credits to");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.7f, 1.0f), "kavan010");
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Visit kavan010's GitHub Profile");
        }

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::Text("GitHub:");
        ImGui::SameLine();
        ImGui::TextUnformatted("https://github.com/kavan010/");
        ImGui::PopStyleColor();
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

// This is the main drawing routine. It clears the screen and calls all the other draw functions.
void Engine::drawScene(float currentFrameTime, float deltaTime) {
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    camera.update(m_width, m_height, deltaTime);

    drawAxes();

    glPushMatrix();
    drawCloud(currentFrameTime);
    glPopMatrix();

    glPushMatrix();
    glPopMatrix();

    renderUI();
}

// Sets up the GLFW window context.
void Engine::initGlfwWindow() {
    if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW.");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window.");
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); 
    glfwSetWindowUserPointer(window, this); 
}

// Loads OpenGL functions and sets up basic drawing rules.
void Engine::initOpenGL() {
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        throw std::runtime_error("Failed to initialize GLAD.");

    glEnable(GL_DEPTH_TEST); 
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(0x8861); 

    initShaders();
}

// Sets up the ImGui menu system.
void Engine::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL2_Init();
}

// Connects keyboard and mouse inputs to the program's functions.
void Engine::setupCallbacks() {
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow *win, int w, int h) {
        auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
        eng->m_width = w;
        eng->m_height = h;
        glViewport(0, 0, w, h);
    });

    glfwSetKeyCallback(window, [](GLFWwindow *win, int key, int scancode, int action, int mods) {
        ImGui_ImplGlfw_KeyCallback(win, key, scancode, action, mods);
        auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));

        if (!ImGui::GetIO().WantCaptureKeyboard && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            bool changed = false;

            if (key == GLFW_KEY_R && action == GLFW_PRESS) {
                eng->resetSimulation();
                return;
            }

            if (key == GLFW_KEY_UP) {
                eng->state.n = std::min(eng->state.n + 1, 6);
                changed = true;
            }
            if (key == GLFW_KEY_DOWN) {
                eng->state.n = std::max(eng->state.n - 1, 1);
                if (eng->state.l >= eng->state.n) eng->state.l = eng->state.n - 1;
                eng->state.m = glm::clamp(eng->state.m, -eng->state.l, eng->state.l);
                changed = true;
            }
            if (key == GLFW_KEY_C && action == GLFW_PRESS) {
                eng->clipEnabled = !eng->clipEnabled;
            }
            if (changed) eng->regenerateCloud();
        }
    });

    glfwSetMouseButtonCallback(window, [](GLFWwindow *win, int button, int action, int mods) {
        ImGui_ImplGlfw_MouseButtonCallback(win, button, action, mods);
        auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
        if (!ImGui::GetIO().WantCaptureMouse) {
            if (action == GLFW_PRESS) eng->m_mouseButtonDown = button;
            else if (action == GLFW_RELEASE) eng->m_mouseButtonDown = -1;
        } else eng->m_mouseButtonDown = -1;
    });

    glfwSetCursorPosCallback(window, [](GLFWwindow *win, double xpos, double ypos) {
        ImGui_ImplGlfw_CursorPosCallback(win, xpos, ypos);
        auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
        if (eng->m_firstMouse) {
            eng->m_lastMouseX = static_cast<float>(xpos);
            eng->m_lastMouseY = static_cast<float>(ypos);
            eng->m_firstMouse = false;
            return;
        }
        float xoffset = static_cast<float>(xpos) - eng->m_lastMouseX;
        float yoffset = eng->m_lastMouseY - static_cast<float>(ypos);
        eng->m_lastMouseX = static_cast<float>(xpos);
        eng->m_lastMouseY = static_cast<float>(ypos);

        if (eng->m_mouseButtonDown != -1 && !ImGui::GetIO().WantCaptureMouse) {
            bool shiftPressed = (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                 glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

            if (shiftPressed || eng->m_mouseButtonDown == GLFW_MOUSE_BUTTON_RIGHT) eng->camera.pan(xoffset, yoffset);
            else if (eng->m_mouseButtonDown == GLFW_MOUSE_BUTTON_LEFT) {
                eng->camera.targetYaw += xoffset * 0.18f;
                eng->camera.targetPitch += yoffset * 0.18f;
                eng->camera.targetPitch = glm::clamp(eng->camera.targetPitch, -89.0f, 89.0f);
            }
        }
    });

    glfwSetScrollCallback(window, [](GLFWwindow *win, double xoffset, double yoffset) {
        ImGui_ImplGlfw_ScrollCallback(win, xoffset, yoffset);
        if (!ImGui::GetIO().WantCaptureMouse) {
            auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
            eng->camera.targetDistance -= static_cast<float>(yoffset) * 22.0f;
            eng->camera.targetDistance = glm::clamp(eng->camera.targetDistance, 60.0f, 1400.0f);
        }
    });

    glfwSetCharCallback(window, [](GLFWwindow *win, unsigned int codepoint) {
        ImGui_ImplGlfw_CharCallback(win, codepoint);
    });
}

// Draws the X, Y, and Z axes to help you see directions in 3D.
void Engine::drawAxes() {
    glPushMatrix();
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(1.0f, 0.0f, 0.0f); 
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(120.0f, 0.0f, 0.0f);

    glColor3f(0.0f, 1.0f, 0.0f); 
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 120.0f, 0.0f);

    glColor3f(0.0f, 0.0f, 1.0f); 
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 120.0f);
    glEnd();
    glPopMatrix();
}

// This function draws all the dots that make up the electron cloud.
void Engine::drawCloud(float timeVal) {
    if (cloudPoints.empty()) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE); 

    glUseProgram(m_shaderProgram);

    float pointScale = glm::clamp(380.0f / camera.distance, 0.7f, 5.0f);
    glPointSize(14.0f * pointScale);

    float maxDensity = 1e-6f;
    const size_t numPoints = cloudPoints.size();

    for (size_t i = 0; i < numPoints; ++i) {
        if (cloudPoints[i].brightness > maxDensity) {
            maxDensity = cloudPoints[i].brightness;
        }
    }

    float invMaxDensity = 1.0f / maxDensity; 
    float globalSpeed = 5.0f / static_cast<float>(state.n);
    bool useRotation = (state.m != 0);
    float m_float = static_cast<float>(state.m);

    glBegin(GL_POINTS);
    for (size_t i = 0; i < numPoints; ++i) {
        const auto &p = cloudPoints[i];
        glm::vec3 pos = p.pos;

        float norm = p.brightness * invMaxDensity;

        // If the atom state has a rotation (m != 0), spin the dots around the center.
        if (useRotation) {
            float probabilitySpeedFactor = 0.15f + (norm * 3.5f);
            float angle = timeVal * globalSpeed * probabilitySpeedFactor * m_float;

            float sinA = std::sin(angle);
            float cosA = std::cos(angle);

            float origX = pos.x;
            float origZ = pos.z;
            pos.x = origX * cosA - origZ * sinA;
            pos.z = origX * sinA + origZ * cosA;
        }

        // If clipping is enabled, don't draw dots in the top-front quadrant.
        if (clipEnabled && pos.y > 0.0f && pos.z > 0.0f) {
            continue;
        }

        glm::vec4 fireColor = heatmapFire(norm);
        glColor4f(fireColor.r, fireColor.g, fireColor.b, fireColor.a);
        glVertex3f(pos.x, pos.y, pos.z);
    }
    glEnd();

    glUseProgram(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// Draws a single white dot that follows a circular path.
void Engine::drawActiveElectron() {
    float radius = 14.5f * static_cast<float>(state.n * state.n);
    float x = radius * std::cos(electronAngle);
    float y = (state.l > 0) ? radius * std::sin(electronAngle) * 0.5f : 0.0f;
    float z = (state.l > 0)
                  ? radius * std::sin(electronAngle) * std::sqrt(1.0f - 0.5f * 0.5f)
                  : radius * std::sin(electronAngle);

    glPointSize(12.0f);
    glBegin(GL_POINTS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glVertex3f(x, y, z);
    glEnd();
}
