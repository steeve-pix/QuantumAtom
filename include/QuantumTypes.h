#pragma once

#include <glm/glm.hpp>

/**
 * @struct CloudPoint
 * @brief Represents a single data point within the quantum probability cloud.
 */
struct CloudPoint {
    glm::vec3 pos;        /**< 3D position of the point */
    glm::vec3 vel;        /**< Velocity vector (reserved for future physics simulations) */
    float brightness;     /**< Probability density value at this location */
    float speedFactor;    /**< Multiplier for the point's rotation speed */
};

/**
 * @struct QuantumState
 * @brief Defines the quantum numbers that describe an electron's state in an atom.
 */
struct QuantumState {
    int n = 1; /**< Principal quantum number: Determines the size and energy of the orbital */
    int l = 0; /**< Azimuthal quantum number: Determines the shape of the orbital */
    int m = 0; /**< Magnetic quantum number: Determines the orientation of the orbital in space */
};