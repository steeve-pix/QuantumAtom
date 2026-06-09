#ifndef ENGINE_H
#define ENGINE_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <random>
#include "Camera.h"
#include "QuantumTypes.h"

// The Engine is the "heart" of the program. 
// It brings together the 3D window, the user controls, and the math to show the atom.
class Engine {
private:
    int m_width; 
    int m_height; 
    std::string m_title; 

    GLuint m_shaderProgram = 0;

    float m_lastMouseX = 600.0f;
    float m_lastMouseY = 500.0f;
    bool m_firstMouse = true;
    int m_mouseButtonDown = -1;

    float m_fps = 0.0f;
    float m_frameTimeMs = 0.0f;
    double m_lastFpsUpdateTime = 0.0;
    int m_frameCount = 0;

    int m_pointsGeneratedSoFar = 0;
    bool m_needsRebuild = false;

    GLuint m_cloudDisplayList = 0;
    bool m_displayListCompiled = false;

    std::mt19937 m_gen;
    std::uniform_real_distribution<float> m_dis;

    // Changes a number into a color (hot/fire colors) to show density.
    glm::vec4 heatmapFire(float value);

    // Private setup steps to get everything ready.
    void initGlfwWindow();
    void initOpenGL();
    void initImGui();
    void setupCallbacks();

public:
    GLFWwindow *window = nullptr; 
    Camera camera; 
    QuantumState state; 
    std::vector<CloudPoint> cloudPoints; 

    const int maxPoints = 2.39e5; 
    bool clipEnabled = false; 
    float clipPlaneZ = 30.0f; 
    float electronAngle = 0.0f; 
    bool m_isInitialized = false;

    // Starts the engine with a window of a certain size and title.
    Engine(int width, int height, const std::string &title);

    // Cleans up when the program closes.
    ~Engine();

    // Re-calculates all the dots in the electron cloud.
    void regenerateCloud();

    // Puts everything back to the starting state (Hydrogen 1s).
    void resetSimulation();

    // Updates just one dot (used for some effects).
    void regenerateSinglePoint(CloudPoint &p);

    // Handles any physics or movement that happens over time.
    void updatePhysics(float deltaTime);

    // Prepares the graphics card to draw our dots.
    GLuint compileShader(GLenum type, const std::string &source);
    void initShaders();

    // Draws the menus and buttons on the screen.
    void renderUI();

    // The main function that draws everything you see.
    void drawScene(float currentFrameTime, float deltaTime);

    // Draws the X, Y, and Z lines to show directions.
    void drawAxes();

    // Draws the actual cloud of dots.
    void drawCloud(float timeVal);

    // Draws a single "electron" moving around.
    void drawActiveElectron();
};

#endif
