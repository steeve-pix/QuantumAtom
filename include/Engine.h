#ifndef ENGINE_H
#define ENGINE_H

#include <atomic>
#include <thread>
#include <mutex>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <random>
#include "Camera.h"
#include "QuantumTypes.h"

class Engine {
private:
    int m_width;
    int m_height;
    std::string m_title;

    GLuint m_shaderProgram = 0;

    // --- VBO handles (GPU-side storage, uploaded ONCE per cloud rebuild) ---
    GLuint m_posVbo = 0; // vec3 positions
    GLuint m_normVbo = 0; // float normalised brightness
    GLuint m_speedVbo = 0; // float per-point speed factor
    bool m_vboDirty = true; // set true after every cloud swap

    // Shader attribute + uniform locations (queried once after link)
    GLint m_attrPos = -1;
    GLint m_attrNorm = -1;
    GLint m_attrSpeed = -1;
    GLint m_uTime = -1;
    GLint m_uGlobalSpeed = -1;
    GLint m_uMFloat = -1;
    GLint m_uUseRotation = -1;
    GLint m_uPointScale = -1;
    GLint m_uClipEnabled = -1;

    // --- Background thread cloud generation ---
    std::thread m_buildThread;
    std::mutex m_swapMutex;
    std::vector<CloudPoint> m_pendingCloud;
    std::atomic<bool> m_cloudReady{false};
    std::atomic<bool> m_buildCancelled{false};
    std::atomic<int> m_buildProgress{0}; // 0-100 percent

    float m_cachedMaxDensity = 1e-6f;

    float m_lastMouseX = 600.0f;
    float m_lastMouseY = 500.0f;
    bool m_firstMouse = true;
    int m_mouseButtonDown = -1;

    float m_fps = 0.0f;
    float m_frameTimeMs = 0.0f;
    double m_lastFpsUpdateTime = 0.0;
    int m_frameCount = 0;

    std::mt19937 m_gen;
    std::uniform_real_distribution<float> m_dis;

    glm::vec4 heatmapFire(float value);

    void initGlfwWindow();

    void initOpenGL();

    void initImGui();

    void setupCallbacks();

public:
    GLFWwindow *window = nullptr;
    Camera camera;
    QuantumState state;
    std::vector<CloudPoint> cloudPoints;

    const int maxPoints = 2.5e5;
    bool clipEnabled = false;
    float clipPlaneZ = 30.0f;
    float electronAngle = 0.0f;
    bool m_isInitialized = false;

    Engine(int width, int height, const std::string &title);

    ~Engine();

    void regenerateCloud();

    void resetSimulation();

    void regenerateSinglePoint(CloudPoint &p);

    void updatePhysics(float deltaTime);

    GLuint compileShader(GLenum type, const std::string &source);

    void initShaders();

    void renderUI();

    void drawScene(float currentFrameTime, float deltaTime);

    void drawAxes();

    void drawCloud(float timeVal);

    void drawActiveElectron();
};

#endif
