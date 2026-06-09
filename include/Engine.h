#ifndef ENGINE_H
#define ENGINE_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <random>
#include "Camera.h"
#include "QuantumTypes.h"

/**
 * @brief Main engine class that handles rendering, UI, and simulation lifecycle
 */
class Engine {
private:
    int m_width;
    int m_height;
    std::string m_title;

    float m_lastMouseX = 600.0f;
    float m_lastMouseY = 500.0f;
    bool m_firstMouse = true;
    int m_mouseButtonDown = -1;

    std::mt19937 m_gen;
    std::uniform_real_distribution<float> m_dis;

    /**
     * @brief Maps a value to a fire-like heatmap color
     */
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

    const int maxPoints = 72000;
    bool clipEnabled = false;
    float clipPlaneZ = 30.0f;
    float electronAngle = 0.0f;

    Engine(int width, int height, const std::string &title);

    ~Engine();

    /**
     * @brief Regenerates the point cloud based on current quantum state
     */
    void regenerateCloud();

    void resetSimulation();

    void regenerateSinglePoint(CloudPoint &p);

    void updatePhysics(float deltaTime);

    void renderUI();

    void drawScene(float currentFrameTime, float deltaTime);

    void drawAxes();

    void drawCloud(float timeVal);

    void drawActiveElectron();
};

#endif // ENGINE_H
