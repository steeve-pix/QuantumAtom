#include "core/Engine.h"
#include "math/QuantumSimulation.h"
#include "utils/ShaderLoader.h"
#include "QuantumAtomVersion.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#ifndef PI
#define PI 3.141592653589793238462643383279502884f
#endif

namespace {
    // DensityGrid is a compact importance-sampling table. The background worker
    // scans rho(r, theta) once, builds a cumulative distribution, then samples
    // points from that table instead of using slow rejection sampling.
    struct DensityGrid {
        int radialSamples = 0;
        int thetaSamples = 0;
        float maxR = 0.0f;
        std::vector<double> cdf;
        double totalWeight = 0.0;
    };

    const char *renderModeName(RenderMode mode) {
        switch (mode) {
            case RenderMode::DensityPoints: return "Density points";
            case RenderMode::GlowBillboards: return "Glowing billboards";
            case RenderMode::IsoShell: return "Iso-density shell";
            case RenderMode::PhaseFlow: return "Phase flow";
            case RenderMode::HaloFog: return "Volumetric halo";
        }
        return "Unknown";
    }

    const char *colorMapName(ColorMap map) {
        switch (map) {
            case ColorMap::Inferno: return "Inferno";
            case ColorMap::Viridis: return "Viridis";
            case ColorMap::Plasma: return "Plasma";
            case ColorMap::Magma: return "Magma";
            case ColorMap::Cividis: return "Cividis";
        }
        return "Unknown";
    }

    const char *themeName(UiTheme theme) {
        switch (theme) {
            case UiTheme::Dark: return "Dark";
            case UiTheme::Classic: return "Classic";
            case UiTheme::Light: return "Light";
        }
        return "Unknown";
    }

    const char *stageName(CloudBuildStage stage) {
        switch (stage) {
            case CloudBuildStage::Idle: return "Idle";
            case CloudBuildStage::CacheHit: return "Cache hit";
            case CloudBuildStage::ScanningDensity: return "Scanning density";
            case CloudBuildStage::PreviewReady: return "Preview ready";
            case CloudBuildStage::Refining: return "Refining";
            case CloudBuildStage::Complete: return "Complete";
        }
        return "Unknown";
    }

    void helpMarker(const char *text) {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    bool renderModeCombo(const char *label, RenderMode &mode) {
        bool changed = false;
        if (ImGui::BeginCombo(label, renderModeName(mode))) {
            for (int i = 0; i <= static_cast<int>(RenderMode::HaloFog); ++i) {
                const auto candidate = static_cast<RenderMode>(i);
                const bool selected = candidate == mode;
                if (ImGui::Selectable(renderModeName(candidate), selected)) {
                    mode = candidate;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool colorMapCombo(const char *label, ColorMap &map) {
        bool changed = false;
        if (ImGui::BeginCombo(label, colorMapName(map))) {
            for (int i = 0; i <= static_cast<int>(ColorMap::Cividis); ++i) {
                const auto candidate = static_cast<ColorMap>(i);
                const bool selected = candidate == map;
                if (ImGui::Selectable(colorMapName(candidate), selected)) {
                    map = candidate;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool themeCombo(const char *label, UiTheme &theme) {
        bool changed = false;
        if (ImGui::BeginCombo(label, themeName(theme))) {
            for (int i = 0; i <= static_cast<int>(UiTheme::Light); ++i) {
                const auto candidate = static_cast<UiTheme>(i);
                const bool selected = candidate == theme;
                if (ImGui::Selectable(themeName(candidate), selected)) {
                    theme = candidate;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    const char *clipModeName(ClipMode mode) {
        switch (mode) {
            case ClipMode::XPlane: return "X plane";
            case ClipMode::PositiveXY: return "+X +Y quadrant";
            case ClipMode::PositiveXYZ: return "+X +Y +Z octant";
        }
        return "Unknown";
    }

    bool clipModeCombo(const char *label, ClipMode &mode) {
        bool changed = false;
        if (ImGui::BeginCombo(label, clipModeName(mode))) {
            for (int i = 0; i <= static_cast<int>(ClipMode::PositiveXYZ); ++i) {
                const auto candidate = static_cast<ClipMode>(i);
                const bool selected = candidate == mode;
                if (ImGui::Selectable(clipModeName(candidate), selected)) {
                    mode = candidate;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    float orbitalMaxRadius(const QuantumState &state) {
        return 9.5f * static_cast<float>(state.n * state.n);
    }

    int densityRadialSamples(const QuantumState &state, int pointCount) {
        // n=7 and n=8 orbitals are much larger and have more nodes. A capped
        // sample count keeps the UI responsive while preserving the broad shape.
        int base = 120 + state.n * 45;
        if (state.n >= 7) base = 560;
        if (pointCount > 250000) base += 80;
        if (pointCount > 450000) base += 80;
        return base;
    }

    int densityThetaSamples(const QuantumState &state, int pointCount) {
        // Angular sampling grows more slowly than radial sampling because phi is
        // sampled uniformly when individual points are generated.
        int base = 96 + state.n * 20;
        if (state.n >= 7) base = 288;
        if (pointCount > 250000) base += 48;
        return base;
    }

    float safeDensity(float r, float theta, const QuantumState &state) {
        // Bad floating-point values should not poison the CDF. Treat them as
        // zero density and let valid cells dominate the distribution.
        const float density = QuantumSimulation::computeProbability(r, theta, 0.0f, state);
        return std::isfinite(density) && density > 0.0f ? density : 0.0f;
    }

    std::tm localTime(std::time_t value) {
        std::tm result{};
#ifdef _WIN32
        localtime_s(&result, &value);
#else
        localtime_r(&value, &result);
#endif
        return result;
    }
}

Engine::Engine(int width, int height, const std::string &title, AppConfig config)
    : m_width(width > 0 ? width : config.windowWidth),
      m_height(height > 0 ? height : config.windowHeight),
      m_title(title),
      m_config(std::move(config)),
      m_launchConfig(m_config),
      m_launchState(state),
      m_dis(0.0f, 1.0f) {
    std::random_device rd;
    m_gen = std::mt19937(rd());

    // Keep an exact launch snapshot so pressing R can restore the same state the
    // executable had at startup, not merely a hard-coded approximation.
    applyRuntimeConfig(m_config);

    initGlfwWindow();
    initOpenGL();
    initImGui();
    setupCallbacks();
    regenerateCloud();
    m_isInitialized = true;
}

Engine::~Engine() {
    m_buildCancelled.store(true);
    if (m_buildThread.joinable()) {
        m_buildThread.join();
    }

    // Persist the current UI state on exit. Startup defaults come back through
    // AppConfig::loadDefaultLocations().
    m_config.windowWidth = m_width;
    m_config.windowHeight = m_height;
    m_config.pointCount = pointBudget;
    m_config.clipEnabled = clipEnabled;
    m_config.clipPlane = clipPlane;
    m_config.clipMode = clipMode;
    m_config.colorIntensity = colorIntensity;
    m_config.densityThreshold = densityThreshold;
    m_config.pointSize = pointSize;
    m_config.animationSpeed = animationSpeed;
    m_config.renderMode = renderMode;
    m_config.colorMap = colorMap;
    m_config.theme = theme;
    m_config.pointTint = pointTint;
    m_config.backgroundColor = backgroundColor;
    (void) m_config.save();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_cloudVao) glDeleteVertexArrays(1, &m_cloudVao);
    if (m_axesVao) glDeleteVertexArrays(1, &m_axesVao);
    if (m_activeVao) glDeleteVertexArrays(1, &m_activeVao);
    if (m_posVbo) glDeleteBuffers(1, &m_posVbo);
    if (m_normVbo) glDeleteBuffers(1, &m_normVbo);
    if (m_omegaVbo) glDeleteBuffers(1, &m_omegaVbo);
    if (m_axesVbo) glDeleteBuffers(1, &m_axesVbo);
    if (m_activeVbo) glDeleteBuffers(1, &m_activeVbo);
    if (m_cloudProgram) glDeleteProgram(m_cloudProgram);
    if (m_axesProgram) glDeleteProgram(m_axesProgram);
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

void Engine::applyTheme(UiTheme selectedTheme) {
    switch (selectedTheme) {
        case UiTheme::Dark:
            ImGui::StyleColorsDark();
            break;
        case UiTheme::Classic:
            ImGui::StyleColorsClassic();
            break;
        case UiTheme::Light:
            ImGui::StyleColorsLight();
            break;
    }

    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
}

void Engine::applyRuntimeConfig(const AppConfig &config) {
    // This is shared by normal startup and full reset, so config-backed runtime
    // settings stay in one place.
    pointBudget = std::clamp(config.pointCount, kMinimumPointCount, kMaximumPointCount);
    clipEnabled = config.clipEnabled;
    clipPlane = config.clipPlane;
    clipMode = config.clipMode;
    colorIntensity = config.colorIntensity;
    densityThreshold = config.densityThreshold;
    pointSize = config.pointSize;
    animationSpeed = config.animationSpeed;
    renderMode = config.renderMode;
    colorMap = config.colorMap;
    theme = config.theme;
    pointTint = config.pointTint;
    backgroundColor = config.backgroundColor;
}

void Engine::resetCameraToLaunchPose() {
    // Camera reset is separate from AppConfig because these values are view
    // ergonomics, not persisted user defaults.
    camera.yaw = -40.0f;
    camera.pitch = 25.0f;
    camera.distance = 380.0f;
    camera.targetPos = glm::vec3(0.0f);
    camera.targetYaw = camera.yaw;
    camera.targetPitch = camera.pitch;
    camera.targetDistance = camera.distance;
    camera.destinationTargetPos = camera.targetPos;
}

void Engine::cacheCloud(const CloudCacheEntry &entry) {
    // Most UI experiments revisit nearby settings. An MRU cache makes switching
    // between a few orbitals instant after the first build.
    const auto sameKey = [&entry](const CloudCacheEntry &candidate) {
        return candidate.key == entry.key;
    };

    std::erase_if(m_cloudCache, sameKey);
    m_cloudCache.push_front(entry);
    while (m_cloudCache.size() > m_cacheLimit) {
        m_cloudCache.pop_back();
    }
}

std::optional<CloudCacheEntry> Engine::findCachedCloud(const CloudCacheKey &key) const {
    for (const auto &entry: m_cloudCache) {
        if (entry.key == key) return entry;
    }
    return std::nullopt;
}

int Engine::previewPointCount(const QuantumState &targetState, int targetPointCount) const {
    // High-n full clouds can take noticeable time, so draw a smaller preview
    // first and refine asynchronously.
    if (targetPointCount <= 90000) return targetPointCount;
    if (targetState.n >= 7) return std::min(targetPointCount, std::max(65000, targetPointCount / 4));
    return std::min(targetPointCount, 120000);
}

void Engine::regenerateCloud() {
    // Sanitize UI/config values before they reach the math routines.
    state.n = std::clamp(state.n, 1, QUANTUMATOM_MAX_N);
    state.l = std::clamp(state.l, 0, state.n - 1);
    state.m = std::clamp(state.m, -state.l, state.l);
    pointBudget = std::clamp(pointBudget, kMinimumPointCount, kMaximumPointCount);

    // Only one cloud build should own the pending buffers at a time.
    m_buildCancelled.store(true);
    if (m_buildThread.joinable()) {
        m_buildThread.join();
    }

    const CloudCacheKey key{state, pointBudget};
    if (const auto cached = findCachedCloud(key)) {
        // Cache hits still mark the VBO dirty so the cached CPU cloud is
        // uploaded to the GPU on the next draw.
        cloudPoints = cached->points;
        m_cachedMaxDensity = cached->maxDensity;
        m_vboDirty = true;
        m_cloudReady.store(false);
        m_buildProgress.store(100);
        m_buildStage.store(static_cast<int>(CloudBuildStage::CacheHit));
        return;
    }

    m_buildCancelled.store(false);
    m_cloudReady.store(false);
    m_buildProgress.store(0);
    m_buildStage.store(static_cast<int>(CloudBuildStage::ScanningDensity));

    const QuantumState capturedState = state;
    const int targetPoints = pointBudget;
    const int previewPoints = previewPointCount(capturedState, targetPoints);

    m_buildThread = std::thread([this, capturedState, targetPoints, previewPoints, key]() {
        // Everything in this lambda runs off the render thread. It may prepare
        // CPU vectors, but it never touches OpenGL objects.
        std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);

        DensityGrid grid;
        grid.radialSamples = densityRadialSamples(capturedState, targetPoints);
        grid.thetaSamples = densityThetaSamples(capturedState, targetPoints);
        grid.maxR = orbitalMaxRadius(capturedState);

        const size_t cellCount = static_cast<size_t>(grid.radialSamples) *
                                 static_cast<size_t>(grid.thetaSamples);
        std::vector<double> weights(cellCount, 0.0);

        // Parallelize the expensive density scan across radial rows. The worker
        // count is capped so the UI thread and OS still have breathing room.
        std::atomic<int> nextRow{0};
        std::atomic<int> rowsDone{0};
        const unsigned hardwareWorkers = std::max(1u, std::thread::hardware_concurrency());
        const unsigned availableWorkers = hardwareWorkers > 2u ? hardwareWorkers - 1u : 1u;
        const unsigned workerCount = std::min<unsigned>(availableWorkers, 4u);
        std::vector<std::thread> workers;
        workers.reserve(workerCount);

        for (unsigned worker = 0; worker < workerCount; ++worker) {
            workers.emplace_back([&, worker]() {
                (void) worker;
                while (!m_buildCancelled.load()) {
                    const int rIndex = nextRow.fetch_add(1);
                    if (rIndex >= grid.radialSamples) break;

                    const float r = grid.maxR *
                                    (static_cast<float>(rIndex) + 0.5f) /
                                    static_cast<float>(grid.radialSamples);
                    const double radialWeight = static_cast<double>(r) * static_cast<double>(r);

                    for (int thetaIndex = 0; thetaIndex < grid.thetaSamples; ++thetaIndex) {
                        const float cosTheta = -1.0f + 2.0f *
                            (static_cast<float>(thetaIndex) + 0.5f) /
                            static_cast<float>(grid.thetaSamples);
                        const float theta = acosf(std::clamp(cosTheta, -1.0f, 1.0f));
                        const float density = safeDensity(r, theta, capturedState);

                        // In spherical coordinates dV = r^2 sin(theta) dr dtheta dphi.
                        // Uniform cos(theta) sampling already accounts for sin(theta), so
                        // the table only needs the r^2 radial volume factor.
                        const size_t idx = static_cast<size_t>(rIndex) *
                                           static_cast<size_t>(grid.thetaSamples) +
                                           static_cast<size_t>(thetaIndex);
                        weights[idx] = static_cast<double>(density) * radialWeight;
                    }

                    const int done = rowsDone.fetch_add(1) + 1;
                    if ((done & 7) == 0) {
                        m_buildProgress.store(std::clamp(done * 52 / grid.radialSamples, 0, 52));
                    }
                }
            });
        }

        for (auto &worker: workers) {
            if (worker.joinable()) worker.join();
        }
        if (m_buildCancelled.load()) return;

        // Convert cell weights into a cumulative distribution so every point can
        // be sampled with one random number plus lower_bound().
        grid.cdf.resize(cellCount);
        double total = 0.0;
        for (size_t i = 0; i < weights.size(); ++i) {
            total += weights[i];
            grid.cdf[i] = total;
        }

        if (total <= std::numeric_limits<double>::min()) {
            // Extremely invalid or degenerate settings should still produce a
            // visible cloud instead of dividing by zero or hanging generation.
            total = 0.0;
            for (size_t i = 0; i < grid.cdf.size(); ++i) {
                total += 1.0;
                grid.cdf[i] = total;
            }
        }
        grid.totalWeight = total;

        auto generatePoints = [&](int count, int progressBegin, int progressEnd) {
            // Inverse-transform sample (r, theta) from the CDF, then choose phi
            // uniformly. This produces a dense cloud without rejection loops.
            std::vector<CloudPoint> points;
            points.reserve(static_cast<size_t>(count));

            for (int i = 0; i < count && !m_buildCancelled.load(); ++i) {
                const double sample = static_cast<double>(dis(gen)) * grid.totalWeight;
                auto it = std::lower_bound(grid.cdf.begin(), grid.cdf.end(), sample);
                size_t idx = static_cast<size_t>(std::distance(grid.cdf.begin(), it));
                idx = std::min(idx, grid.cdf.size() - 1);

                const int rIndex = static_cast<int>(idx / static_cast<size_t>(grid.thetaSamples));
                const int thetaIndex = static_cast<int>(idx % static_cast<size_t>(grid.thetaSamples));

                const float r = grid.maxR *
                                (static_cast<float>(rIndex) + dis(gen)) /
                                static_cast<float>(grid.radialSamples);
                const float cosTheta = -1.0f + 2.0f *
                    (static_cast<float>(thetaIndex) + dis(gen)) /
                    static_cast<float>(grid.thetaSamples);
                const float theta = acosf(std::clamp(cosTheta, -1.0f, 1.0f));
                const float phi = 2.0f * PI * dis(gen);

                const float density = QuantumSimulation::computeProbability(r, theta, phi, capturedState);
                const float sinTheta = sinf(theta);
                // The scene uses Y as the polar axis, matching theta measured
                // from the vertical axis.
                glm::vec3 pos(
                    r * sinTheta * cosf(phi),
                    r * cosf(theta),
                    r * sinTheta * sinf(phi)
                );

                const float omega = 0.12f + dis(gen) * 0.72f;
                points.push_back({pos, glm::vec3(0.0f), density, omega});

                if ((i & 0x7FF) == 0) {
                    const int local = count > 0 ? i * (progressEnd - progressBegin) / count : 0;
                    m_buildProgress.store(std::clamp(progressBegin + local, 0, 99));
                }
            }

            return points;
        };

        auto publish = [&](std::vector<CloudPoint> &&points, bool finalCloud) {
            if (m_buildCancelled.load()) return;

            float maxD = 1e-6f;
            for (const auto &point: points) {
                if (std::isfinite(point.brightness)) {
                    maxD = std::max(maxD, point.brightness);
                }
            }

            {
                // The render thread picks up m_pendingCloud in drawScene(). The
                // mutex protects the handoff, while the atomic flag avoids
                // locking every frame when no new cloud is ready.
                std::lock_guard<std::mutex> lock(m_swapMutex);
                m_pendingCloud = std::move(points);
                m_pendingKey = key;
                m_pendingMaxDensity = maxD;
                m_pendingIsFinal = finalCloud;
            }

            m_cloudReady.store(true);
            m_buildStage.store(static_cast<int>(finalCloud ? CloudBuildStage::Complete : CloudBuildStage::PreviewReady));
            if (finalCloud) {
                m_buildProgress.store(100);
            }
        };

        const bool progressive = previewPoints < targetPoints;
        // Show something quickly, then spend extra time on the final density if
        // the requested cloud is expensive.
        auto preview = generatePoints(previewPoints, 55, progressive ? 68 : 100);
        if (m_buildCancelled.load()) return;
        publish(std::move(preview), !progressive);

        if (!progressive) return;

        for (int spin = 0; spin < 60 && m_cloudReady.load() && !m_buildCancelled.load(); ++spin) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        m_buildStage.store(static_cast<int>(CloudBuildStage::Refining));
        auto refined = generatePoints(targetPoints, 68, 100);
        if (m_buildCancelled.load()) return;
        publish(std::move(refined), true);
    });
}

void Engine::resetSimulation() {
    // R means "exactly as launched": cancel background work, restore launch
    // config/state, clear transient caches, and rebuild from scratch.
    m_buildCancelled.store(true);
    if (m_buildThread.joinable()) {
        m_buildThread.join();
    }

    state = m_launchState;
    applyRuntimeConfig(m_launchConfig);
    applyTheme(theme);
    resetCameraToLaunchPose();
    electronAngle = 0.0f;
    m_firstMouse = true;
    m_mouseButtonDown = -1;
    m_showAxes = true;
    m_showElectronTracker = true;
    m_lastScreenshotPath.clear();
    m_cloudCache.clear();
    cloudPoints.clear();
    m_pendingCloud.clear();
    m_cloudReady.store(false);
    m_buildProgress.store(0);
    m_buildStage.store(static_cast<int>(CloudBuildStage::Idle));
    m_vboDirty = true;
    regenerateCloud();
}

void Engine::regenerateSinglePoint(CloudPoint &p) {
    const float maxR = orbitalMaxRadius(state);
    const float maxTargetProb = std::max(m_cachedMaxDensity, 1e-20f);

    for (int attempts = 0; attempts < 20000; ++attempts) {
        const float r = cbrtf(m_dis(m_gen)) * maxR;
        const float theta = acosf(2.0f * m_dis(m_gen) - 1.0f);
        const float phi = 2.0f * PI * m_dis(m_gen);

        const float prob = QuantumSimulation::computeProbability(r, theta, phi, state);
        if (prob > m_dis(m_gen) * maxTargetProb) {
            const float sinTheta = sinf(theta);
            p.pos = glm::vec3(r * sinTheta * cosf(phi),
                              r * cosf(theta),
                              r * sinTheta * sinf(phi));
            p.vel = glm::vec3(0.0f);
            p.brightness = prob;
            p.omega = 0.35f;
            return;
        }
    }

    p = {};
}

void Engine::updatePhysics(float deltaTime) {
    const float orbitSpeed = 4.5f / static_cast<float>(state.n * state.n);
    electronAngle += orbitSpeed * animationSpeed * deltaTime;
    if (electronAngle > 2.0f * PI) {
        electronAngle = fmodf(electronAngle, 2.0f * PI);
    }
}

void Engine::initShaders() {
    m_cloudProgram = ShaderLoader::createProgramFromFiles("cloud.vert.glsl", "cloud.frag.glsl");
    m_axesProgram = ShaderLoader::createProgramFromFiles("axes.vert.glsl", "axes.frag.glsl");

    m_uCloudViewProjection = glGetUniformLocation(m_cloudProgram, "uViewProjection");
    m_uCloudTime = glGetUniformLocation(m_cloudProgram, "uTime");
    m_uCloudM = glGetUniformLocation(m_cloudProgram, "uMFloat");
    m_uCloudColorIntensity = glGetUniformLocation(m_cloudProgram, "uColorIntensity");
    m_uCloudClipEnabled = glGetUniformLocation(m_cloudProgram, "uClipEnabled");
    m_uCloudClipPlane = glGetUniformLocation(m_cloudProgram, "uClipPlane");
    m_uCloudClipMode = glGetUniformLocation(m_cloudProgram, "uClipMode");
    m_uCloudDensityThreshold = glGetUniformLocation(m_cloudProgram, "uDensityThreshold");
    m_uCloudRenderMode = glGetUniformLocation(m_cloudProgram, "uRenderMode");
    m_uCloudColorMap = glGetUniformLocation(m_cloudProgram, "uColorMap");
    m_uCloudPointSize = glGetUniformLocation(m_cloudProgram, "uPointSize");
    m_uCloudAnimationSpeed = glGetUniformLocation(m_cloudProgram, "uAnimationSpeed");
    m_uCloudIsoLevel = glGetUniformLocation(m_cloudProgram, "uIsoLevel");
    m_uCloudIsoWidth = glGetUniformLocation(m_cloudProgram, "uIsoWidth");
    m_uCloudTint = glGetUniformLocation(m_cloudProgram, "uTintColor");

    m_uAxesViewProjection = glGetUniformLocation(m_axesProgram, "uViewProjection");
    m_uAxesPointSize = glGetUniformLocation(m_axesProgram, "uPointSize");
}

void Engine::initGlfwWindow() {
    if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW.");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window.");
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(m_config.vsync ? 1 : 0);
    glfwSetWindowUserPointer(window, this);
}

void Engine::initOpenGL() {
    if (!loadOpenGLFunctions()) {
        throw std::runtime_error("Failed to initialize GLAD.");
    }

    if (!GLAD_GL_VERSION_3_3) {
        throw std::runtime_error("QuantumAtom requires OpenGL 3.3 or newer.");
    }

    glViewport(0, 0, m_width, m_height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);

    initShaders();
    initStaticGeometry();
}

void Engine::initStaticGeometry() {
    const float axisLength = 160.0f;
    const std::array<float, 36> axes = {
        0.0f, 0.0f, 0.0f, 1.0f, 0.18f, 0.18f,
        axisLength, 0.0f, 0.0f, 1.0f, 0.18f, 0.18f,
        0.0f, 0.0f, 0.0f, 0.25f, 0.95f, 0.35f,
        0.0f, axisLength, 0.0f, 0.25f, 0.95f, 0.35f,
        0.0f, 0.0f, 0.0f, 0.3f, 0.55f, 1.0f,
        0.0f, 0.0f, axisLength, 0.3f, 0.55f, 1.0f,
    };

    glGenVertexArrays(1, &m_axesVao);
    glGenBuffers(1, &m_axesVbo);
    glBindVertexArray(m_axesVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_axesVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(axes.size() * sizeof(float)), axes.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void *>(3 * sizeof(float)));
    glBindVertexArray(0);

    glGenVertexArrays(1, &m_activeVao);
    glGenBuffers(1, &m_activeVbo);
    glBindVertexArray(m_activeVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_activeVbo);
    glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void *>(3 * sizeof(float)));
    glBindVertexArray(0);
}

void Engine::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    applyTheme(theme);
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void Engine::setupCallbacks() {
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow *win, int w, int h) {
        auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
        eng->m_width = std::max(1, w);
        eng->m_height = std::max(1, h);
        glViewport(0, 0, eng->m_width, eng->m_height);
    });

    glfwSetKeyCallback(window, [](GLFWwindow *win, int key, int scancode, int action, int mods) {
        ImGui_ImplGlfw_KeyCallback(win, key, scancode, action, mods);
        auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));

        if (ImGui::GetIO().WantCaptureKeyboard || !(action == GLFW_PRESS || action == GLFW_REPEAT)) {
            return;
        }

        bool changed = false;
        if ((key == GLFW_KEY_R || key == GLFW_KEY_SPACE) && action == GLFW_PRESS) {
            eng->resetSimulation();
            return;
        }
        if (key == GLFW_KEY_S && action == GLFW_PRESS) {
            eng->requestScreenshot();
            return;
        }
        if (key == GLFW_KEY_UP) {
            eng->state.n = std::min(eng->state.n + 1, QUANTUMATOM_MAX_N);
            changed = true;
        }
        if (key == GLFW_KEY_DOWN) {
            eng->state.n = std::max(eng->state.n - 1, 1);
            changed = true;
        }
        if (key == GLFW_KEY_C && action == GLFW_PRESS) {
            eng->clipEnabled = !eng->clipEnabled;
        }

        if (changed) {
            eng->state.l = std::clamp(eng->state.l, 0, eng->state.n - 1);
            eng->state.m = std::clamp(eng->state.m, -eng->state.l, eng->state.l);
            eng->regenerateCloud();
        }
    });

    glfwSetMouseButtonCallback(window, [](GLFWwindow *win, int button, int action, int mods) {
        ImGui_ImplGlfw_MouseButtonCallback(win, button, action, mods);
        auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
        if (!ImGui::GetIO().WantCaptureMouse) {
            eng->m_mouseButtonDown = action == GLFW_PRESS ? button : -1;
        } else {
            eng->m_mouseButtonDown = -1;
        }
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

        const float dx = static_cast<float>(xpos) - eng->m_lastMouseX;
        const float dy = eng->m_lastMouseY - static_cast<float>(ypos);
        eng->m_lastMouseX = static_cast<float>(xpos);
        eng->m_lastMouseY = static_cast<float>(ypos);

        if (eng->m_mouseButtonDown != -1 && !ImGui::GetIO().WantCaptureMouse) {
            const bool shift = glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                               glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            if (shift || eng->m_mouseButtonDown == GLFW_MOUSE_BUTTON_RIGHT) {
                eng->camera.pan(dx, dy);
            } else if (eng->m_mouseButtonDown == GLFW_MOUSE_BUTTON_LEFT) {
                eng->camera.targetYaw += dx * 0.18f;
                eng->camera.targetPitch += dy * 0.18f;
                eng->camera.targetPitch = std::clamp(eng->camera.targetPitch, -89.0f, 89.0f);
            }
        }
    });

    glfwSetScrollCallback(window, [](GLFWwindow *win, double xoff, double yoff) {
        ImGui_ImplGlfw_ScrollCallback(win, xoff, yoff);
        (void) xoff;
        if (!ImGui::GetIO().WantCaptureMouse) {
            auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
            const float maxDistance = std::max(1600.0f, 32.0f * static_cast<float>(eng->state.n * eng->state.n));
            eng->camera.targetDistance -= static_cast<float>(yoff) * 22.0f;
            eng->camera.targetDistance = std::clamp(eng->camera.targetDistance, 45.0f, maxDistance);
        }
    });

    glfwSetCharCallback(window, [](GLFWwindow *win, unsigned int codepoint) {
        ImGui_ImplGlfw_CharCallback(win, codepoint);
    });
}

void Engine::renderUI() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // FPS is averaged over half-second windows so the HUD stays stable instead
    // of flickering every frame.
    ImGuiIO &io = ImGui::GetIO();
    const double currentTime = glfwGetTime();
    ++m_frameCount;
    if (currentTime - m_lastFpsUpdateTime >= 0.5) {
        m_fps = static_cast<float>(m_frameCount) /
                static_cast<float>(currentTime - m_lastFpsUpdateTime);
        m_frameTimeMs = m_fps > 0.0f ? 1000.0f / m_fps : 0.0f;
        m_frameCount = 0;
        m_lastFpsUpdateTime = currentTime;
    }

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 15.0f, 15.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.58f);
    // The HUD is read-only and intentionally separated from the main controls.
    constexpr ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration |
                                          ImGuiWindowFlags_AlwaysAutoResize |
                                          ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_NoFocusOnAppearing |
                                          ImGuiWindowFlags_NoNav |
                                          ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("HUD", nullptr, hudFlags)) {
        ImGui::Text("QuantumAtom %s", QUANTUMATOM_VERSION_STRING);
        ImGui::Separator();
        ImGui::Text("FPS %.1f  %.2f ms", m_fps, m_frameTimeMs);
        ImGui::Text("Points %d / %d", static_cast<int>(cloudPoints.size()), pointBudget);
        const auto stage = static_cast<CloudBuildStage>(m_buildStage.load());
        ImGui::Text("Build %s", stageName(stage));
        const int pct = m_buildProgress.load();
        if (pct < 100 && stage != CloudBuildStage::Idle && stage != CloudBuildStage::CacheHit) {
            char label[16];
            snprintf(label, sizeof(label), "%d%%", pct);
            ImGui::ProgressBar(static_cast<float>(pct) / 100.0f, ImVec2(220.0f, 0.0f), label);
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(455.0f, 690.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("QuantumAtom Controls");

    bool stateChanged = false;
    bool pointBudgetChanged = false;

    // Tabs keep the growing toolset discoverable without making the first panel
    // taller than the screen.
    if (ImGui::BeginTabBar("MainTabs")) {
        if (ImGui::BeginTabItem("Quantum Numbers")) {
            ImGui::SliderInt("n", &state.n, 1, QUANTUMATOM_MAX_N);
            const bool nFinished = ImGui::IsItemDeactivatedAfterEdit();
            state.n = std::clamp(state.n, 1, QUANTUMATOM_MAX_N);
            state.l = std::clamp(state.l, 0, state.n - 1);
            state.m = std::clamp(state.m, -state.l, state.l);
            helpMarker("Principal quantum number. Higher n creates larger orbitals and more radial nodes.");
            if (nFinished) stateChanged = true;

            ImGui::SliderInt("l", &state.l, 0, state.n - 1);
            const bool lFinished = ImGui::IsItemDeactivatedAfterEdit();
            state.l = std::clamp(state.l, 0, state.n - 1);
            state.m = std::clamp(state.m, -state.l, state.l);
            helpMarker("Azimuthal quantum number. l=0,1,2,3 map to s,p,d,f; larger l values are high angular-momentum states.");
            if (lFinished) stateChanged = true;

            ImGui::SliderInt("m", &state.m, -state.l, state.l);
            const bool mFinished = ImGui::IsItemDeactivatedAfterEdit();
            state.m = std::clamp(state.m, -state.l, state.l);
            helpMarker("Magnetic quantum number. The sign changes phase-flow direction; |m| changes angular structure.");
            if (mFinished) stateChanged = true;

            ImGui::Spacing();
            ImGui::SeparatorText("Presets");
            const float buttonWidth = 62.0f;
            int column = 0;
            for (const auto &preset: kOrbitalPresets) {
                if (preset.n > QUANTUMATOM_MAX_N) continue;
                if (column > 0) ImGui::SameLine();
                if (ImGui::Button(preset.label.data(), ImVec2(buttonWidth, 0.0f))) {
                    state = {preset.n, preset.l, preset.m};
                    stateChanged = true;
                }
                column = (column + 1) % 5;
            }

            ImGui::Spacing();
            ImGui::TextWrapped("Current state: n=%d, l=%d, m=%d", state.n, state.l, state.m);
            if (state.n >= 7) {
                ImGui::TextWrapped("n=7 and n=8 use a quick preview cloud first, then refine in the background.");
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Rendering")) {
            (void) renderModeCombo("Mode", renderMode);
            helpMarker("Density points are standard samples; glow and halo are additive billboards; iso-shell filters around a normalized density band; phase flow animates m-dependent phase.");

            (void) colorMapCombo("Colormap", colorMap);
            helpMarker("Colormap lookup is evaluated in the fragment shader.");

            ImGui::SliderInt("Point count", &pointBudget, kMinimumPointCount, kMaximumPointCount);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                pointBudget = std::clamp(pointBudget, kMinimumPointCount, kMaximumPointCount);
                pointBudgetChanged = true;
            }
            helpMarker("Changing this rebuilds the cloud. Larger values improve density at the cost of memory and generation time.");

            ImGui::SliderFloat("Density threshold", &densityThreshold, 0.0f, 0.95f, "%.3f");
            ImGui::SliderFloat("Point size", &pointSize, 1.5f, 32.0f, "%.1f");
            ImGui::SliderFloat("Color intensity", &colorIntensity, 0.1f, 10.0f, "%.2f");
            ImGui::SliderFloat("Animation speed", &animationSpeed, 0.0f, 4.0f, "%.2f");

            if (renderMode == RenderMode::IsoShell) {
                ImGui::SliderFloat("Iso level", &isoLevel, 0.02f, 0.98f, "%.3f");
                ImGui::SliderFloat("Iso width", &isoWidth, 0.005f, 0.25f, "%.3f");
            }

            ImGui::Checkbox("Enable clipping", &clipEnabled);
            (void) clipModeCombo("Clip mode", clipMode);
            helpMarker("X plane removes points beyond the adjustable X value. +X +Y removes the positive XY quadrant. +X +Y +Z restores the old positive-octant clip.");
            if (clipMode == ClipMode::XPlane) {
                ImGui::SliderFloat("Clip X", &clipPlane, -600.0f, 600.0f, "%.1f");
            }

            ImGui::Checkbox("Axes", &m_showAxes);
            ImGui::SameLine();
            ImGui::Checkbox("Electron tracker", &m_showElectronTracker);

            ImGui::ColorEdit3("Point tint", glm::value_ptr(pointTint));
            ImGui::ColorEdit3("Background", glm::value_ptr(backgroundColor));

            if (themeCombo("Theme", theme)) {
                applyTheme(theme);
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Camera")) {
            ImGui::SliderFloat("Yaw", &camera.targetYaw, -180.0f, 180.0f, "%.1f");
            ImGui::SliderFloat("Pitch", &camera.targetPitch, -89.0f, 89.0f, "%.1f");
            ImGui::SliderFloat("Distance", &camera.targetDistance, 45.0f, 2200.0f, "%.1f");
            ImGui::SliderFloat("Smoothing", &camera.smoothness, 0.1f, 12.0f, "%.2f");

            if (ImGui::Button("Reset View")) {
                resetCameraToLaunchPose();
            }

            ImGui::TextWrapped("Left drag rotates. Right drag or Shift+left drag pans. Mouse wheel zooms.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Export")) {
            if (ImGui::Button("Screenshot PNG")) {
                requestScreenshot();
            }
            helpMarker("Captures the current framebuffer, including the UI, into the screenshots directory.");

            if (!m_lastScreenshotPath.empty()) {
                ImGui::TextWrapped("Last screenshot: %s", m_lastScreenshotPath.c_str());
            }

            if (ImGui::Button("Clear cloud cache")) {
                m_cloudCache.clear();
            }
            ImGui::TextWrapped("Cached clouds: %d / %d",
                               static_cast<int>(m_cloudCache.size()),
                               static_cast<int>(m_cacheLimit));
            ImGui::TextWrapped("Keyboard: S saves a screenshot, C toggles clipping, Space/R resets.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Info")) {
            ImGui::Text("Hydrogen-like orbital model");
            ImGui::Separator();
            ImGui::TextWrapped("psi_nlm(r, theta, phi) = R_nl(r) Y_l^m(theta, phi)");
            ImGui::TextWrapped("rho(r, theta, phi) = |psi_nlm|^2");
            ImGui::TextWrapped("R_nl is evaluated with associated Laguerre polynomials. Y_l^m is evaluated with associated Legendre polynomials and normalized factorial terms.");
            ImGui::TextWrapped("The cloud is sampled from rho * r^2 on a radial/theta density grid, then phi is sampled uniformly. This keeps high-n orbitals responsive and avoids slow rejection-sampling stalls.");
            ImGui::Spacing();
            ImGui::TextWrapped("%s", QUANTUMATOM_PROJECT_DESCRIPTION);
            ImGui::TextWrapped("%s", QUANTUMATOM_PROJECT_HOMEPAGE);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();

    if (stateChanged || pointBudgetChanged) {
        regenerateCloud();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Engine::drawScene(float currentFrameTime, float deltaTime) {
    if (m_cloudReady.load()) {
        // Accept completed CPU work at the start of the frame. OpenGL uploads
        // happen later in drawCloud(), safely on the render thread.
        CloudCacheEntry finalEntry;
        bool shouldCache = false;
        {
            std::lock_guard<std::mutex> lock(m_swapMutex);
            cloudPoints = std::move(m_pendingCloud);
            m_cachedMaxDensity = m_pendingMaxDensity;
            if (m_pendingIsFinal) {
                finalEntry = {m_pendingKey, cloudPoints, m_pendingMaxDensity};
                shouldCache = true;
            }
        }
        if (shouldCache) {
            cacheCloud(finalEntry);
        }
        m_cloudReady.store(false);
        m_vboDirty = true;
    }

    glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    camera.update(m_width, m_height, deltaTime);
    const glm::mat4 viewProjection = camera.getViewProjectionMatrix(m_width, m_height);

    drawAxes(viewProjection);
    drawCloud(currentFrameTime, viewProjection);
    drawActiveElectron(viewProjection);

    renderUI();

    if (m_screenshotRequested) {
        if (!saveScreenshotPng()) {
            std::cerr << "[Export] Failed to write screenshot." << std::endl;
        }

        m_screenshotRequested = false;
    }
}

void Engine::drawAxes(const glm::mat4 &viewProjection) {
    if (!m_showAxes || !m_axesProgram || !m_axesVao) return;

    glUseProgram(m_axesProgram);
    glUniformMatrix4fv(m_uAxesViewProjection, 1, GL_FALSE, glm::value_ptr(viewProjection));
    glUniform1f(m_uAxesPointSize, 1.0f);
    glBindVertexArray(m_axesVao);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Engine::drawCloud(float timeVal, const glm::mat4 &viewProjection) {
    if (cloudPoints.empty()) return;

    if (m_vboDirty) {
        // Keep static attributes in separate buffers. The shader reads position,
        // normalized density, and per-point animation speed independently.
        const size_t n = cloudPoints.size();

        float maxD = 1e-6f;
        for (const auto &point: cloudPoints) {
            if (std::isfinite(point.brightness)) {
                maxD = std::max(maxD, point.brightness);
            }
        }
        m_cachedMaxDensity = maxD;
        const float invMax = 1.0f / std::max(maxD, 1e-20f);

        std::vector<float> positions(n * 3);
        std::vector<float> norms(n);
        std::vector<float> omegas(n);

        for (size_t i = 0; i < n; ++i) {
            const auto &point = cloudPoints[i];
            positions[i * 3 + 0] = point.pos.x;
            positions[i * 3 + 1] = point.pos.y;
            positions[i * 3 + 2] = point.pos.z;
            norms[i] = std::clamp(point.brightness * invMax, 0.0f, 1.0f);
            omegas[i] = point.omega;
        }

        if (!m_cloudVao) glGenVertexArrays(1, &m_cloudVao);
        if (!m_posVbo) glGenBuffers(1, &m_posVbo);
        if (!m_normVbo) glGenBuffers(1, &m_normVbo);
        if (!m_omegaVbo) glGenBuffers(1, &m_omegaVbo);

        glBindVertexArray(m_cloudVao);

        glBindBuffer(GL_ARRAY_BUFFER, m_posVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(positions.size() * sizeof(float)),
                     positions.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

        glBindBuffer(GL_ARRAY_BUFFER, m_normVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(norms.size() * sizeof(float)),
                     norms.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, nullptr);

        glBindBuffer(GL_ARRAY_BUFFER, m_omegaVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(omegas.size() * sizeof(float)),
                     omegas.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, nullptr);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_vboDirty = false;
    }

    // Additive blending makes glow/halo modes accumulate light. Depth writes are
    // disabled so translucent point sprites do not punch holes into later points.
    glEnable(GL_BLEND);
    const bool additive = renderMode == RenderMode::GlowBillboards ||
                          renderMode == RenderMode::PhaseFlow ||
                          renderMode == RenderMode::HaloFog;
    glBlendFunc(GL_SRC_ALPHA, additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glUseProgram(m_cloudProgram);
    // Most visual controls are uniforms, so changing color, threshold, clipping,
    // or render mode does not require regenerating or re-uploading the cloud.
    glUniformMatrix4fv(m_uCloudViewProjection, 1, GL_FALSE, glm::value_ptr(viewProjection));
    glUniform1f(m_uCloudTime, timeVal);
    glUniform1f(m_uCloudM, static_cast<float>(state.m));
    glUniform1f(m_uCloudColorIntensity, colorIntensity);
    glUniform1i(m_uCloudClipEnabled, clipEnabled ? 1 : 0);
    glUniform1f(m_uCloudClipPlane, clipPlane);
    glUniform1i(m_uCloudClipMode, static_cast<int>(clipMode));
    glUniform1f(m_uCloudDensityThreshold, densityThreshold);
    glUniform1i(m_uCloudRenderMode, static_cast<int>(renderMode));
    glUniform1i(m_uCloudColorMap, static_cast<int>(colorMap));
    glUniform1f(m_uCloudPointSize, pointSize);
    glUniform1f(m_uCloudAnimationSpeed, animationSpeed);
    glUniform1f(m_uCloudIsoLevel, isoLevel);
    glUniform1f(m_uCloudIsoWidth, isoWidth);
    glUniform3fv(m_uCloudTint, 1, glm::value_ptr(pointTint));

    glBindVertexArray(m_cloudVao);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(cloudPoints.size()));
    glBindVertexArray(0);

    glUseProgram(0);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Engine::drawActiveElectron(const glm::mat4 &viewProjection) {
    if (!m_showElectronTracker || !m_activeVao || !m_axesProgram) return;

    const float radius = 5.25f * static_cast<float>(state.n * state.n);
    const float x = radius * cosf(electronAngle);
    const float y = state.l > 0 ? radius * sinf(electronAngle) * 0.35f : 0.0f;
    const float z = state.l > 0
                        ? radius * sinf(electronAngle) * sqrtf(1.0f - 0.35f * 0.35f)
                        : radius * sinf(electronAngle);

    const std::array<float, 6> point = {
        x, y, z,
        1.0f, 1.0f, 1.0f
    };

    glUseProgram(m_axesProgram);
    glUniformMatrix4fv(m_uAxesViewProjection, 1, GL_FALSE, glm::value_ptr(viewProjection));
    glUniform1f(m_uAxesPointSize, 12.0f);
    glBindVertexArray(m_activeVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_activeVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(point.size() * sizeof(float)), point.data());
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Engine::requestScreenshot() {
    m_screenshotRequested = true;
}

bool Engine::saveScreenshotPng() {
    std::vector<unsigned char> pixels(static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 3);
    std::vector<unsigned char> flipped(pixels.size());

    // OpenGL returns pixels bottom-up; PNG files expect the first row to be the
    // top of the image, so the rows are flipped before writing.
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, m_width, m_height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    const size_t rowBytes = static_cast<size_t>(m_width) * 3;
    for (int y = 0; y < m_height; ++y) {
        const auto src = pixels.begin() + static_cast<std::ptrdiff_t>(y) * static_cast<std::ptrdiff_t>(rowBytes);
        const auto dst = flipped.begin() +
                         static_cast<std::ptrdiff_t>(m_height - 1 - y) *
                         static_cast<std::ptrdiff_t>(rowBytes);
        std::copy(src, src + static_cast<std::ptrdiff_t>(rowBytes), dst);
    }

    std::error_code ec;
    std::filesystem::create_directories("screenshots", ec);

    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    const std::tm tm = localTime(nowTime);

    std::ostringstream name;
    name << "screenshots/QuantumAtom_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".png";
    m_lastScreenshotPath = name.str();

    return stbi_write_png(m_lastScreenshotPath.c_str(), m_width, m_height, 3,
                          flipped.data(), static_cast<int>(rowBytes)) != 0;
}
