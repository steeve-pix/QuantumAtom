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

/*
 * Engine Constructor: Sets up the initial application state.
 */
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

/*
 * Engine Destructor: Safely cleans up resources.
 */
Engine::~Engine() {
    // Terminate the background thread before cleaning up OpenGL contexts.
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

/*
 * Legacy CPU-based heatmap calculation (preserved for reference).
 */
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
    if (t < 0.03f) alpha *= (t / 0.03f);

    return glm::vec4(color, alpha);
}

/*
 * Asynchronously regenerates the electron cloud on a background thread.
 * Uses Monte Carlo rejection sampling to match the probability density function.
 */
void Engine::regenerateCloud() {
    // Stop any ongoing generation process.
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

        /*
         * First, search for the maximum probability density in the current state.
         * This value is needed to normalize the rejection sampling.
         */
        float maxTestDensity = 0.0f;
        for (int i = 0; i < 3000 && !m_buildCancelled.load(); ++i) {
            float testR = (i < 1500)
                              ? peakR * (0.5f + dis(gen) * 1.5f)
                              : dis(gen) * maxR;
            float testTh = std::acos(2.0f * dis(gen) - 1.0f);
            float testPh = 2.0f * PI * dis(gen);
            maxTestDensity = std::max(maxTestDensity,
                                      QuantumSimulation::computeProbability(testR, testTh, testPh, capturedState));
        }
        if (maxTestDensity <= 1e-7f) maxTestDensity = 1.0f;
        maxTestDensity *= 1.15f;

        /*
         * Rejection Sampling Loop:
         * Generates random points and accepts them based on the probability distribution.
         */
        std::vector<CloudPoint> newCloud;
        newCloud.reserve(targetPoints);

        int maxAttempts = targetPoints * 25;
        int attempts = 0;

        while (static_cast<int>(newCloud.size()) < targetPoints
               && attempts < maxAttempts
               && !m_buildCancelled.load()) {
            ++attempts;
            float r = maxR * dis(gen);
            float theta = std::acos(2.0f * dis(gen) - 1.0f);
            float phi = 2.0f * PI * dis(gen);

            float density = QuantumSimulation::computeProbability(r, theta, phi, capturedState);
            float adjustedDensity = density * r * r;

            if (dis(gen) * (maxTestDensity * maxR * maxR) < adjustedDensity) {
                glm::vec3 pos(
                    r * std::sin(theta) * std::cos(phi),
                    r * std::cos(theta),
                    r * std::sin(theta) * std::sin(phi)
                );
                float spd = 0.3f + (dis(gen) * 2.2f);
                newCloud.push_back({pos, glm::vec3(0.0f), density, spd});

                // Periodic progress updates
                if ((newCloud.size() & 0xFF) == 0)
                    m_buildProgress.store((int) (newCloud.size() * 100 / targetPoints));
            }
        }

        if (m_buildCancelled.load()) return;

        // Fill remaining slots if sampling was incomplete
        while (static_cast<int>(newCloud.size()) < targetPoints)
            newCloud.push_back({{0, 0, 0}, {0, 0, 0}, 0.0f, 1.0f});

        float maxD = 1e-6f;
        for (const auto &p: newCloud)
            maxD = std::max(maxD, p.brightness);

        {
            // Atomically swap the new cloud into the standby buffer.
            std::lock_guard<std::mutex> lock(m_swapMutex);
            m_pendingCloud = std::move(newCloud);
            m_cachedMaxDensity = maxD;
        }
        m_buildProgress.store(100);
        m_cloudReady.store(true);
    });
}

/*
 * Returns the simulation to a default state (Hydrogen ground state).
 */
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
    std::cout << "[System] Simulation environment successfully reset.\n";
}

/*
 * Legacy function for single-point regeneration.
 */
// void Engine::regenerateSinglePoint(CloudPoint &p) {
//     const float maxR = 12.0f * static_cast<float>(state.n * state.n);
//     float r = m_dis(m_gen) * maxR * 0.98f;
//     float theta = std::acos(2.0f * m_dis(m_gen) - 1.0f);
//     float phi = 2.0f * PI * m_dis(m_gen);
//     p.pos = glm::vec3(r * std::sin(theta) * std::cos(phi),
//                       r * std::sin(theta) * std::sin(phi),
//                       r * std::cos(theta));
//     p.vel = glm::vec3(0.0f);
//     p.brightness = QuantumSimulation::computeProbability(r, theta, phi, state);
// }

void Engine::regenerateSinglePoint(CloudPoint &p) {
    // 1. Dynamic bounding radius based on your current scaling
    const float maxR = 12.0f * static_cast<float>(state.n * state.n);

    // 2. We need a target to compare the probability against.
    // Since max density drops sharply as 'n' increases, we scale our maximum target.
    // For n=4, a target max around 0.0001f to 0.001f works well, or use your m_cachedMaxDensity.
    float maxTargetProb = m_cachedMaxDensity;

    while (true) {
        // Pick a random spot in spherical coordinates
        float r = std::cbrt(m_dis(m_gen)) * maxR;
        float theta = std::acos(2.0f * m_dis(m_gen) - 1.0f);
        float phi = 2.0f * PI * m_dis(m_gen);

        // Compute the probability at this exact spot
        float prob = QuantumSimulation::computeProbability(r, theta, phi, state);

        // Standard Rejection Sampling:
        // Pick a random threshold. If the probability is higher, ACCEPT the point!
        float threshold = m_dis(m_gen) * maxTargetProb;
        if (prob > threshold) {

            // Map the accepted coordinates to 3D Cartesian space
            p.pos = glm::vec3(r * std::sin(theta) * std::cos(phi),
                              r * std::sin(theta) * std::sin(phi),
                              r * std::cos(theta));
            p.vel = glm::vec3(0.0f);

            // Store the raw probability or normalized brightness for your shader
            p.brightness = prob;

            return; // We found a valid point, exit the function!
        }
    }
}

/*
 * Logic for frame-by-frame updates of visual elements (like the orbital sphere).
 */
void Engine::updatePhysics(float deltaTime) {
    float orbitSpeed = 4.5f / static_cast<float>(state.n * state.n);
    electronAngle += orbitSpeed * deltaTime;
    if (electronAngle > 2.0f * PI)
        electronAngle = std::fmod(electronAngle, 2.0f * PI);
}

/*
 * Shader compilation helper: Processes GLSL source into an OpenGL shader object.
 */
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
        std::cerr << "[Shader Error] " << infoLog << "\n";
    }
    return shader;
}

/*
 * Shader System:
 * We move rotation and color calculation to the GPU (vertex shader) 
 * for maximum performance.
 */
void Engine::initShaders() {
    // Vertex Shader: Handles 3D transformations and heatmap logic.
    const std::string vertSrc = R"GLSL(
#version 120
attribute vec3  aPos;
attribute float aNorm;
attribute float aSpeed;

uniform float uTime;
uniform float uGlobalSpeed;
uniform float uMFloat;
uniform int   uUseRotation;
uniform float uPointScale;
uniform int   uClipEnabled;

varying float vNorm;
varying float vDiscard;

void main() {
    vec3 pos = aPos;

    if (uUseRotation == 1) {
        float angle = uTime * uGlobalSpeed * (0.15 + aNorm * 3.5) * uMFloat;
        float s = sin(angle);
        float c = cos(angle);
        float newX = pos.x * c - pos.z * s;
        float newZ = pos.x * s + pos.z * c;
        pos.x = newX;
        pos.z = newZ;
    }

    vDiscard = (uClipEnabled == 1 && pos.y > 0.0 && pos.z > 0.0) ? 1.0 : 0.0;
    vNorm    = aNorm;

    // GLSL Heatmap Implementation
    float t  = pow(aNorm, 0.35);
    float s6 = t * 6.0;
    int   ci = int(s6);
    float lt = s6 - float(ci);

    vec3 c0 = vec3(0.280, 0.000, 0.550);
    vec3 c1 = vec3(0.450, 0.000, 0.650);
    vec3 c2 = vec3(0.800, 0.000, 0.550);
    vec3 c3 = vec3(0.950, 0.050, 0.050);
    vec3 c4 = vec3(1.000, 0.500, 0.000);
    vec3 c5 = vec3(1.000, 0.900, 0.000);
    vec3 c6 = vec3(1.000, 1.000, 0.850);

    vec3 col;
    if      (ci == 0) col = mix(c0, c1, lt);
    else if (ci == 1) col = mix(c1, c2, lt);
    else if (ci == 2) col = mix(c2, c3, lt);
    else if (ci == 3) col = mix(c3, c4, lt);
    else if (ci == 4) col = mix(c4, c5, lt);
    else              col = mix(c5, c6, lt);

    float alpha = pow(t, 0.60) * 0.95;
    if (t < 0.03) alpha *= (t / 0.03);

    gl_FrontColor = vec4(col, alpha);
    gl_PointSize  = 14.0 * uPointScale;
    gl_Position   = gl_ModelViewProjectionMatrix * vec4(pos, 1.0);
}
)GLSL";

    // Fragment Shader: Handles alpha blending and clipping.
    const std::string fragSrc = R"GLSL(
#version 120
varying float vNorm;
varying float vDiscard;

void main() {
    if (vDiscard > 0.5) discard;
    if (vNorm < 0.0001) discard;

    vec2  cc     = gl_PointCoord - vec2(0.5);
    float distSq = dot(cc, cc);
    if (distSq > 0.25) discard;

    float a = smoothstep(0.25, 0.0, distSq);
    gl_FragColor = vec4(gl_Color.rgb, gl_Color.a * a);
}
)GLSL";

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
        std::cerr << "[Shader Link Error] " << log << "\n";
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    // Cache location handles
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

/*
 * User Interface:
 * Renders the ImGui control panel and performance metrics overlay.
 */
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

    // Performance HUD
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
        ImGui::Text("Points:  %d / %d", (int) cloudPoints.size(), maxPoints);
        bool building = m_buildThread.joinable() && !m_cloudReady.load();
        if (building) {
            int pct = m_buildProgress.load();
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Building cloud...");
            char buf[16];
            snprintf(buf, sizeof(buf), "%d%%", pct);
            ImGui::ProgressBar(pct / 100.0f, ImVec2(-1.0f, 0.0f), buf);
        }
    }
    ImGui::End();

    // Main Control Panel
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

    // Credits Overlay
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

/*
 * Render Thread: Handles the primary drawing loop and thread synchronization.
 */
void Engine::drawScene(float currentFrameTime, float deltaTime) {
    // Check if the background thread has finished generating a new cloud.
    if (m_cloudReady.load()) {
        {
            std::lock_guard<std::mutex> lock(m_swapMutex);
            cloudPoints = std::move(m_pendingCloud);
        }
        m_cloudReady.store(false);
        m_vboDirty = true; // Signal for GPU buffer update.
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

/*
 * Window Management: Sets up the GLFW window and graphics context.
 */
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

/*
 * Graphics API Initialization: Loads OpenGL functions and sets global state.
 */
void Engine::initOpenGL() {
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        throw std::runtime_error("Failed to initialize GLAD.");

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(0x8861); // GL_POINT_SPRITE

    initShaders();
}

/*
 * UI Initialization: Sets up the Dear ImGui context and backends.
 */
void Engine::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL2_Init();
}

/*
 * Event Handlers: Connects GLFW input events to engine logic.
 */
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
                eng->state.m = glm::clamp(eng->state.m, -eng->state.l, eng->state.l);
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
                eng->camera.targetPitch = glm::clamp(eng->camera.targetPitch, -89.0f, 89.0f);
            }
        }
    });

    glfwSetScrollCallback(window, [](GLFWwindow *win, double xoff, double yoff) {
        ImGui_ImplGlfw_ScrollCallback(win, xoff, yoff);
        if (!ImGui::GetIO().WantCaptureMouse) {
            auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
            eng->camera.targetDistance -= static_cast<float>(yoff) * 22.0f;
            eng->camera.targetDistance = glm::clamp(eng->camera.targetDistance, 60.0f, 1400.0f);
        }
    });

    glfwSetCharCallback(window, [](GLFWwindow *win, unsigned int cp) {
        ImGui_ImplGlfw_CharCallback(win, cp);
    });
}

/*
 * World Axes: Renders the RGB lines representing X, Y, and Z.
 */
void Engine::drawAxes() {
    glPushMatrix();
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(1, 0, 0); // X-axis (Red)
    glVertex3f(0, 0, 0);
    glVertex3f(120, 0, 0);
    glColor3f(0, 1, 0); // Y-axis (Green)
    glVertex3f(0, 0, 0);
    glVertex3f(0, 120, 0);
    glColor3f(0, 0, 1); // Z-axis (Blue)
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0, 120);
    glEnd();
    glPopMatrix();
}

/*
 * Cloud Rendering:
 * Transforms the particle data into high-performance GPU buffers 
 * and issues the drawing command.
 */
void Engine::drawCloud(float timeVal) {
    if (cloudPoints.empty()) return;

    // Buffer Update: Only executed when the particle cloud changes.
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
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (positions.size() * sizeof(float)), positions.data(),
                     GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, m_normVbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (norms.size() * sizeof(float)), norms.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, m_speedVbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (speeds.size() * sizeof(float)), speeds.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_vboDirty = false;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    glUseProgram(m_shaderProgram);

    // Uniform Updates
    float pointScale = glm::clamp(380.0f / camera.distance, 0.7f, 5.0f);
    glUniform1f(m_uTime, timeVal);
    glUniform1f(m_uGlobalSpeed, 5.0f / static_cast<float>(state.n));
    glUniform1f(m_uMFloat, static_cast<float>(state.m));
    glUniform1i(m_uUseRotation, (state.m != 0) ? 1 : 0);
    glUniform1f(m_uPointScale, pointScale);
    glUniform1i(m_uClipEnabled, clipEnabled ? 1 : 0);

    // Attribute Binding
    glBindBuffer(GL_ARRAY_BUFFER, m_posVbo);
    glEnableVertexAttribArray(m_attrPos);
    glVertexAttribPointer(m_attrPos, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, m_normVbo);
    glEnableVertexAttribArray(m_attrNorm);
    glVertexAttribPointer(m_attrNorm, 1, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, m_speedVbo);
    glEnableVertexAttribArray(m_attrSpeed);
    glVertexAttribPointer(m_attrSpeed, 1, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Single Draw Call for all particles
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(cloudPoints.size()));

    glDisableVertexAttribArray(m_attrPos);
    glDisableVertexAttribArray(m_attrNorm);
    glDisableVertexAttribArray(m_attrSpeed);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUseProgram(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

/*
 * Visual Overlay: Draws the traveling sphere following classical orbital paths.
 */
void Engine::drawActiveElectron() {
    float radius = 14.5f * static_cast<float>(state.n * state.n);
    float x = radius * std::cos(electronAngle);
    float y = (state.l > 0) ? radius * std::sin(electronAngle) * 0.5f : 0.0f;
    float z = (state.l > 0)
                  ? radius * std::sin(electronAngle) * std::sqrt(1.0f - 0.5f * 0.5f)
                  : radius * std::sin(electronAngle);

    glPointSize(12.0f);
    glBegin(GL_POINTS);
    glColor4f(1, 1, 1, 1);
    glVertex3f(x, y, z);
    glEnd();
}
