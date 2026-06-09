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
#define PI 3.14159265358979323846f
#endif

// Engine constructor: Initializes subsystems and prepares the initial simulation state.
Engine::Engine(int width, int height, const std::string &title)
    : m_width(width), m_height(height), m_title(title), m_dis(0.0f, 1.0f) {
    std::random_device rd;
    m_gen = std::mt19937(rd()); // Seed the random number generator

    initGlfwWindow(); // Setup GLFW and create the window
    initOpenGL(); // Initialize GLAD and basic GL states
    initImGui(); // Setup the ImGui context and backends
    setupCallbacks(); // Configure input and window callbacks
    regenerateCloud(); // Generate the first orbital cloud (1s state)
}

// Engine destructor: Cleans up window and GLFW resources.
Engine::~Engine() {
    if (m_shaderProgram) {
        glDeleteProgram(m_shaderProgram);
    }
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

// Implements a professional heatmap color mapping using an Inferno-like palette.
glm::vec4 Engine::heatmapFire(float value) {
    // Dynamic Gamma-Correction for contrast at low-probability areas
    float clamp_v = std::clamp(value, 0.0f, 1.0f);
    float t = std::pow(clamp_v, 0.35f);

    // High-fidelity color stops: Highest probability (1.0) is bright white/yellow,
    // fading through yellow, orange, red, magenta, purple to dark purple (0.0).
    const int num_stops = 7;
    static const glm::vec3 stops[num_stops] = {
        {0.280f, 0.000f, 0.550f}, // 0: Luminous Indigo (Was dull dark purple)
        {0.450f, 0.000f, 0.650f}, // 1: Vibrant Purple
        {0.800f, 0.000f, 0.550f}, // 2: Neon Magenta / Hot Pink
        {0.950f, 0.050f, 0.050f}, // 3: Bright Red
        {1.000f, 0.500f, 0.000f}, // 4: Intense Orange
        {1.000f, 0.900f, 0.000f}, // 5: Yellow
        {1.000f, 1.000f, 0.850f} // 6: Very Bright Yellow/White
    };

    float scaled_v = t * (num_stops - 1);
    int i = static_cast<int>(scaled_v);
    int next_i = std::min(i + 1, num_stops - 1);
    float local_t = scaled_v - static_cast<float>(i);

    // Linear interpolation between color stops
    glm::vec3 color = glm::mix(stops[i], stops[next_i], local_t);

    // Dynamic Alpha: Non-linear mapping to emphasize core orbital structures
    float alpha = std::pow(t, 0.60f) * 0.95f;
    if (t < 0.03f) alpha *= (t / 0.03f); // Soft fade-out for noise

    return glm::vec4(color, alpha);
}

// Monte Carlo sampling to generate the probability density cloud.
void Engine::regenerateCloud() {
    cloudPoints.clear();
    cloudPoints.reserve(maxPoints);

    // Scale the maximum sampling radius based on the principal quantum number n
    const float maxR = 6.0f * static_cast<float>(state.n * state.n) * 2.0f;

    // Step 1: Find an approximate peak density to normalize the sampling budget
    float maxTestDensity = 0.0f;
    for (int i = 0; i < 600; ++i) {
        float testR = m_dis(m_gen) * maxR;
        float testTh = std::acos(2.0f * m_dis(m_gen) - 1.0f);
        float testPh = 2.0f * PI * m_dis(m_gen);
        maxTestDensity = std::max(maxTestDensity, QuantumSimulation::computeProbability(testR, testTh, testPh, state));
    }
    if (maxTestDensity <= 1e-7f) maxTestDensity = 1.0f;

    // Step 2: Sampling loop (Rejection Sampling)
    int maxAttempts = maxPoints * 10;
    int attempts = 0;

    while (static_cast<int>(cloudPoints.size()) < maxPoints && attempts < maxAttempts) {
        attempts++;
        float u = m_dis(m_gen);
        float r = maxR * std::pow(u, 1.5f);

        float theta = std::acos(2.0f * m_dis(m_gen) - 1.0f);
        float phi = 2.0f * PI * m_dis(m_gen);

        float density = QuantumSimulation::computeProbability(r, theta, phi, state);

        float compensation = (u > 0.0001f) ? (1.5f * std::sqrt(u)) : 1.0f;
        float adjustedDensity = density * compensation;

        // Only keep the point if it passes the probability threshold
        if (m_dis(m_gen) * maxTestDensity < adjustedDensity) {
            glm::vec3 pos(r * std::sin(theta) * std::cos(phi), r * std::cos(theta),
                          r * std::sin(theta) * std::sin(phi));

            float individualSpeed = 0.3f + (m_dis(m_gen) * 2.2f);
            cloudPoints.push_back({pos, glm::vec3(0.0f), density, individualSpeed});
        }
    }

    // Pad the cloud with empty points if the budget wasn't met
    while (static_cast<int>(cloudPoints.size()) < maxPoints) {
        cloudPoints.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f});
    }
}

// Resets all simulation parameters and the camera to default values.
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

// Re-generates a single point in the cloud (e.g., for dynamic replacement).
void Engine::regenerateSinglePoint(CloudPoint &p) {
    const float maxR = 12.0f * static_cast<float>(state.n * state.n);
    float r = m_dis(m_gen) * maxR * 0.98f;
    float theta = std::acos(2.0f * m_dis(m_gen) - 1.0f);
    float phi = 2.0f * PI * m_dis(m_gen);
    p.pos = glm::vec3(r * std::sin(theta) * std::cos(phi), r * std::sin(theta) * std::sin(phi), r * std::cos(theta));
    p.vel = glm::vec3(0.0f);
    p.brightness = QuantumSimulation::computeProbability(r, theta, phi, state);
}

// Updates time-dependent simulation variables (e.g., electron orbit angle).
void Engine::updatePhysics(float deltaTime) {
    float orbitSpeed = 4.5f / static_cast<float>(state.n * state.n);
    electronAngle += orbitSpeed * deltaTime;
    if (electronAngle > 2.0f * PI) electronAngle -= 2.0f * PI;
}

// Helper compile utility
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

// Initializes programmatic styling for points to make them perfectly soft round orbs
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
            "    // Convert square point primitives into smooth anti-aliased mathematical circles\n"
            "    vec2 circCoord = gl_PointCoord - vec2(0.5);\n"
            "    float distSq = dot(circCoord, circCoord);\n"
            "    if (distSq > 0.25) discard;\n" // Cut off outer square corners
            "    \n"
            "    // Create a beautiful gaussian radial falloff density signature\n"
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

// Renders the main ImGui configuration panel and theory section.
void Engine::renderUI() {
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiIO &io = ImGui::GetIO();

    // --- Dynamic FPS and Frame Time Calculation ---
    double currentTime = glfwGetTime();
    m_frameCount++;
    if (currentTime - m_lastFpsUpdateTime >= 0.5) {
        // Update every 500ms for stability
        m_fps = static_cast<float>(m_frameCount) / static_cast<float>(currentTime - m_lastFpsUpdateTime);
        m_frameTimeMs = 1000.0f / m_fps;
        m_frameCount = 0;
        m_lastFpsUpdateTime = currentTime;
    }

    // =========================================================================
    // --- NEW HUD LEGEND (TOP RIGHT) ---
    // =========================================================================
    float hudMargin = 15.0f;
    ImVec2 hudPos = ImVec2(io.DisplaySize.x - hudMargin, hudMargin);
    ImVec2 hudPivot = ImVec2(1.0f, 0.0f); // Pivot top-right corner
    ImGui::SetNextWindowPos(hudPos, ImGuiCond_Always, hudPivot);
    ImGui::SetNextWindowBgAlpha(0.55f); // Semi-transparent overlay

    ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("Performance & Math HUD", nullptr, hudFlags)) {
        // System Metrics Subheading
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "SYSTEM METRICS");
        ImGui::Separator();
        ImGui::Text("Performance: %.1f ms", m_frameTimeMs);
        ImGui::Text("Frame Time:  %.2f FPS", m_fps);
        ImGui::Text("Cloud Density: %d / %d Points", static_cast<int>(cloudPoints.size()), maxPoints);

        ImGui::Spacing();
        ImGui::Spacing();
    }
    ImGui::End();


    // =========================================================================
    // --- CONFIGURATION CONTROL PANEL (LEFT) ---
    // =========================================================================
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(420, 680), ImGuiCond_Once);
    ImGui::Begin("Quantum Configuration & Information", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // --- SECTION 1: QUANTUM NUMBERS CONTROL ---
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "QUANTUM NUMBERS CONTROL");
    ImGui::Separator();

    bool stateChanged = false;

    // Principal Quantum Number (n)
    ImGui::SliderInt("Principle (n)", &state.n, 1, 6);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        stateChanged = true;
        if (state.l >= state.n) state.l = state.n - 1;
        state.m = glm::clamp(state.m, -state.l, state.l);
    }
    ImGui::TextDisabled("Defines energy shell and size limit boundaries.");

    // Azimuthal Quantum Number (l)
    ImGui::SliderInt("Azimuthal (l)", &state.l, 0, state.n - 1);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        stateChanged = true;
        state.m = glm::clamp(state.m, -state.l, state.l);
    }
    ImGui::TextDisabled("Defines the subshell shape layout geometry (s, p, d, f).");

    // Magnetic Quantum Number (m)
    ImGui::SliderInt("Magnetic (m)", &state.m, -state.l, state.l);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        stateChanged = true;
    }
    ImGui::TextDisabled("Defines spatial orientation axes constraints.");

    ImGui::Spacing();
    ImGui::Checkbox("Enable Cross-Section Clip", &clipEnabled);
    if (stateChanged) regenerateCloud(); // Trigger cloud refresh on change

    ImGui::Spacing();
    ImGui::Spacing();

    // --- SECTION 2: EDUCATIONAL THEORY ---
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

    // --- SECTION: OVERLAY CREDITS (BOTTOM RIGHT) ---
    float margin = 15.0f;
    ImVec2 window_pos = ImVec2(static_cast<float>(m_width) - margin, static_cast<float>(m_height) - margin);
    ImVec2 window_pos_pivot = ImVec2(1.0f, 1.0f);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    ImGui::SetNextWindowBgAlpha(0.35f); //

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

    // Final UI render call
    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

// Main drawing routine: clears buffers and draws all 3D/2D components.
void Engine::drawScene(float currentFrameTime, float deltaTime) {
    // Explicitly clear background buffers completely to structural defaults
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f); // Clean deep cosmic slate background
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Update camera matrices
    camera.update(m_width, m_height, deltaTime);

    // Draw reference elements
    drawAxes();

    // Draw the orbital cloud
    glPushMatrix();
    drawCloud(currentFrameTime);
    glPopMatrix();

    // Optional: Draw classical tracker
    glPushMatrix();
    // drawActiveElectron();
    glPopMatrix();

    // Draw overlay UI
    renderUI();
}

// Initialized the GLFW window and context.
void Engine::initGlfwWindow() {
    if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW.");

    // Use legacy OpenGL 2.1 for maximum compatibility and simplicity
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window.");
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable VSync
    glfwSetWindowUserPointer(window, this); // Store 'this' for use in callbacks
}

// Initializes basic OpenGL state and GLAD loader.
void Engine::initOpenGL() {
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        throw std::runtime_error("Failed to initialize GLAD.");

    glEnable(GL_DEPTH_TEST); // Enable depth buffering
    glDepthFunc(GL_LEQUAL);

    // Enable point sprites behavior across native hardware drivers
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(0x8861); // GL_POINT_SPRITE compatibility mapping for older hardware

    // Compile color mapping shaders
    initShaders();
}

// Initializes the Dear ImGui context and platform/renderer backends.
void Engine::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL2_Init();
}

// Registers GLFW callbacks for window resize, keyboard, mouse, and scroll events.
void Engine::setupCallbacks() {
    // Handle window resizing
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow *win, int w, int h) {
        auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
        eng->m_width = w;
        eng->m_height = h;
        glViewport(0, 0, w, h);
    });

    // Handle keyboard input
    glfwSetKeyCallback(window, [](GLFWwindow *win, int key, int scancode, int action, int mods) {
        ImGui_ImplGlfw_KeyCallback(win, key, scancode, action, mods);
        auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));

        // Only process hotkeys if ImGui isn't capturing keyboard input
        if (!ImGui::GetIO().WantCaptureKeyboard && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            bool changed = false;

            // 'R' to reset simulation
            if (key == GLFW_KEY_R && action == GLFW_PRESS) {
                eng->resetSimulation();
                return;
            }

            // UP/DOWN arrows to change energy shell (n)
            if (key == GLFW_KEY_UP) {
                eng->state.n = std::min(eng->state.n + 1, 6);
                changed = true;
            }
            if (key == GLFW_KEY_DOWN) {
                eng->state.n = std::max(eng->state.n - 1, 1);
                // Ensure dependent quantum numbers stay in valid ranges
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

    // Handle mouse button clicks
    glfwSetMouseButtonCallback(window, [](GLFWwindow *win, int button, int action, int mods) {
        ImGui_ImplGlfw_MouseButtonCallback(win, button, action, mods);
        auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
        if (!ImGui::GetIO().WantCaptureMouse) {
            if (action == GLFW_PRESS) eng->m_mouseButtonDown = button;
            else if (action == GLFW_RELEASE) eng->m_mouseButtonDown = -1;
        } else eng->m_mouseButtonDown = -1;
    });

    // Handle mouse movement for camera orbit/pan
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

            // Pan with Right Mouse Button or Shift + Left Mouse Button
            if (shiftPressed || eng->m_mouseButtonDown == GLFW_MOUSE_BUTTON_RIGHT) eng->camera.pan(xoffset, yoffset);
                // Orbit with Left Mouse Button
            else if (eng->m_mouseButtonDown == GLFW_MOUSE_BUTTON_LEFT) {
                eng->camera.targetYaw += xoffset * 0.18f;
                eng->camera.targetPitch += yoffset * 0.18f;
                eng->camera.targetPitch = glm::clamp(eng->camera.targetPitch, -89.0f, 89.0f);
            }
        }
    });

    // Handle mouse scroll wheel for zooming
    glfwSetScrollCallback(window, [](GLFWwindow *win, double xoffset, double yoffset) {
        ImGui_ImplGlfw_ScrollCallback(win, xoffset, yoffset);
        if (!ImGui::GetIO().WantCaptureMouse) {
            auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
            eng->camera.targetDistance -= static_cast<float>(yoffset) * 22.0f;
            eng->camera.targetDistance = glm::clamp(eng->camera.targetDistance, 60.0f, 1400.0f);
        }
    });

    // Pass character input to ImGui
    glfwSetCharCallback(window, [](GLFWwindow *win, unsigned int codepoint) {
        ImGui_ImplGlfw_CharCallback(win, codepoint);
    });
}

// Draws the XYZ coordinate axes at the world origin.
void Engine::drawAxes() {
    glPushMatrix();
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(1.0f, 0.0f, 0.0f); // X-Axis (Red)
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(120.0f, 0.0f, 0.0f);

    glColor3f(0.0f, 1.0f, 0.0f); // Y-Axis (Green)
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 120.0f, 0.0f);

    glColor3f(0.0f, 0.0f, 1.0f); // Z-Axis (Blue)
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 120.0f);
    glEnd();
    glPopMatrix();
}

// Renders the probability density cloud using point sprites and additive-like blending.
void Engine::drawCloud(float timeVal) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE); // Allow particles to overlap cleanly without blocking borders

    // Turn on the programmatic color-mapping raytracer styling shader
    glUseProgram(m_shaderProgram);

    // Scale point size based on camera distance for a consistent visual density
    float pointScale = glm::clamp(380.0f / camera.distance, 0.7f, 5.0f);
    glPointSize(14.0f * pointScale); // Standard cleaner sizing threshold

    // Find local max density for relative color scaling
    float maxDensity = 0.000001f;
    for (const auto &p: cloudPoints) if (p.brightness > maxDensity) maxDensity = p.brightness;

    glBegin(GL_POINTS);
    for (const auto &p: cloudPoints) {
        glm::vec3 pos = p.pos;

        // Add dynamic rotation for non-zero Magnetic numbers (m)
        if (state.m != 0) {
            float globalSpeed = 5.0f / static_cast<float>(state.n);
            float norm = p.brightness / maxDensity;
            float probabilitySpeedFactor = 0.15f + (norm * 3.5f);
            float angle = timeVal * globalSpeed * probabilitySpeedFactor * static_cast<float>(state.m);
            float origX = pos.x;
            float origZ = pos.z;
            pos.x = origX * std::cos(angle) - origZ * std::sin(angle);
            pos.z = origX * std::sin(angle) + origZ * std::cos(angle);
        }

        // Apply cross-section clipping if enabled
        if (clipEnabled && pos.x > 0.0f && pos.y > 0.0f && pos.z > 0.0f) continue;

        // Map brightness density to heatmap color
        float norm = p.brightness / maxDensity;
        glm::vec4 fireColor = heatmapFire(norm);

        glColor4f(fireColor.r, fireColor.g, fireColor.b, fireColor.a);
        glVertex3f(pos.x, pos.y, pos.z);
    }
    glEnd();

    glUseProgram(0); // Unbind point shader safely
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// (Internal) Draws a bright sphere representing a classical electron point-particle.
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