#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <random>
#include "utils/Camera.h"
#include "utils/QuantumTypes.h"

/**
 * @class Engine
 * @brief Orchestrates the application lifecycle, including rendering, simulation, and user interaction.
 */
class Engine {
private:
    int m_width;  /**< Window width */
    int m_height; /**< Window height */
    std::string m_title; /**< Window title */

    GLuint m_shaderProgram = 0; /**< Handle for the compiled GLSL shader program */

    /** @name GPU Buffers */
    ///@{
    GLuint m_posVbo = 0;    /**< VBO for point positions */
    GLuint m_normVbo = 0;   /**< VBO for normalized probability values */
    bool m_vboDirty = true; /**< Flag indicating if GPU buffers need updating */
    ///@}

    /** @name Shader Uniform/Attribute Locations */
    ///@{
    GLint m_attrPos = -1;
    GLint m_attrNorm = -1;
    GLint m_uTime = -1;
    GLint m_uMFloat = -1;
    GLint m_uClipEnabled = -1;
    ///@}

    /** @name Multithreading Management */
    ///@{
    std::thread m_buildThread;                 /**< Thread for background cloud generation */
    std::mutex m_swapMutex;                    /**< Protects access to pending cloud data */
    std::vector<CloudPoint> m_pendingCloud;    /**< Buffer for newly generated cloud points */
    std::atomic<bool> m_cloudReady{false};     /**< Flag indicating if a pending cloud is ready to swap */
    std::atomic<bool> m_buildCancelled{false}; /**< Signal to abort the current build thread */
    std::atomic<int> m_buildProgress{0};       /**< Percentage of generation completion */
    ///@}

    float m_cachedMaxDensity = 1e-6f; /**< Maximum probability density for sampling */

    /** @name User Input State */
    ///@{
    float m_lastMouseX = 600.0f;
    float m_lastMouseY = 500.0f;
    bool m_firstMouse = true;
    int m_mouseButtonDown = -1;
    ///@}

    /** @name Performance Metrics */
    ///@{
    float m_fps = 0.0f;
    float m_frameTimeMs = 0.0f;
    double m_lastFpsUpdateTime = 0.0;
    int m_frameCount = 0;
    ///@}

    /** @name Random Number Generation */
    ///@{
    std::mt19937 m_gen;
    std::uniform_real_distribution<float> m_dis;
    ///@}

    /** @name Initialization Helpers */
    ///@{
    static glm::vec4 heatmapInferno(float value);
    void initGlfwWindow();
    void initOpenGL();
    void initImGui();
    void setupCallbacks();
    ///@}

public:
    GLFWwindow *window = nullptr; /**< GLFW window handle */
    Camera camera;                /**< Main viewing camera */
    QuantumState state;           /**< Current quantum configuration */
    std::vector<CloudPoint> cloudPoints; /**< Current active set of points for rendering */

    const int maxPoints = 2.5e5; /**< Total number of points in the probability cloud */
    bool clipEnabled = false;    /**< Toggle for cross-section clipping */
    float clipPlaneZ = 30.0f;    /**< Distance of the clipping plane */
    float electronAngle = 0.0f;  /**< Current orbital rotation angle for the shell visualization */
    bool m_isInitialized = false; /**< Initialization status flag */

    Engine(int width, int height, const std::string &title);
    ~Engine();

    /** @brief Initiates background generation of the probability cloud. */
    void regenerateCloud();

    /** @brief Resets simulation state and camera. */
    void resetSimulation();

    /** @brief Generates a single point based on current probability density. */
    void regenerateSinglePoint(CloudPoint &p);

    /** @brief Updates the logical state for each frame. */
    void updatePhysics(float deltaTime);

    /** @name Rendering Functions */
    ///@{
    GLuint compileShader(GLenum type, const std::string &source);
    void initShaders();
    void renderUI();
    void drawScene(float currentFrameTime, float deltaTime);
    void drawAxes();
    void drawCloud(float timeVal);
    void drawActiveElectron();
    ///@}
};

