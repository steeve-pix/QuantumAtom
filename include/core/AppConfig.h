#pragma once

#include "utils/QuantumTypes.h"
#include <filesystem>
#include <glm/glm.hpp>

// AppConfig is the small persistent settings layer for the application. It is
// intentionally plain data so startup, reset, and shutdown can copy it around
// without side effects.
struct AppConfig {
    // Window and render workload defaults.
    int windowWidth = 1280;
    int windowHeight = 720;
    int pointCount = QUANTUMATOM_DEFAULT_POINT_COUNT;

    // Visual controls mirrored by the Rendering tab.
    float densityThreshold = 0.0f;
    float pointSize = 7.0f;
    float colorIntensity = 1.0f;
    float animationSpeed = 1.0f;

    // Clipping defaults. ClipMode::PositiveXYZ matches the original octant-cut
    // behavior the project used before adjustable modes were added.
    float clipPlane = 0.0f;
    bool clipEnabled = false;
    ClipMode clipMode = ClipMode::PositiveXYZ;

    // Presentation defaults saved when the app exits.
    bool vsync = true;
    RenderMode renderMode = RenderMode::DensityPoints;
    ColorMap colorMap = ColorMap::Inferno;
    UiTheme theme = UiTheme::Dark;
    glm::vec3 pointTint = glm::vec3(1.0f);
    glm::vec3 backgroundColor = glm::vec3(0.035f, 0.04f, 0.055f);
    std::filesystem::path sourcePath = "config/QuantumAtom.ini";

    // Look for config files relative to build, install, and release layouts.
    static AppConfig loadDefaultLocations();

    // Persist current runtime defaults back to sourcePath.
    bool save() const;
};
