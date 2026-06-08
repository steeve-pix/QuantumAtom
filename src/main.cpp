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
    int n = 4;
    int l = 3;
    int m = 1;
};

// CAMERA COMPONENT MODULE
class Camera {
public:
    float yaw = -40.0f;
    float pitch = 25.0f;
    float distance = 380.0f;
    float panX = 0.0f;
    float panY = 0.0f;

    void update(int width, int height) {
        glm::vec3 camPos(
            distance * std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)),
            distance * std::sin(glm::radians(pitch)),
            distance * std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch))
        );

        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height),
                                          0.1f, 6000.0f);

        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(glm::value_ptr(proj));
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(glm::value_ptr(view));
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
        float Plm = associatedLegendre(l_val, std::abs(m_val), std::cos(theta));

        auto factorial = [](int num) {
            float res = 1.0f;
            for (int i = 2; i <= num; ++i) res *= i;
            return res;
        };

        float num = (2 * l_val + 1) * factorial(l_val - std::abs(m_val));
        float den = 4.0f * PI * factorial(l_val + std::abs(m_val));
        float norm = std::sqrt(num / den);

        float phiPart = 1.0f;
        if (m_val > 0) phiPart = std::cos(m_val * phi);
        else if (m_val < 0) phiPart = std::sin(std::abs(m_val) * phi);

        return norm * Plm * phiPart;
    }

public:
    static float computeProbability(float r, float theta, float phi, const QuantumState &state) {
        if (r < 0.001f) return 0.0f;

        float rho = (2.0f * r) / (state.n * 9.0f);
        int k = state.n - state.l - 1;
        int alpha = 2 * state.l + 1;

        float radial = std::exp(-rho / 2.0f) * std::pow(rho, state.l) * associatedLaguerre(k, alpha, rho);
        float angular = sphericalHarmonic(state.l, state.m, theta, phi);

        float psiSquared = radial * radial * angular * angular;

        return psiSquared * (420.0f / (state.n * state.n * state.n * state.n));
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

        float local_t = scaled_v - 1;

        vec4 result;
        result.r = colors[i].r + local_t * (colors[next_i].r - colors[i].r);
        result.g = colors[i].g + local_t * (colors[next_i].g - colors[i].g);
        result.b = colors[i].b + local_t * (colors[next_i].b - colors[i].b);

        result.a = std::min(1.0f, value * 1.2f);
        return result;
    };

public:
    GLFWwindow *window = nullptr;
    Camera camera;
    QuantumState state;
    std::vector<CloudPoint> cloudPoints;

    const int maxPoints = 36000;
    bool clipEnabled = false;
    float clipPlaneZ = 30.0f;
    float electronAngle = 0.0f;

    Engine(int width, int height, const std::string &title)
        : m_width(width), m_height(height), m_title(title), m_dis(0.0f, 1.0f) {
        std::random_device rd;
        m_gen = std::mt19937(rd());

        initGlfwWindow();
        initOpenGL();
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

        const float maxR = 12.0f * state.n * state.n;
        int attempts = 0;
        const int maxAttemptsBudget = maxPoints * 100; // Watchdog guard to stop freezing completely

        while (static_cast<int>(cloudPoints.size()) < maxPoints && attempts < maxAttemptsBudget) {
            attempts++;
            float r = m_dis(m_gen) * maxR;
            float theta = std::acos(2.0f * m_dis(m_gen) - 1.0f);
            float phi = 2.0f * PI * m_dis(m_gen);

            float density = QuantumSimulation::computeProbability(r, theta, phi, state);
            float volumeElement = (r * r) * std::sin(theta);
            float weight = (density * density) * volumeElement;

            if (m_dis(m_gen) * (maxR * 0.15f) < weight) {
                glm::vec3 pos(
                    r * std::sin(theta) * std::cos(phi),
                    r * std::sin(theta) * std::sin(phi),
                    r * std::cos(theta)
                );

                glm::vec3 angularDir = glm::normalize(glm::cross(pos, glm::vec3(0.0f, 0.0f, 1.0f)));
                if (glm::length(angularDir) < 0.1f) {
                    angularDir = glm::normalize(glm::cross(pos, glm::vec3(0.0f, 1.0f, 0.0f)));
                }

                float speed = (2.0f + m_dis(m_gen) * 2.0f) / static_cast<float>(state.n);
                glm::vec3 vel = angularDir * speed;

                cloudPoints.push_back({pos, vel, density});
            }
        }
    }

    void updatePhysics(float deltaTime) {
        float orbitSpeed = 4.5f / static_cast<float>(state.n * state.n);
        electronAngle += orbitSpeed * deltaTime;
        if (electronAngle > 2.0f * PI) electronAngle -= 2.0f * PI;
    }

    void drawScene(float currentFrameTime) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        camera.update(m_width, m_height);

        drawAxes();
        drawCloud(currentFrameTime);
        drawActiveElectron();
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

    void setupCallbacks() {
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow *win, int w, int h) {
            auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
            eng->m_width = w;
            eng->m_height = h;
            glViewport(0, 0, w, h);
        });

        glfwSetKeyCallback(window, [](GLFWwindow *win, int key, int scancode, int action, int mods) {
            auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
            if (action == GLFW_PRESS || action == GLFW_REPEAT) {
                bool changed = false;
                if (key == GLFW_KEY_UP) {
                    eng->state.n = std::min(eng->state.n + 1, 6);
                    changed = true;
                }
                if (key == GLFW_KEY_DOWN) {
                    eng->state.n = std::max(eng->state.n - 1, 1);
                    if (eng->state.l >= eng->state.n) {
                        eng->state.l = eng->state.n - 1;
                        eng->state.m = glm::clamp(eng->state.m, -eng->state.l, eng->state.l);
                    }
                    changed = true;
                }
                if (key == GLFW_KEY_RIGHT) {
                    eng->state.l = std::min(eng->state.l + 1, eng->state.n - 1);
                    eng->state.m = glm::clamp(eng->state.m, -eng->state.l, eng->state.l);
                    changed = true;
                }
                if (key == GLFW_KEY_LEFT) {
                    eng->state.l = std::max(eng->state.l - 1, 0);
                    eng->state.m = glm::clamp(eng->state.m, -eng->state.l, eng->state.l);
                    changed = true;
                }
                if (key == GLFW_KEY_E) {
                    eng->state.m = std::min(eng->state.m + 1, eng->state.l);
                    changed = true;
                }
                if (key == GLFW_KEY_Q) {
                    eng->state.m = std::max(eng->state.m - 1, -eng->state.l);
                    changed = true;
                }
                if (key == GLFW_KEY_C) {
                    eng->clipEnabled = !eng->clipEnabled;
                }

                if (changed) {
                    eng->regenerateCloud();
                }
            }
        });

        glfwSetMouseButtonCallback(window, [](GLFWwindow *win, int button, int action, int mods) {
            auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
            if (action == GLFW_PRESS) eng->m_mouseButtonDown = button;
            else if (action == GLFW_RELEASE) eng->m_mouseButtonDown = -1;
        });

        glfwSetCursorPosCallback(window, [](GLFWwindow *win, double xpos, double ypos) {
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

            if (eng->m_mouseButtonDown == GLFW_MOUSE_BUTTON_LEFT) {
                eng->camera.yaw += xoffset * 0.18f;
                eng->camera.pitch += yoffset * 0.18f;
                eng->camera.pitch = glm::clamp(eng->camera.pitch, -89.0f, 89.0f);
            } else if (eng->m_mouseButtonDown == GLFW_MOUSE_BUTTON_RIGHT) {
                eng->camera.panX += xoffset * 1.8f;
                eng->camera.panY -= yoffset * 1.8f;
            }
        });

        glfwSetScrollCallback(window, [](GLFWwindow *win, double xoffset, double yoffset) {
            auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
            eng->camera.distance -= static_cast<float>(yoffset) * 22.0f;
            eng->camera.distance = glm::clamp(eng->camera.distance, 60.0f, 1400.0f);
        });
    }

    void drawAxes() {
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
    }

    void drawCloud(float timeVal) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_POINT_SMOOTH);
        glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);

        float pointScale = glm::clamp(380.0f / camera.distance, 0.7f, 5.0f);
        glPointSize(10.0f * pointScale);

        glBegin(GL_POINTS);

        float maxDensity = 0.0001f;
        for (const auto &p: cloudPoints) {
            if (p.brightness > maxDensity) maxDensity = p.brightness;
        }

        const float maxR = 12.0f * state.n * state.n;
        const float dt = 0.16f;

        for (auto &p: cloudPoints) {
            if (state.m == 0) {
                p.vel = glm::vec3(0.0f);
            }

            if (p.vel != glm::vec3(0.0f)) {
                p.pos += p.vel * dt;

                float r = glm::length(p.pos);
                if (r < 0.01f) r = 0.01f;
                float theta = std::acos(glm::clamp(p.pos.z / r, -1.0f, 1.0f));
                float phi = std::atan2(p.pos.y, p.pos.x);

                p.brightness = QuantumSimulation::computeProbability(r, theta, phi, state);

                float normTest = p.brightness / maxDensity;
                if (normTest < 0.015f || r > maxR) {
                    // Fix: Using the engine's persistent distributions prevents point-respawn stalls
                    float randR = m_dis(m_gen) * maxR;
                    float randTheta = std::acos(2.0f * m_dis(m_gen) - 1.0f);
                    float randPhi = 2.0f * PI * m_dis(m_gen);

                    p.pos = glm::vec3(randR * std::sin(randTheta) * std::cos(randPhi),
                                      randR * std::sin(randTheta) * std::sin(randPhi),
                                      randR * std::cos(randTheta));

                    glm::vec3 angularDir = glm::normalize(glm::cross(p.pos, glm::vec3(0.0f, 0.0f, 1.0f)));
                    if (glm::length(angularDir) < 0.1f) {
                        angularDir = glm::normalize(glm::cross(p.pos, glm::vec3(0.0f, 1.0f, 0.0f)));
                    }

                    float speed = (1.5f + m_dis(m_gen) * 2.0f) / static_cast<float>(state.n);
                    p.vel = angularDir * speed;
                    p.brightness = QuantumSimulation::computeProbability(randR, randTheta, randPhi, state);
                }
            }

            if (clipEnabled) {
                if (p.pos.x > 0.0f && p.pos.y > 0.0f && p.pos.z > 0.0f) {
                    continue;
                }
            }

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
            glVertex3f(p.pos.x, p.pos.y, p.pos.z);
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
            app.drawScene(currentFrameTime);

            glfwSwapBuffers(app.window);
            glfwPollEvents();
        }
    } catch (const std::exception &e) {
        std::cerr << "Application Runtime Error Crash: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}