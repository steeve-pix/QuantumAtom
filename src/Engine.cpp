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

Engine::~Engine() {
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

glm::vec4 Engine::heatmapFire(float value) {
    value = std::max(0.0f, std::min(1.0f, value));
    const int num_stops = 6;
    glm::vec4 colors[num_stops] = {
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
    glm::vec4 result;
    result.r = colors[i].r + local_t * (colors[next_i].r - colors[i].r);
    result.g = colors[i].g + local_t * (colors[next_i].g - colors[i].g);
    result.b = colors[i].b + local_t * (colors[next_i].b - colors[i].b);
    result.a = std::min(1.0f, value * 1.5f);
    return result;
}

void Engine::regenerateCloud() {
    cloudPoints.clear();
    cloudPoints.reserve(maxPoints);
    const float maxR = 6.0f * state.n * state.n * 2.0f;
    float maxTestDensity = 0.0f;
    for (int i = 0; i < 600; ++i) {
        float testR = m_dis(m_gen) * maxR;
        float testTh = std::acos(2.0f * m_dis(m_gen) - 1.0f);
        float testPh = 2.0f * PI * m_dis(m_gen);
        maxTestDensity = std::max(maxTestDensity, QuantumSimulation::computeProbability(testR, testTh, testPh, state));
    }
    if (maxTestDensity <= 0.0000001f) maxTestDensity = 1.0f;
    int targetBudget = maxPoints * 8;
    for (int i = 0; i < targetBudget && static_cast<int>(cloudPoints.size()) < maxPoints; ++i) {
        float r = m_dis(m_gen) * maxR;
        float theta = std::acos(2.0f * m_dis(m_gen) - 1.0f);
        float phi = 2.0f * PI * m_dis(m_gen);
        float density = QuantumSimulation::computeProbability(r, theta, phi, state);
        if (m_dis(m_gen) * maxTestDensity < density) {
            glm::vec3 pos(r * std::sin(theta) * std::cos(phi), r * std::cos(theta),
                          r * std::sin(theta) * std::sin(phi));
            cloudPoints.push_back({pos, glm::vec3(0.0f), density});
        }
    }
    while (static_cast<int>(cloudPoints.size()) < maxPoints) {
        cloudPoints.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f});
    }
}

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

void Engine::regenerateSinglePoint(CloudPoint &p) {
    const float maxR = 12.0f * state.n * state.n;
    float r = m_dis(m_gen) * maxR * 0.98f;
    float theta = std::acos(2.0f * m_dis(m_gen) - 1.0f);
    float phi = 2.0f * PI * m_dis(m_gen);
    p.pos = glm::vec3(r * std::sin(theta) * std::cos(phi), r * std::sin(theta) * std::sin(phi), r * std::cos(theta));
    p.vel = glm::vec3(0.0f);
    p.brightness = QuantumSimulation::computeProbability(r, theta, phi, state);
}

void Engine::updatePhysics(float deltaTime) {
    float orbitSpeed = 4.5f / static_cast<float>(state.n * state.n);
    electronAngle += orbitSpeed * deltaTime;
    if (electronAngle > 2.0f * PI) electronAngle -= 2.0f * PI;
}

void Engine::renderUI() {
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
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
    ImGui::TextWrapped(
        "This particle cloud is a 3D solution to the time-independent Schrodinger Equation for a Hydrogen-like atom.");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "The bright traveling sphere represents a localized simulation snapshot tracker showing the dynamic orbital path tracking across calculated classical kinetic constraints.");
    ImGui::EndChild();
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

void Engine::drawScene(float currentFrameTime, float deltaTime) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    camera.update(m_width, m_height, deltaTime);
    drawAxes();
    glPushMatrix();
    drawCloud(currentFrameTime);
    glPopMatrix();
    glPushMatrix();
    // drawActiveElectron();
    glPopMatrix();
    renderUI();
}

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

void Engine::initOpenGL() {
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) throw std::runtime_error(
        "Failed to initialize GLAD.");
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
}

void Engine::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL2_Init();
}

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
            bool shiftPressed = (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(
                                     win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
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

void Engine::drawAxes() {
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

void Engine::drawCloud(float timeVal) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    float pointScale = glm::clamp(380.0f / camera.distance, 0.7f, 5.0f);
    glPointSize(35.0f * pointScale);
    float maxDensity = 0.000001f;
    for (const auto &p: cloudPoints) if (p.brightness > maxDensity) maxDensity = p.brightness;
    glBegin(GL_POINTS);
    for (const auto &p: cloudPoints) {
        glm::vec3 pos = p.pos;
        if (state.m != 0) {
            float globalSpeed = 0.8f / static_cast<float>(state.n * state.n);
            float pointVariation = 1.0f + 0.15f * std::sin(p.pos.x * 10.5f + p.pos.y * 10.5f);
            float angle = timeVal * globalSpeed * pointVariation * static_cast<float>(state.m);
            float origX = pos.x;
            float origZ = pos.z;
            pos.x = origX * std::cos(angle) - origZ * std::sin(angle);
            pos.z = origX * std::sin(angle) + origZ * std::cos(angle);
        }
        if (clipEnabled && pos.x > 0.0f && pos.y > 0.0f && pos.z > 0.0f) continue;
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

void Engine::drawActiveElectron() {
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
    glDisable(GL_POINT_SMOOTH);
}
