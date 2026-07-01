#pragma once

#include <glm/glm.hpp>

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
        return n >= 1 && l >= 0 && l < n && m >= -l && m <= l;
    }
    
    /**
     * @brief Checks if this state differs from another.
     */
    bool operator!=(const QuantumState& other) const {
        return n != other.n || l != other.l || m != other.m;
    }
};
