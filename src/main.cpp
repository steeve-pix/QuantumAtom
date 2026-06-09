#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <random>

#include "../external/glm/glm.hpp"
#include "../external/glm/gtc/matrix_transform.hpp"
#include "../external/glm/gtc/type_ptr.hpp"

#include "../external/imgui/imgui.h"
#include "../external/imgui/backends/imgui_impl_glfw.h"
#include "../external/imgui/backends/imgui_impl_opengl2.h"

#ifndef PI
# define PI 3.14159265358979323846f
#endif

using namespace std;
using namespace glm;

// DATA STRUCTURES & LOGGING CONFIGS
struct CloudPoint {
    glm::vec3 pos;
    glm::vec3 vel;
    float brightness;
};

struct QuantumState {
    int n = 1;
    int l = 0;
    int m = 0;
};

// CAMERA COMPONENT MODULE
class Camera {
public:
    float yaw = -40.0f;
    float pitch = 25.0f;
    float distance = 380.0f;
    vec3 targetPos = vec3(0.0f);

    float targetYaw = -40.0f;
    float targetPitch = 25.0f;
    float targetDistance = 380.0f;
    vec3 destinationTargetPos = vec3(0.0f);

    float smoothness = 5.0f;


    void update(int width, int height, float deltaTime) {
        float blend = 1.0f - std::exp(-smoothness * deltaTime);
        yaw += (targetYaw - yaw) * blend;
        pitch += (targetPitch - pitch) * blend;
        distance += (targetDistance - distance) * blend;
        targetPos += (destinationTargetPos - targetPos) * blend;

        // Compute spatial location around our panning focal target
        glm::vec3 offset(
            distance * std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)),
            distance * std::sin(glm::radians(pitch)),
            distance * std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch))
        );
        glm::vec3 camPos = targetPos + offset;

        glm::mat4 view = glm::lookAt(camPos, targetPos, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height),
                                          0.1f, 6000.0f);

        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(glm::value_ptr(proj));
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(glm::value_ptr(view));
    }

    // Safely compute perspective viewing vectors to offset target paths
    void pan(float deltaX, float deltaY) {
        // Reconstruct local view coordinates relative to current rotation state
        float radYaw = glm::radians(yaw);
        float radPitch = glm::radians(pitch);

        glm::vec3 forward(
            -std::cos(radYaw) * std::cos(radPitch),
            -std::sin(radPitch),
            -std::sin(radYaw) * std::cos(radPitch)
        );
        forward = glm::normalize(forward);

        glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
        glm::vec3 up = glm::cross(right, forward);

        // Scale panning speed dynamically with distance so it feels identical near or far
        float factor = distance * 0.0012f;
        destinationTargetPos += right * (-deltaX * factor) + up * (-deltaY * factor);
    }
};

// QUANTUM SIMULATION MATHEMATICS ENGINE
class QuantumSimulation {
private:
    static float associatedLegendre(int l_val, int m_val, float x) {
        int absM = std::abs(m_val);
        if (absM > l_val) return 0.0f;

        float pmm = 1.0f;
        if (absM > 0) {
            float somx2 = std::sqrt((1.0f - x) * (1.0f + x));
            float fact = 1.0f;
            for (int i = 1; i <= absM; i++) {
                pmm *= -fact * somx2;
                fact += 2.0f;
            }
        }
        if (l_val == absM) return pmm;

        float pmmp1 = x * (2.0f * absM + 1.0f) * pmm;
        if (l_val == absM + 1) return pmmp1;

        float pll = 0.0f;
        for (int ll = absM + 2; ll <= l_val; ll++) {
            pll = (x * (2.0f * ll - 1.0f) * pmmp1 - (ll + absM - 1.0f) * pmm) / (ll - absM);
            pmm = pmmp1;
            pmmp1 = pll;
        }
        return pmmp1;
    }

    static float associatedLaguerre(int k, int alpha, float x) {
        if (k == 0) return 1.0f;
        if (k == 1) return 1.0f + alpha - x;

        float L0 = 1.0f;
        float L1 = 1.0f + alpha - x;
        float L2 = 0.0f;

        for (int j = 2; j <= k; j++) {
            L2 = ((2 * j - 1 + alpha - x) * L1 - (j - 1 + alpha) * L0) / j;
            L0 = L1;
            L1 = L2;
        }
        return L2;
    }

    static float sphericalHarmonic(int l_val, int m_val, float theta, float phi) {
        int absM = std::abs(m_val);
        float Plm = associatedLegendre(l_val, absM, std::cos(theta));

        auto factorial = [](int num) {
            float res = 1.0f;
            for (int i = 2; i <= num; ++i) res *= i;
            return res;
        };

        // Standardized real spherical harmonic normalization
        float num = (2 * l_val + 1) * factorial(l_val - absM);
        float den = 4.0f * PI * factorial(l_val + absM);
        float norm = std::sqrt(num / den);

        if (m_val > 0) {
            return std::sqrt(2.0f) * norm * Plm * std::cos(m_val * phi);
        } else if (m_val < 0) {
            return std::sqrt(2.0f) * norm * Plm * std::sin(absM * phi);
        }

        return norm * Plm; // m == 0
    }

public:
    static float computeProbability(float r, float theta, float phi, const QuantumState &state) {
        float a0 = 4.0f; // Adjusted for viewport bounds
        float rho = (2.0f * r) / (state.n * a0);

        int k = state.n - state.l - 1;
        int alpha = 2 * state.l + 1;

        auto factorial = [](int num) {
            float res = 1.0f;
            for (int i = 2; i <= num; ++i) res *= i;
            return res;
        };

        // Proper normalization factor for Radial Wave Function R_nl
        float radNorm = std::sqrt(std::pow(2.0f / (state.n * a0), 3) * factorial(state.n - state.l - 1) /
                                  (2.0f * state.n * factorial(state.n + state.l)));

        float radial = radNorm * std::exp(-rho / 2.0f) * std::pow(rho, state.l) * associatedLaguerre(k, alpha, rho);
        float angular = sphericalHarmonic(state.l, state.m, theta, phi);

        float psi = radial * angular;
        return psi * psi; // Return accurate |Ψ|²
    }
};

// MAIN ENGINE CONTAINER CLASS
class Engine {
private:
    int m_width;
    int m_height;
    std::string m_title;

    // Interaction Management States
    float m_lastMouseX = 600.0f;
    float m_lastMouseY = 500.0f;
    bool m_firstMouse = true;
    int m_mouseButtonDown = -1;

    // Persistent Random Engine Tools moved out of functions to avoid thread stalling
    std::mt19937 m_gen;
    std::uniform_real_distribution<float> m_dis;

    vec4 heatmapFire(float value) {
        value = std::max(0.0f, std::min(1.0f, value));

        const int num_stops = 6;
        vec4 colors[num_stops] = {
            {0.0f, 0.0f, 0.0f, 1.0f},
            {0.3f, 0.0f, 0.6f, 1.0f},
            {0.8f, 0.0f, 0.0f, 1.0f},
            {1.0f, 0.5f, 0.0f, 1.0f},
            {1.0f, 1.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f, 1.0f}
        };

        float scaled_v = value * (num_stops - 1);
        int i = static_cast<int>(scaled_v);
        int next_i = std::min(i + 1, num_stops - 1);

        float local_t = scaled_v - static_cast<float>(i);

        vec4 result;
        result.r = colors[i].r + local_t * (colors[next_i].r - colors[i].r);
        result.g = colors[i].g + local_t * (colors[next_i].g - colors[i].g);
        result.b = colors[i].b + local_t * (colors[next_i].b - colors[i].b);

        result.a = std::min(1.0f, value * 1.5f);
        return result;
    };

public:
    GLFWwindow *window = nullptr;
    Camera camera;
    QuantumState state;
    std::vector<CloudPoint> cloudPoints;

    const int maxPoints = 72000;
    bool clipEnabled = false;
    float clipPlaneZ = 30.0f;
    float electronAngle = 0.0f;

    Engine(int width, int height, const std::string &title)
        : m_width(width), m_height(height), m_title(title), m_dis(0.0f, 1.0f) {
        std::random_device rd;
        m_gen = std::mt19937(rd());

        initGlfwWindow();
        initOpenGL();
        initImGui();
        setupCallbacks();
        setupCallbacks();
        regenerateCloud();
    }

    ~Engine() {
        if (window) {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
    }

    void regenerateCloud() {
        cloudPoints.clear();
        cloudPoints.reserve(maxPoints);

        const float maxR = 6.0f * state.n * state.n * 2.0f;

        // Find peak density to scale the rejection test safely
        float maxTestDensity = 0.0f;
        for (int i = 0; i < 600; ++i) {
            float testR = m_dis(m_gen) * maxR;
            float testTh = std::acos(2.0f * m_dis(m_gen) - 1.0f);
            float testPh = 2.0f * PI * m_dis(m_gen);
            maxTestDensity = std::max(maxTestDensity,
                                      QuantumSimulation::computeProbability(testR, testTh, testPh, state));
        }
        if (maxTestDensity <= 0.0000001f) maxTestDensity = 1.0f;

        // Fixed Budget: Loop exactly maxPoints * 8 times. No infinite while() loop!
        int targetBudget = maxPoints * 8;
        for (int i = 0; i < targetBudget && static_cast<int>(cloudPoints.size()) < maxPoints; ++i) {
            float r = m_dis(m_gen) * maxR;
            float theta = std::acos(2.0f * m_dis(m_gen) - 1.0f);
            float phi = 2.0f * PI * m_dis(m_gen);

            float density = QuantumSimulation::computeProbability(r, theta, phi, state);

            if (m_dis(m_gen) * maxTestDensity < density) {
                glm::vec3 pos(
                    r * std::sin(theta) * std::cos(phi),
                    r * std::cos(theta),
                    r * std::sin(theta) * std::sin(phi)
                );
                cloudPoints.push_back({pos, glm::vec3(0.0f), density});
            }
        }

        // If we didn't hit maxPoints because the orbital is very thin,
        // fill the rest with safe low-density coordinates so vector size remains constant.
        while (static_cast<int>(cloudPoints.size()) < maxPoints) {
            cloudPoints.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f});
        }
    }

    void regenerateSinglePoint(CloudPoint &p) {
        const float maxR = 12.0f * state.n * state.n;
        float r = m_dis(m_gen) * maxR * 0.98f;
        float theta = std::acos(2.0f * m_dis(m_gen) - 1.0f);
        float phi = 2.0f * PI * m_dis(m_gen);

        p.pos = glm::vec3(r * std::sin(theta) * std::cos(phi),
                          r * std::sin(theta) * std::sin(phi),
                          r * std::cos(theta));

        p.vel = glm::vec3(0.0f);
        p.brightness = QuantumSimulation::computeProbability(r, theta, phi, state);
    }

    void updatePhysics(float deltaTime) {
        float orbitSpeed = 4.5f / static_cast<float>(state.n * state.n);
        electronAngle += orbitSpeed * deltaTime;
        if (electronAngle > 2.0f * PI) electronAngle -= 2.0f * PI;
    }

    void renderUI() {
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Pin custom menu to top-left overlay display bounds
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(420, 680), ImGuiCond_Once);

        ImGui::Begin("Quantum Configuration & Information", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "QUANTUM NUMBERS CONTROL");
        ImGui::Separator();

        bool changed = false;

        // Interactive Sliders
        if (ImGui::SliderInt("Principle (n)", &state.n, 1, 6)) {
            changed = true;
            // Bound safety adjustments down the chain
            if (state.l >= state.n) state.l = state.n - 1;
            state.m = glm::clamp(state.m, -state.l, state.l);
        }
        ImGui::TextDisabled("Defines energy shell and size limit boundaries.");

        if (ImGui::SliderInt("Azimuthal (l)", &state.l, 0, state.n - 1)) {
            changed = true;
            state.m = glm::clamp(state.m, -state.l, state.l);
        }
        ImGui::TextDisabled("Defines the subshell shape layout geometry (s, p, d, f).");

        if (ImGui::SliderInt("Magnetic (m)", &state.m, -state.l, state.l)) {
            changed = true;
        }
        ImGui::TextDisabled("Defines spatial orientation axes constraints.");

        ImGui::Spacing();
        ImGui::Checkbox("Enable Cross-Section Clip", &clipEnabled);

        if (changed) {
            regenerateCloud();
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // EDUCATIONAL SECTION
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "WHAT ARE YOU LOOKING AT?");
        ImGui::Separator();

        ImGui::BeginChild("TheoryScroll", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        ImGui::TextWrapped(
            "This particle cloud is a 3D solution to the time-independent Schrödinger Equation for a Hydrogen-like atom.");
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "The Wavefunction (|Ψ|²):");
        ImGui::TextWrapped(
            "In quantum mechanics, electrons do not orbit the nucleus in fixed planetary tracks. Instead, they exist as a 'probability cloud'. The density of points in this view represents |Ψ|², meaning a higher accumulation of glowing dots marks where an electron is statistically most likely to manifest upon physical measurement.");
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "The Quantum Numbers:");

        ImGui::BulletText(
            "n (Principle): Quantizes energy levels. As n increases, the atom's structural shells grow further outwards from the nucleus core matrix.");

        ImGui::BulletText(
            "l (Azimuthal/Orbital): Dictates angular momentum shapes. l=0 produces spherical 's' bounds, l=1 outputs the split dumbbell 'p' configurations, while l=2 provides complex multi-lobed 'd' structures.");

        ImGui::BulletText(
            "m (Magnetic): Governs spatial orientations. When m is zero, the cloud structures are completely static along the vertical Y center vector axis. Changing m introduces phase spin trajectories.");

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Dynamic Particle:");
        ImGui::TextWrapped(
            "The bright traveling sphere represents a localized simulation snapshot tracker showing the dynamic orbital path tracking across calculated classical kinetic constraints.");

        ImGui::EndChild();
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    }

    void drawScene(float currentFrameTime, float deltaTime) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        camera.update(m_width, m_height, deltaTime);

        drawAxes();

        glPushMatrix(); // isolate cloud rendering
        drawCloud(currentFrameTime);
        glPopMatrix();

        glPushMatrix(); // isolate electron rendering
        drawActiveElectron();
        glPopMatrix();

        renderUI();
    }

private:
    void initGlfwWindow() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW Configuration.");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

        window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create native platform GLFW Viewport Window.");
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        glfwSetWindowUserPointer(window, this);
    }

    void initOpenGL() {
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            throw std::runtime_error("Failed to map pipeline addresses using GLAD.");
        }
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LINE_SMOOTH);
    }

    void initImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void) io;
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL2_Init();
    }

    void setupCallbacks() {
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow *win, int w, int h) {
            auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
            eng->m_width = w;
            eng->m_height = h;
            glViewport(0, 0, w, h);
        });
        glfwSetKeyCallback(window, [](GLFWwindow *win, int key, int scancode, int action, int mods) {
            // Manually route the event to ImGui backend first
            ImGui_ImplGlfw_KeyCallback(win, key, scancode, action, mods);

            auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
            if (!ImGui::GetIO().WantCaptureKeyboard && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
                bool changed = false;
                if (key == GLFW_KEY_UP) {
                    eng->state.n = std::min(eng->state.n + 1, 6);
                    changed = true;
                }
                if (key == GLFW_KEY_DOWN) {
                    eng->state.n = std::max(eng->state.n - 1, 1);
                    if (eng->state.l >= eng->state.n) {
                        eng->state.l = eng->state.n - 1;
                    }
                    eng->state.m = glm::clamp(eng->state.m, -eng->state.l, eng->state.l);
                    changed = true;
                }
                if (changed) eng->regenerateCloud();
            }
        });

        glfwSetMouseButtonCallback(window, [](GLFWwindow *win, int button, int action, int mods) {
            // Manually route mouse clicks to ImGui backend so it updates focus and capture flags instantly
            ImGui_ImplGlfw_MouseButtonCallback(win, button, action, mods);

            auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
            // FIXED: Only engage 3D camera mouse tracking state if ImGui explicitly didn't claim the click
            if (!ImGui::GetIO().WantCaptureMouse) {
                if (action == GLFW_PRESS) eng->m_mouseButtonDown = button;
                else if (action == GLFW_RELEASE) eng->m_mouseButtonDown = -1;
            } else {
                eng->m_mouseButtonDown = -1;
            }
        });

        glfwSetCursorPosCallback(window, [](GLFWwindow *win, double xpos, double ypos) {
            // Forward mouse movement to ImGui
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

            // FIXED: Extra guard here to prevent camera drifting while manipulating sliders
            if (eng->m_mouseButtonDown != -1 && !ImGui::GetIO().WantCaptureMouse) {
                if (eng->m_mouseButtonDown == GLFW_MOUSE_BUTTON_LEFT) {
                    eng->camera.targetYaw += xoffset * 0.18f;
                    eng->camera.targetPitch += yoffset * 0.18f;
                    eng->camera.targetPitch = glm::clamp(eng->camera.targetPitch, -89.0f, 89.0f);
                } else if (eng->m_mouseButtonDown == GLFW_MOUSE_BUTTON_RIGHT) {
                    eng->camera.pan(xoffset, yoffset);
                }
            }
        });

        glfwSetScrollCallback(window, [](GLFWwindow *win, double xoffset, double yoffset) {
            // Forward scroll events to ImGui first (crucial for scrollable text window)
            ImGui_ImplGlfw_ScrollCallback(win, xoffset, yoffset);

            if (!ImGui::GetIO().WantCaptureMouse) {
                auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
                eng->camera.targetDistance -= static_cast<float>(yoffset) * 22.0f;
                eng->camera.targetDistance = glm::clamp(eng->camera.targetDistance, 60.0f, 1400.0f);
            }
        });

        glfwSetCharCallback(window, [](GLFWwindow *win, unsigned int codepoint) {
            // Allows text box components inside ImGui to receive normal keyboard typing characters
            ImGui_ImplGlfw_CharCallback(win, codepoint);
        });
    }

    void drawAxes() {
        glPushMatrix();
        glTranslatef(camera.targetPos.x, camera.targetPos.y, camera.targetPos.z);
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

    void drawCloud(float timeVal) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        glEnable(GL_POINT_SMOOTH);
        glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);

        float pointScale = glm::clamp(380.0f / camera.distance, 0.7f, 5.0f);
        glPointSize(35.0f * pointScale);

        float maxDensity = 0.000001f;
        for (const auto &p: cloudPoints) {
            if (p.brightness > maxDensity) maxDensity = p.brightness;
        }

        glBegin(GL_POINTS);
        for (const auto &p: cloudPoints) {
            glm::vec3 pos = p.pos;

            if (state.m != 0) {
                // 1. Calculate base speed scaled down by the principle energy level (n)
                float globalSpeed = 0.8f / static_cast<float>(state.n * state.n);

                // 2. Give each point a slightly varied speed based on its position so the cloud doesn't look rigid
                float pointVariation = 1.0f + 0.15f * std::sin(p.pos.x * 10.5f + p.pos.y * 10.5f);
                // float pointVariation = 1.0f + 0.15f * std::sin(p.pos.x * 0.5f + p.pos.y * 0.5f);

                // 3. Compute a continuously growing angle (no sine wave clamping!)
                // The sign of state.m determines if it goes clockwise or counter-clockwise
                float angle = timeVal * globalSpeed * pointVariation * static_cast<float>(state.m);

                // 4. Cache original coordinates to prevent distortion
                float origX = pos.x;
                float origZ = pos.z;

                // 5. Apply the standard 2D rotation matrix around the Z-axis (XY Plane)
                pos.x = origX * std::cos(angle) - origZ * std::sin(angle);
                pos.z = origX * std::sin(angle) + origZ * std::cos(angle);
            }

            if (clipEnabled && pos.x > 0.0f && pos.y > 0.0f && pos.z > 0.0f) {
                continue;
            }

            // === ORIGINAL COLOR CODE ===
            float norm = p.brightness / maxDensity;
            float t = glm::clamp(std::pow(norm, 0.22f), 0.0f, 1.0f);

            if (t < 0.05f) continue;

            float r_col = 0.0f, g_col = 0.0f, b_col = 0.0f, alpha = 0.0f;

            if (t < 0.25f) {
                float local_t = t / 0.25f;
                r_col = 0.05f * local_t;
                b_col = 0.4f + (0.6f * local_t);
                alpha = 0.15f * local_t;
            } else if (t < 0.75f) {
                float local_t = (t - 0.25f) / 0.50f;
                r_col = 0.05f + (0.95f * local_t);
                b_col = 1.0f - (0.4f * local_t);
                alpha = 0.15f + (0.65f * local_t);
            } else {
                float local_t = (t - 0.75f) / 0.25f;
                r_col = 1.0f;
                g_col = 0.9f * local_t;
                b_col = 0.6f * (1.0f - local_t) + (1.0f * local_t);
                alpha = 0.80f + (0.20f * local_t);
            }

            glColor4f(r_col, g_col, b_col, alpha);
            glVertex3f(pos.x, pos.y, pos.z);
        }
        glEnd();

        glDisable(GL_POINT_SMOOTH);
        glDisable(GL_BLEND);
    }

    void drawActiveElectron() {
        float radius = 14.5f * (state.n * state.n);

        float x = radius * std::cos(electronAngle);
        float y = (state.l > 0) ? radius * std::sin(electronAngle) * 0.5f : 0.0f;
        float z = (state.l > 0)
                      ? radius * std::sin(electronAngle) * std::sqrt(1.0f - 0.5f * 0.5f)
                      : radius * std::sin(electronAngle);

        glEnable(GL_POINT_SMOOTH);
        glPointSize(16.0f);
        glBegin(GL_POINTS);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glVertex3f(x, y, z);
        glEnd();

        glLineWidth(1.5f);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 60; ++i) {
            float sampleAngle = electronAngle - (i * 0.03f);
            float sx = radius * std::cos(sampleAngle);
            float sy = (state.l > 0) ? radius * std::sin(sampleAngle) * 0.5f : 0.0f;
            float sz = (state.l > 0)
                           ? radius * std::sin(sampleAngle) * std::sqrt(1.0f - 0.5f * 0.5f)
                           : radius * std::sin(sampleAngle);

            float trailFade = 1.0f - (static_cast<float>(i) / 60.0f);
            glColor4f(0.0f, 1.0f, 0.8f, trailFade * 0.4f);
            glVertex3f(sx, sy, sz);
        }
        glEnd();
        glDisable(GL_POINT_SMOOTH);
    }
};

int main() {
    try {
        Engine app(1200, 1000, "Quantum Atom Visualizer");

        float lastFrameTime = 0.0f;
        float lastTimeBuffer = 0.0f;
        int frameCount = 0;
        int currentFPS = 0;

        while (!glfwWindowShouldClose(app.window)) {
            if (glfwGetKey(app.window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(app.window, GLFW_TRUE);
            }

            float currentFrameTime = static_cast<float>(glfwGetTime());
            float deltaTime = currentFrameTime - lastFrameTime;
            lastFrameTime = currentFrameTime;

            frameCount++;
            if (currentFrameTime - lastTimeBuffer >= 1.0f) {
                currentFPS = frameCount;
                frameCount = 0;
                lastTimeBuffer = currentFrameTime;

                std::string titleStr = "Quantum Atom Visualizer | FPS: " + std::to_string(currentFPS) +
                                       " | Quantum State (n, l, m) = (" +
                                       std::to_string(app.state.n) + ", " +
                                       std::to_string(app.state.l) + ", " +
                                       std::to_string(app.state.m) + ")";
                glfwSetWindowTitle(app.window, titleStr.c_str());
            }

            app.updatePhysics(deltaTime);
            app.drawScene(currentFrameTime, deltaTime);

            glfwSwapBuffers(app.window);
            glfwPollEvents();
        }
    } catch (const std::exception &e) {
        std::cerr << "Application Runtime Error Crash: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
