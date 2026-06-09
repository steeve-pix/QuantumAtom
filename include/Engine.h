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
 * @brief Main engine class that handles rendering, UI, and simulation lifecycle.
 * 
 * The Engine class acts as the central hub of the application. It initializes
 * the OpenGL context, manages the GLFW window, handles user input through ImGui
 * and callbacks, and coordinates the quantum orbital simulation.
 */
class Engine {
private:
    int m_width;  ///< Window width in pixels
    int m_height; ///< Window height in pixels
    std::string m_title; ///< Window title

    GLuint m_shaderProgram = 0;

    // Mouse tracking state for camera control
    float m_lastMouseX = 600.0f;
    float m_lastMouseY = 500.0f;
    bool m_firstMouse = true;
    int m_mouseButtonDown = -1;

    float m_fps = 0.0f;
    float m_frameTimeMs = 0.0f;
    double m_lastFpsUpdateTime = 0.0;
    int m_frameCount = 0;

    // OpenGL Legacy Performance Caching Context Display Objects
    int m_pointsGeneratedSoFar = 0;
    bool m_needsRebuild = false;

    GLuint m_cloudDisplayList = 0;
    bool m_displayListCompiled = false;

    // Random number generation for cloud point sampling
    std::mt19937 m_gen;
    std::uniform_real_distribution<float> m_dis;

    /**
     * @brief Maps a normalized probability value to a dynamic heatmap color.
     * @param value Normalized probability (0.0 to 1.0)
     * @return RGBA color vector
     */
    glm::vec4 heatmapFire(float value);

    // Initialization helpers
    void initGlfwWindow();
    void initOpenGL();
    void initImGui();
    void setupCallbacks();

public:
    GLFWwindow *window = nullptr; ///< Pointer to the GLFW window context
    Camera camera; ///< Camera controller for the 3D scene
    QuantumState state; ///< Current quantum numbers (n, l, m)
    std::vector<CloudPoint> cloudPoints; ///< Collection of points forming the orbital cloud

    const int maxPoints = 1.5e5; ///< Maximum number of points in the visualization
    bool clipEnabled = false; ///< Toggle for cross-section clipping view
    float clipPlaneZ = 30.0f; ///< (Internal) Z-plane for clipping
    float electronAngle = 0.0f; ///< Rotation angle for the classical electron tracker
    bool m_isInitialized = false;

    /**
     * @brief Constructor for the Engine.
     * @param width Initial window width
     * @param height Initial window height
     * @param title Window title string
     */
    Engine(int width, int height, const std::string &title);

    /**
     * @brief Destructor that ensures proper cleanup of GLFW resources.
     */
    ~Engine();

    /**
     * @brief Regenerates the point cloud using Monte Carlo sampling.
     * 
     * Uses the current QuantumState to sample valid positions in 3D space
     * based on the wave function's probability density.
     */
    void regenerateCloud();

    /**
     * @brief Resets the simulation to its default state (Hydrogen 1s ground state).
     */
    void resetSimulation();

    /**
     * @brief (Optional) Regenerates a single point in the cloud for dynamic effects.
     */
    void regenerateSinglePoint(CloudPoint &p);

    /**
     * @brief Updates physics-related variables like orbital rotation.
     * @param deltaTime Time elapsed since last frame
     */
    void updatePhysics(float deltaTime);

    /**
         * @brief Compiles an individual OpenGL shader stage from source code.
         * @param type The OpenGL shader type (e.g., GL_VERTEX_SHADER, GL_FRAGMENT_SHADER)
         * @param source The raw GLSL source code string
         * @return The compiled shader object ID, or 0 if compilation fails
         */
    GLuint compileShader(GLenum type, const std::string &source);

    /**
     * @brief Initializes, compiles, and links the main OpenGL shader program.
     */
    void initShaders();

    /**
     * @brief Renders the ImGui user interface and credits overlay.
     */
    void renderUI();

    /**
     * @brief Main rendering call that coordinates all scene drawing.
     */
    void drawScene(float currentFrameTime, float deltaTime);

    /**
     * @brief Draws the RGB coordinate axes at the center of the scene.
     */
    void drawAxes();

    /**
     * @brief Renders the probability density cloud using GL_POINTS.
     * @param timeVal Current simulation time for dynamic effects
     */
    void drawCloud(float timeVal);

    /**
     * @brief Draws a localized sphere representing a classical electron path.
     */
    void drawActiveElectron();
};

#endif // ENGINE_H
