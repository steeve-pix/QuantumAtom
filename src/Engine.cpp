#include "Engine.h"
#include "QuantumSimulation.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl2.h>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>

#ifndef PI
#define PI 3.141592653589793238462643383279502884f
#endif

namespace {
std::string loadShaderSource(const std::string &fileName) {
    const std::string shaderPath = "shaders/" + fileName;
    const std::array<std::string, 3> candidates = {
        shaderPath,
        "../" + shaderPath,
        "../../" + shaderPath
    };

    for (const auto &path: candidates) {
        std::ifstream file(path, std::ios::in);
        if (file) {
            std::ostringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }
    }

    throw std::runtime_error("Unable to load shader file: " + shaderPath);
}
}

// Constructor: Initializes window, OpenGL, ImGui, and starts the initial cloud generation
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

// Destructor: Ensures background threads are joined and OpenGL resources are released
Engine::~Engine() {
    m_buildCancelled.store(true);
    if (m_buildThread.joinable())
        m_buildThread.join();

    if (m_posVbo)
        glDeleteBuffers(1, &m_posVbo);
    if (m_normVbo)
        glDeleteBuffers(1, &m_normVbo);
    if (m_speedVbo)
        glDeleteBuffers(1, &m_speedVbo);
    if (m_shaderProgram)
        glDeleteProgram(m_shaderProgram);
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

// Legacy CPU-side heatmap lookup (kept for historical reference/debugging)
glm::vec4 Engine::heatmapFire(float value) {
    float clamp_v = std::clamp(value, 0.0f, 1.0f);
    float t = powf(clamp_v, 0.35f);

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

    glm::vec3 color = mix(stops[i], stops[next_i], local_t);
    float alpha = powf(t, 0.60f) * 0.95f;
    if (t < 0.03f) alpha *= (t / 0.03f);

    return {glm::vec4(color, alpha)};
}

// Starts a background thread to generate new probability cloud points via rejection sampling
void Engine::regenerateCloud() {
    m_buildCancelled.store(true);
    if (m_buildThread.joinable())
        m_buildThread.join();

    m_buildCancelled.store(false);
    m_cloudReady.store(false);
    m_buildProgress.store(0);

    QuantumState capturedState = state;
    int targetPoints = maxPoints;

    m_buildThread = std::thread([this, capturedState, targetPoints]() {
        std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);

        const float a0 = 4.0f;
        const float maxR = 6.0f * static_cast<float>(capturedState.n * capturedState.n) * 1.5f;
        const float peakR = static_cast<float>(capturedState.n * capturedState.n) * a0;

        // Estimate maximum probability density for the current state to normalize sampling
        float maxTestDensity = 0.0f;
        for (int i = 0; i < 3000 && !m_buildCancelled.load(); ++i) {
            float testR = (i < 1500)
                              ? peakR * (0.5f + dis(gen) * 1.5f)
                              : dis(gen) * maxR;
            float testTh = acosf(2.0f * dis(gen) - 1.0f);
            float testPh = 2.0f * PI * dis(gen);
            maxTestDensity = std::max(maxTestDensity,
                                      QuantumSimulation::computeProbability(testR, testTh, testPh, capturedState));
        }
        if (maxTestDensity <= 1e-7f) maxTestDensity = 1.0f;
        maxTestDensity *= 1.15f;

        std::vector<CloudPoint> newCloud;
        newCloud.reserve(targetPoints);

        int maxAttempts = targetPoints * 25;
        int attempts = 0;

        // Monte Carlo rejection sampling loop
        while (static_cast<int>(newCloud.size()) < targetPoints
               && attempts < maxAttempts
               && !m_buildCancelled.load()) {
            ++attempts;
            float r = maxR * dis(gen);
            float theta = acosf(2.0f * dis(gen) - 1.0f);
            float phi = 2.0f * PI * dis(gen);

            float density = QuantumSimulation::computeProbability(r, theta, phi, capturedState);
            float adjustedDensity = density * r * r;

            if (dis(gen) * (maxTestDensity * maxR * maxR) < adjustedDensity) {
                glm::vec3 pos(
                    r * sinf(theta) * cosf(phi),
                    r * cosf(theta),
                    r * sinf(theta) * sinf(phi)
                );
                float spd = 0.3f + (dis(gen) * 2.2f);
                newCloud.push_back({pos, glm::vec3(0.0f), density, spd});

                if ((newCloud.size() & 0xFF) == 0)
                    m_buildProgress.store(static_cast<int>(newCloud.size() * 100 / targetPoints));
            }
        }

        if (m_buildCancelled.load()) return;

        // Pad the vector if we didn't reach the target number of points
        while (static_cast<int>(newCloud.size()) < targetPoints)
            newCloud.push_back({{0, 0, 0}, {0, 0, 0}, 0.0f, 1.0f});

        float maxD = 1e-6f;
        for (const auto &p: newCloud)
            maxD = std::max(maxD, p.brightness);

        {
            // Thread-safe swap of the generated cloud
            std::lock_guard<std::mutex> lock(m_swapMutex);
            m_pendingCloud = std::move(newCloud);
            m_cachedMaxDensity = maxD;
        }
        m_buildProgress.store(100);
        m_cloudReady.store(true);
    });
}

// Resets all simulation parameters to ground state (n=1, l=0, m=0)
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

// Synchronous generation of a single point (legacy method)
void Engine::regenerateSinglePoint(CloudPoint &p) {
    const float maxR = 12.0f * static_cast<float>(state.n * state.n);
    float maxTargetProb = m_cachedMaxDensity;

    while (true) {
        float r = cbrtf(m_dis(m_gen)) * maxR;
        float theta = acosf(2.0f * m_dis(m_gen) - 1.0f);
        float phi = 2.0f * PI * m_dis(m_gen);

        float prob = QuantumSimulation::computeProbability(r, theta, phi, state);
        float threshold = m_dis(m_gen) * maxTargetProb;
        if (prob > threshold) {
            p.pos = glm::vec3(r * sinf(theta) * cosf(phi),
                         r * sinf(theta) * sinf(phi),
                         r * cosf(theta));
            p.vel = glm::vec3(0.0f);
            p.brightness = prob;
            return;
        }
    }
}

// Updates time-based physics variables for the current frame
void Engine::updatePhysics(float deltaTime) {
    float orbitSpeed = 4.5f / static_cast<float>(state.n * state.n);
    electronAngle += orbitSpeed * deltaTime;
    if (electronAngle > 2.0f * PI)
        electronAngle = fmodf(electronAngle, 2.0f * PI);
}

// Utility to compile and check for errors in GLSL shaders
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
        std::cerr << "[Shader Error] " << infoLog << std::endl;
    }
    return shader;
}

// Compiles and links the GLSL program used for point cloud rendering
void Engine::initShaders() {
    const std::string vertSrc = loadShaderSource("cloud.vert.glsl");
    const std::string fragSrc = loadShaderSource("cloud.frag.glsl");

    GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vs);
    glAttachShader(m_shaderProgram, fs);
    glLinkProgram(m_shaderProgram);

    GLint ok;
    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(m_shaderProgram, 512, nullptr, log);
        std::cerr << "[Shader Link Error] " << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    m_attrPos = glGetAttribLocation(m_shaderProgram, "aPos");
    m_attrNorm = glGetAttribLocation(m_shaderProgram, "aNorm");
    m_attrSpeed = glGetAttribLocation(m_shaderProgram, "aSpeed");
    m_uTime = glGetUniformLocation(m_shaderProgram, "uTime");
    m_uGlobalSpeed = glGetUniformLocation(m_shaderProgram, "uGlobalSpeed");
    m_uMFloat = glGetUniformLocation(m_shaderProgram, "uMFloat");
    m_uUseRotation = glGetUniformLocation(m_shaderProgram, "uUseRotation");
    m_uPointScale = glGetUniformLocation(m_shaderProgram, "uPointScale");
    m_uClipEnabled = glGetUniformLocation(m_shaderProgram, "uClipEnabled");
}

// Renders the ImGui interface including performance stats and theory information
void Engine::renderUI() {
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiIO &io = ImGui::GetIO();

    double currentTime = glfwGetTime();
    m_frameCount++;
    if (currentTime - m_lastFpsUpdateTime >= 0.5) {
        m_fps = static_cast<float>(m_frameCount) /
                static_cast<float>(currentTime - m_lastFpsUpdateTime);
        m_frameTimeMs = 1000.0f / m_fps;
        m_frameCount = 0;
        m_lastFpsUpdateTime = currentTime;
    }

    // Performance statistics HUD
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 15.0f, 15.0f),
                            ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration |
                                ImGuiWindowFlags_AlwaysAutoResize |
                                ImGuiWindowFlags_NoSavedSettings |
                                ImGuiWindowFlags_NoFocusOnAppearing |
                                ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("HUD", nullptr, hudFlags)) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "SYSTEM METRICS");
        ImGui::Separator();
        ImGui::Text("Performance:   %.1f ms", m_frameTimeMs);
        ImGui::Text("FPS:     %.2f", m_fps);
        ImGui::Text("Points:  %d / %d", static_cast<int>(cloudPoints.size()), maxPoints);
        bool building = m_buildThread.joinable() && !m_cloudReady.load();
        if (building) {
            int pct = m_buildProgress.load();
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Building cloud...");
            char buf[16];
            snprintf(buf, sizeof(buf), "%d%%", pct);
            ImGui::ProgressBar(static_cast<float>(pct) / 100.0f, ImVec2(-1.0f, 0.0f), buf);
        }
    }
    ImGui::End();

    // Main control and information panel
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(420, 680), ImGuiCond_Once);
    ImGui::Begin("Quantum Configuration & Information", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "QUANTUM NUMBERS CONTROL");
    ImGui::Separator();

    bool stateChanged = false;

    ImGui::SliderInt("Principle (n)", &state.n, 1, 6);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        stateChanged = true;
        if (state.l >= state.n) state.l = state.n - 1;
        state.m = std::clamp(state.m, -state.l, state.l);
    }
    ImGui::TextDisabled("Defines energy shell and size limit boundaries.");

    ImGui::SliderInt("Azimuthal (l)", &state.l, 0, state.n - 1);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        stateChanged = true;
        state.m = std::clamp(state.m, -state.l, state.l);
    }
    ImGui::TextDisabled("Defines the subshell shape layout geometry (s, p, d, f).");

    ImGui::SliderInt("Magnetic (m)", &state.m, -state.l, state.l);
    if (ImGui::IsItemDeactivatedAfterEdit()) stateChanged = true;
    ImGui::TextDisabled("Defines spatial orientation axes constraints.");

    ImGui::Spacing();
    ImGui::Checkbox("Enable Cross-Section Clip", &clipEnabled);

    if (stateChanged) regenerateCloud();

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "WHAT ARE YOU LOOKING AT?");
    ImGui::Separator();

    ImGui::BeginChild("TheoryScroll", ImVec2(0, 0), true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::SeparatorText("The Quantum Probability Cloud");
    ImGui::TextWrapped(
        "This particle cloud is a 3D solution to the time-independent Schrodinger Equation "
        "for a Hydrogen-like atom.");
    ImGui::Spacing();
    ImGui::BulletText("Wave Function (Psi):");
    ImGui::TextWrapped(
        "Electrons do not travel in planetary rings. They exist as a standing wave of probability. "
        "The denser the cloud points are in a region, the higher the mathematical likelihood "
        "of finding the electron there upon measurement.");
    ImGui::Spacing();
    ImGui::BulletText("Hydrogen-Like Analytical Solution:");
    ImGui::TextWrapped(
        "By simulating a single-electron system, we omit chaotic electron-electron repulsions. "
        "This isolates pure electrostatic attraction, rendering an exact mathematical representation "
        "of atomic geometry.");
    ImGui::Spacing();
    ImGui::SeparatorText("The Classical Simulation Tracker");
    ImGui::TextWrapped(
        "The bright traveling sphere represents a localized simulation snapshot tracker showing "
        "the dynamic orbital path tracking across calculated classical kinetic constraints.");
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "Bridging Two Frameworks:");
    ImGui::TextWrapped(
        "Because the Heisenberg Uncertainty Principle forbids an exact path for quantum waves, "
        "this sphere is an analytical overlay. It maps classical momentum, velocity gradients, "
        "and potential energy fields as a localized point riding through the probability cloud, "
        "making the abstract math intuitive to human sight.");
    ImGui::EndChild();
    ImGui::End();

    // Credits section
    ImGui::SetNextWindowPos(
        ImVec2(static_cast<float>(m_width) - 15.0f,
               static_cast<float>(m_height) - 15.0f),
        ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGuiWindowFlags credFlags = ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("Credits", nullptr, credFlags)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
        ImGui::Text("Credits to");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.7f, 1.0f), "kavan010");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::Text("GitHub: https://github.com/kavan010/");
        ImGui::PopStyleColor();
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

// Orchestrates the rendering of the entire scene
void Engine::drawScene(float currentFrameTime, float deltaTime) {
    if (m_cloudReady.load()) {
        {
            std::lock_guard<std::mutex> lock(m_swapMutex);
            cloudPoints = std::move(m_pendingCloud);
        }
        m_cloudReady.store(false);
        m_vboDirty = true;
    }

    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    camera.update(m_width, m_height, deltaTime);

    drawAxes();

    glPushMatrix();
    drawCloud(currentFrameTime);
    glPopMatrix();

    renderUI();
}

// Initializes GLFW and creates the main application window
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

// Initializes Glad and configures basic OpenGL state
void Engine::initOpenGL() {
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        throw std::runtime_error("Failed to initialize GLAD.");

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(0x8861); // GL_POINT_SPRITE

    initShaders();
}

// Configures the ImGui context and backends
void Engine::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL2_Init();
}

// Sets up user input callbacks for the GLFW window
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

        if (!ImGui::GetIO().WantCaptureKeyboard &&
            (action == GLFW_PRESS || action == GLFW_REPEAT)) {
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
                eng->state.m = std::clamp(eng->state.m, -eng->state.l, eng->state.l);
                changed = true;
            }
            if (key == GLFW_KEY_C && action == GLFW_PRESS)
                eng->clipEnabled = !eng->clipEnabled;

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
        float dx = static_cast<float>(xpos) - eng->m_lastMouseX;
        float dy = eng->m_lastMouseY - static_cast<float>(ypos);
        eng->m_lastMouseX = static_cast<float>(xpos);
        eng->m_lastMouseY = static_cast<float>(ypos);

        if (eng->m_mouseButtonDown != -1 && !ImGui::GetIO().WantCaptureMouse) {
            bool shift = (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                          glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
            if (shift || eng->m_mouseButtonDown == GLFW_MOUSE_BUTTON_RIGHT)
                eng->camera.pan(dx, dy);
            else if (eng->m_mouseButtonDown == GLFW_MOUSE_BUTTON_LEFT) {
                eng->camera.targetYaw += dx * 0.18f;
                eng->camera.targetPitch += dy * 0.18f;
                eng->camera.targetPitch = std::clamp(eng->camera.targetPitch, -89.0f, 89.0f);
            }
        }
    });

    glfwSetScrollCallback(window, [](GLFWwindow *win, double xoff, double yoff) {
        ImGui_ImplGlfw_ScrollCallback(win, xoff, yoff);
        if (!ImGui::GetIO().WantCaptureMouse) {
            auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
            eng->camera.targetDistance -= static_cast<float>(yoff) * 22.0f;
            eng->camera.targetDistance = std::clamp(eng->camera.targetDistance, 60.0f, 1400.0f);
        }
    });

    glfwSetCharCallback(window, [](GLFWwindow *win, unsigned int cp) {
        ImGui_ImplGlfw_CharCallback(win, cp);
    });
}

// Draws world coordinate axes (X: Red, Y: Green, Z: Blue)
void Engine::drawAxes() {
    glPushMatrix();
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(1, 0, 0);
    glVertex3f(0, 0, 0);
    glVertex3f(120, 0, 0);
    glColor3f(0, 1, 0);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 120, 0);
    glColor3f(0, 0, 1);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0, 120);
    glEnd();
    glPopMatrix();
}

// Uploads cloud data to GPU and renders points using the custom shader
void Engine::drawCloud(float timeVal) {
    if (cloudPoints.empty()) return;

    if (m_vboDirty) {
        const size_t n = cloudPoints.size();

        float maxD = 1e-6f;
        for (const auto &p: cloudPoints) maxD = std::max(maxD, p.brightness);
        float invMax = 1.0f / maxD;

        std::vector<float> positions(n * 3);
        std::vector<float> norms(n);
        std::vector<float> speeds(n);

        for (size_t i = 0; i < n; ++i) {
            const auto &p = cloudPoints[i];
            positions[i * 3 + 0] = p.pos.x;
            positions[i * 3 + 1] = p.pos.y;
            positions[i * 3 + 2] = p.pos.z;
            norms[i] = p.brightness * invMax;
            speeds[i] = p.speedFactor;
        }

        if (!m_posVbo)
            glGenBuffers(1, &m_posVbo);
        if (!m_normVbo)
            glGenBuffers(1, &m_normVbo);
        if (!m_speedVbo)
            glGenBuffers(1, &m_speedVbo);

        glBindBuffer(GL_ARRAY_BUFFER, m_posVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(positions.size() * sizeof(float)), positions.data(),
                     GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, m_normVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(norms.size() * sizeof(float)), norms.data(),
                     GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, m_speedVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(speeds.size() * sizeof(float)), speeds.data(),
                     GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_vboDirty = false;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    glUseProgram(m_shaderProgram);

    float pointScale = std::clamp(380.0f / camera.distance, 0.7f, 5.0f);
    glUniform1f(m_uTime, timeVal);
    glUniform1f(m_uGlobalSpeed, 5.0f / static_cast<float>(state.n));
    glUniform1f(m_uMFloat, static_cast<float>(state.m));
    glUniform1i(m_uUseRotation, (state.m != 0) ? 1 : 0);
    glUniform1f(m_uPointScale, pointScale);
    glUniform1i(m_uClipEnabled, clipEnabled ? 1 : 0);

    glBindBuffer(GL_ARRAY_BUFFER, m_posVbo);
    glEnableVertexAttribArray(m_attrPos);
    glVertexAttribPointer(m_attrPos, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, m_normVbo);
    glEnableVertexAttribArray(m_attrNorm);
    glVertexAttribPointer(m_attrNorm, 1, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, m_speedVbo);
    glEnableVertexAttribArray(m_attrSpeed);
    glVertexAttribPointer(m_attrSpeed, 1, GL_FLOAT, GL_FALSE, 0, nullptr);

    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(cloudPoints.size()));

    glDisableVertexAttribArray(m_attrPos);
    glDisableVertexAttribArray(m_attrNorm);
    glDisableVertexAttribArray(m_attrSpeed);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUseProgram(0);
}

// Renders a single white point to represent a classically tracked electron
void Engine::drawActiveElectron() {
    float radius = 14.5f * static_cast<float>(state.n * state.n);
    float x = radius * cosf(electronAngle);
    float y = (state.l > 0) ? radius * sinf(electronAngle) * 0.5f : 0.0f;
    float z = (state.l > 0)
                  ? radius * sinf(electronAngle) * sqrtf(1.0f - 0.5f * 0.5f)
                  : radius * sinf(electronAngle);

    glPointSize(12.0f);
    glBegin(GL_POINTS);
    glColor4f(1, 1, 1, 1);
    glVertex3f(x, y, z);
    glEnd();
}
