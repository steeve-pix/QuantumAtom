#pragma once

#include "core/AppConfig.h"
#include "utils/Camera.h"
#include "utils/QuantumTypes.h"

#include <atomic>
#include <deque>
#include "utils/OpenGLLoader.h"
#include <GLFW/glfw3.h>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

struct ImFont;

// Cache keys deliberately ignore purely visual settings. Color maps, threshold,
// clipping, and render mode are shader uniforms, so the same cloud can be reused.
struct CloudCacheKey {
    QuantumState state;
    int pointCount = QUANTUMATOM_DEFAULT_POINT_COUNT;

    bool operator==(const CloudCacheKey &other) const {
        return state == other.state && pointCount == other.pointCount;
    }
};

// Cached clouds keep CPU point data plus the density normalization needed when
// the VBO is rebuilt after a cache hit.
struct CloudCacheEntry {
    CloudCacheKey key;
    std::vector<CloudPoint> points;
    float maxDensity = 1e-6f;
};

// UI-facing status for the background cloud generator.
enum class CloudBuildStage : int {
    Idle = 0,
    CacheHit,
    ScanningDensity,
    PreviewReady,
    Refining,
    Complete
};

/**
 * @class Engine
 * @brief Orchestrates windowing, simulation, rendering, UI, caching, and exports.
 */
class Engine {
private:
    // Window/config state. m_launchConfig and m_launchState exist so R can
    // restore exactly the startup state.
    int m_width;
    int m_height;
    std::string m_title;
    AppConfig m_config;
    AppConfig m_launchConfig;
    QuantumState m_launchState;

    // OpenGL program and object handles. These are created on the render thread
    // and destroyed in the Engine destructor.
    GLuint m_cloudProgram = 0;
    GLuint m_axesProgram = 0;

    GLuint m_cloudVao = 0;
    GLuint m_posVbo = 0;
    GLuint m_normVbo = 0;
    GLuint m_omegaVbo = 0;

    GLuint m_axesVao = 0;
    GLuint m_axesVbo = 0;
    GLuint m_activeVao = 0;
    GLuint m_activeVbo = 0;

    GLint m_uCloudViewProjection = -1;
    GLint m_uCloudTime = -1;
    GLint m_uCloudM = -1;
    GLint m_uCloudColorIntensity = -1;
    GLint m_uCloudClipEnabled = -1;
    GLint m_uCloudClipPlane = -1;
    GLint m_uCloudClipMode = -1;
    GLint m_uCloudDensityThreshold = -1;
    GLint m_uCloudRenderMode = -1;
    GLint m_uCloudColorMap = -1;
    GLint m_uCloudPointSize = -1;
    GLint m_uCloudAnimationSpeed = -1;
    GLint m_uCloudIsoLevel = -1;
    GLint m_uCloudIsoWidth = -1;
    GLint m_uCloudTint = -1;
    GLint m_uAxesViewProjection = -1;
    GLint m_uAxesPointSize = -1;

    // CPU cloud data is uploaded lazily when m_vboDirty is true.
    bool m_vboDirty = true;
    float m_cachedMaxDensity = 1e-6f;

    // Background generation handoff. The worker fills m_pendingCloud, then the
    // render thread swaps it into cloudPoints at the start of drawScene().
    std::thread m_buildThread;
    std::mutex m_swapMutex;
    std::vector<CloudPoint> m_pendingCloud;
    CloudCacheKey m_pendingKey;
    float m_pendingMaxDensity = 1e-6f;
    bool m_pendingIsFinal = false;
    std::atomic<bool> m_cloudReady{false};
    std::atomic<bool> m_buildCancelled{false};
    std::atomic<int> m_buildProgress{0};
    std::atomic<int> m_buildStage{static_cast<int>(CloudBuildStage::Idle)};

    std::deque<CloudCacheEntry> m_cloudCache;
    size_t m_cacheLimit = 6;

    // Input/UI telemetry.
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

    bool m_screenshotRequested = false;
    std::string m_lastScreenshotPath;
    ImFont *m_infoMathFont = nullptr;
    bool m_showAxes = true;
    bool m_showElectronTracker = false;

    // Initialization and utility helpers are private so the frame loop remains
    // small and main.cpp does not need to know OpenGL details.
    void initGlfwWindow();
    void initOpenGL();
    void initImGui();
    void setupCallbacks();
    void initStaticGeometry();

    void applyTheme(UiTheme theme);
    void applyRuntimeConfig(const AppConfig &config);
    void resetCameraToLaunchPose();
    void cacheCloud(const CloudCacheEntry &entry);
    std::optional<CloudCacheEntry> findCachedCloud(const CloudCacheKey &key) const;
    int previewPointCount(const QuantumState &state, int targetPointCount) const;
    void requestScreenshot();
    bool saveScreenshotPng();

public:
    // Runtime state that is edited by the UI and read by rendering/simulation.
    GLFWwindow *window = nullptr;
    Camera camera;
    QuantumState state;
    std::vector<CloudPoint> cloudPoints;

    int pointBudget = QUANTUMATOM_DEFAULT_POINT_COUNT;
    bool clipEnabled = false;
    float clipPlane = 0.0f;
    ClipMode clipMode = ClipMode::PositiveXYZ;
    float colorIntensity = 1.0f;
    float densityThreshold = 0.0f;
    float pointSize = 7.0f;
    float animationSpeed = 1.0f;
    float isoLevel = 0.36f;
    float isoWidth = 0.055f;
    glm::vec3 pointTint = glm::vec3(1.0f);
    glm::vec3 backgroundColor = glm::vec3(0.035f, 0.04f, 0.055f);
    RenderMode renderMode = RenderMode::DensityPoints;
    ColorMap colorMap = ColorMap::Inferno;
    UiTheme theme = UiTheme::Dark;
    UiLanguage uiLanguage = UiLanguage::English;
    float electronAngle = 0.0f;
    bool m_isInitialized = false;

    Engine(int width, int height, const std::string &title, AppConfig config = {});
    ~Engine();

    void regenerateCloud();
    void resetSimulation();
    void regenerateSinglePoint(CloudPoint &p);
    void updatePhysics(float deltaTime);

    void initShaders();
    void renderUI();
    void drawScene(float currentFrameTime, float deltaTime);
    void drawAxes(const glm::mat4 &viewProjection);
    void drawCloud(float timeVal, const glm::mat4 &viewProjection);
    void drawActiveElectron(const glm::mat4 &viewProjection);
};
