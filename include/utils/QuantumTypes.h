#pragma once

#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <string_view>

#ifndef QUANTUMATOM_MAX_N
// CMake can override this at compile time. Keep the source default at the
// highest orbital currently tuned for responsive background generation.
#define QUANTUMATOM_MAX_N 8
#endif

#ifndef QUANTUMATOM_DEFAULT_POINT_COUNT
// A conservative default keeps startup fast on laptops while still giving a
// dense enough cloud for the lower orbitals.
#define QUANTUMATOM_DEFAULT_POINT_COUNT 120000
#endif

// RenderMode values are sent directly to the cloud shaders, so keep this enum
// synchronized with shaders/cloud.vert.glsl and shaders/cloud.frag.glsl.
enum class RenderMode : int {
    DensityPoints = 0,
    GlowBillboards = 1,
    IsoShell = 2,
    PhaseFlow = 3,
    HaloFog = 4
};

// Small shader-side colormap ramps. The UI stores the selected value in config
// and the fragment shader converts normalized density into final color.
enum class ColorMap : int {
    Inferno = 0,
    Viridis = 1,
    Plasma = 2,
    Magma = 3,
    Cividis = 4
};

enum class UiTheme : int {
    Dark = 0,
    Classic = 1,
    Light = 2
};

enum class UiLanguage : int {
    English = 0,
    French = 1,
    Spanish = 2
};

// Clipping is intentionally shader-side so switching modes does not rebuild the
// probability cloud. XPlane is adjustable; the quadrant/octant modes use zero as
// the split point to reveal the orbital interior quickly.
enum class ClipMode : int {
    XPlane = 0,
    PositiveXY = 1,
    PositiveXYZ = 2
};

// Presets provide common textbook orbitals and a few high-l states through n=8.
struct OrbitalPreset {
    std::string_view label;
    int n;
    int l;
    int m;
};

inline constexpr std::array<OrbitalPreset, 18> kOrbitalPresets{{
    {"1s", 1, 0, 0},
    {"2s", 2, 0, 0},
    {"2p", 2, 1, 0},
    {"2p+", 2, 1, 1},
    {"3s", 3, 0, 0},
    {"3p", 3, 1, 0},
    {"3d", 3, 2, 0},
    {"3d+", 3, 2, 2},
    {"4f", 4, 3, 0},
    {"4f+", 4, 3, 3},
    {"5g", 5, 4, 0},
    {"5g+", 5, 4, 4},
    {"6h", 6, 5, 0},
    {"6h+", 6, 5, 5},
    {"7i", 7, 6, 0},
    {"7i+", 7, 6, 6},
    {"8k", 8, 7, 0},
    {"8k+", 8, 7, 7},
}};

/**
 * @struct CloudPoint
 * @brief Represents a single data point within the quantum probability cloud.
 */
struct CloudPoint {
    glm::vec3 pos;           /**< 3D position of the point */
    glm::vec3 vel;           /**< Velocity vector (reserved for future physics simulations) */
    float brightness;        /**< Probability density value at this location */
    float omega;             /**< Per-point angular velocity factor for phase-flow visualization */
};

/**
 * @struct QuantumState
 * @brief Defines the quantum numbers that describe an electron's state in an atom.
 * 
 * Physical constraints: n >= 1, 0 <= l < n, -l <= m <= l
 */
struct QuantumState {
    int n = 1;  /**< Principal quantum number: Determines the size and energy of the orbital */
    int l = 0;  /**< Azimuthal quantum number: Determines the shape of the orbital */
    int m = 0;  /**< Magnetic quantum number: Determines the orientation of the orbital in space */
    
    /**
     * @brief Validates the quantum state against physical constraints.
     * @return true if n, l, m form a valid quantum configuration
     */
    [[nodiscard]] bool isValid() const {
        return n >= 1 && n <= QUANTUMATOM_MAX_N && l >= 0 && l < n && m >= -l && m <= l;
    }
    
    /**
     * @brief Checks if this state differs from another.
     */
    bool operator!=(const QuantumState& other) const {
        return n != other.n || l != other.l || m != other.m;
    }

    bool operator==(const QuantumState& other) const {
        return !(*this != other);
    }
};

struct CloudBuildSettings {
    QuantumState state;
    int pointCount = QUANTUMATOM_DEFAULT_POINT_COUNT;
};

// UI and config clamps. Raising the upper limit increases memory, upload time,
// and build-thread work, so keep it paired with progressive preview generation.
inline constexpr int kMinimumPointCount = 25000;
inline constexpr int kMaximumPointCount = 600000;
