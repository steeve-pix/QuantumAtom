#include "Engine.h"
#include "QuantumSimulation.h"
#include <future>
#include <mutex>
#include <atomic>
#include <thread>
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
    if (m_isRegenerating) return;
    m_isRegenerating = true;

    // Launch regeneration in a background thread
    std::thread([this]() {
        std::vector<CloudPoint> newPoints;
        newPoints.reserve(maxPoints);

        const float maxR = 6.0f * static_cast<float>(state.n * state.n) * 1.5f;
        const QuantumState currentState = state; // Capture state to avoid race conditions

        // Use multiple threads for rejection sampling
        unsigned int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 2;
        
        std::vector<std::future<std::vector<CloudPoint>>> futures;
        int pointsPerThread = maxPoints / numThreads;

        for (unsigned int t = 0; t < numThreads; ++t) {
            futures.push_back(std::async(std::launch::async, [this, pointsPerThread, maxR, currentState]() {
                std::vector<CloudPoint> threadPoints;
                std::mt19937 threadGen(std::random_device{}());
                std::uniform_real_distribution<float> threadDis(0.0f, 1.0f);

                // Find max density for this thread
                float threadMaxTestDensity = 0.0f;
                for (int i = 0; i < 500; ++i) {
                    float testR = threadDis(threadGen) * maxR;
                    float testTh = std::acos(2.0f * threadDis(threadGen) - 1.0f);
                    float testPh = 2.0f * PI * threadDis(threadGen);
                    threadMaxTestDensity = std::max(threadMaxTestDensity, QuantumSimulation::computeProbability(testR, testTh, testPh, currentState));
                }
                if (threadMaxTestDensity <= 1e-7f) threadMaxTestDensity = 1.0f;

                int attempts = 0;
                int maxAttempts = pointsPerThread * 50;
                while (static_cast<int>(threadPoints.size()) < pointsPerThread && attempts < maxAttempts) {
                    attempts++;
                    float u = threadDis(threadGen);
                    float r = maxR * u;
                    float theta = std::acos(2.0f * threadDis(threadGen) - 1.0f);
                    float phi = 2.0f * PI * threadDis(threadGen);

                    float density = QuantumSimulation::computeProbability(r, theta, phi, currentState);
                    float adjustedDensity = density * r * r;

                    if (threadDis(threadGen) * (threadMaxTestDensity * maxR * maxR) < adjustedDensity) {
                        glm::vec3 pos(r * std::sin(theta) * std::cos(phi),
                                      r * std::cos(theta),
                                      r * std::sin(theta) * std::sin(phi));
                        float speed = 0.3f + (threadDis(threadGen) * 2.2f);
                        threadPoints.push_back({pos, glm::vec3(0.0f), density, speed});
                    }
                }
                return threadPoints;
            }));
        }

        for (auto &f : futures) {
            auto res = f.get();
            newPoints.insert(newPoints.end(), res.begin(), res.end());
        }

        // Fill remaining
        while (static_cast<int>(newPoints.size()) < maxPoints) {
            newPoints.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f});
        }

        // Calculate max density once
        float maxD = 1e-6f;
        for (const auto& p : newPoints) {
            if (p.brightness > maxD) maxD = p.brightness;
        }

        {
            std::lock_guard<std::mutex> lock(m_cloudMutex);
            cloudPoints = std::move(newPoints);
            m_cachedMaxDensity = maxD;
            m_needsRebuild = true;
        }
        m_isRegenerating = false;
    }).detach();
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
            "uniform float u_time;\n"
            "uniform float u_globalSpeed;\n"
            "uniform float u_m;\n"
            "uniform int u_useRotation;\n"
            "uniform int u_clipEnabled;\n"
            "void main() {\n"
            "    vec3 pos = gl_Vertex.xyz;\n"
            "    if (u_useRotation != 0) {\n"
            "        float angle = u_time * u_globalSpeed * u_m;\n"
            "        float s = sin(angle);\n"
            "        float c = cos(angle);\n"
            "        float x = pos.x * c - pos.z * s;\n"
            "        float z = pos.x * s + pos.z * c;\n"
            "        pos.x = x;\n"
            "        pos.z = z;\n"
            "    }\n"
            "    if (u_clipEnabled != 0 && pos.y > 0.0 && pos.z > 0.0) {\n"
            "        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);\n" // Simple way to discard point in VS
            "    } else {\n"
            "        gl_Position = gl_ModelViewProjectionMatrix * vec4(pos, 1.0);\n"
            "    }\n"
            "    gl_FrontColor = gl_Color;\n"
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
    std::lock_guard<std::mutex> lock(m_cloudMutex);
    if (cloudPoints.empty()) return;

    if (!m_vboInitialized) {
        glGenBuffers(1, &m_vbo);
        m_vboInitialized = true;
    }

    struct Vertex {
        glm::vec3 pos;
        glm::vec4 color;
    };

    static std::vector<Vertex> vertexBuffer;
    if (vertexBuffer.empty()) vertexBuffer.reserve(maxPoints);

    float invMaxDensity = 1.0f / m_cachedMaxDensity;
    float globalSpeed = 5.0f / static_cast<float>(state.n);
    bool useRotation = (state.m != 0);
    float m_float = static_cast<float>(state.m);

    if (m_needsRebuild || vertexBuffer.empty()) {
        vertexBuffer.clear();
        for (const auto &p : cloudPoints) {
            float norm = p.brightness * invMaxDensity;
            glm::vec4 color = heatmapFire(norm);
            vertexBuffer.push_back({p.pos, color});
        }
        m_needsRebuild = false;
        
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size() * sizeof(Vertex), vertexBuffer.data(), GL_DYNAMIC_DRAW);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glUseProgram(m_shaderProgram);

    float pointScale = glm::clamp(380.0f / camera.distance, 0.7f, 5.0f);
    glPointSize(14.0f * pointScale);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glVertexPointer(3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glColorPointer(4, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, color));

    if (useRotation) {
        // For rotation, we unfortunately still need some CPU work if we don't move it to the shader.
        // But for now, let's keep it simple and just update the VBO if rotation is active,
        // or better, use a shader uniform for rotation.
        // Actually, the simplest "minimal change" to fix the FPS is to just draw what we have.
        // If we want rotation, we should update the VBO each frame OR do it in the shader.
        // Let's try to do it in the shader for better performance.
        GLint timeLoc = glGetUniformLocation(m_shaderProgram, "u_time");
        GLint speedLoc = glGetUniformLocation(m_shaderProgram, "u_globalSpeed");
        GLint mLoc = glGetUniformLocation(m_shaderProgram, "u_m");
        GLint useRotLoc = glGetUniformLocation(m_shaderProgram, "u_useRotation");

        glUniform1f(timeLoc, timeVal);
        glUniform1f(speedLoc, globalSpeed);
        glUniform1f(mLoc, m_float);
        glUniform1i(useRotLoc, useRotation ? 1 : 0);
    } else {
        GLint useRotLoc = glGetUniformLocation(m_shaderProgram, "u_useRotation");
        glUniform1i(useRotLoc, 0);
    }
    
    GLint clipLoc = glGetUniformLocation(m_shaderProgram, "u_clipEnabled");
    glUniform1i(clipLoc, clipEnabled ? 1 : 0);

    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(cloudPoints.size()));

    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

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
