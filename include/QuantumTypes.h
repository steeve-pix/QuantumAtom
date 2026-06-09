#ifndef QUANTUM_TYPES_H
#define QUANTUM_TYPES_H

#include <glm/glm.hpp>

/**
 * @brief Represents a single point in the probability cloud.
 * 
 * Each point in the visual cloud stores its position, an optional velocity
 * (for future kinematic simulations), and its relative brightness based on 
 * the probability density at that point.
 */
struct CloudPoint {
    glm::vec3 pos;        ///< 3D position in the simulation space
    glm::vec3 vel;        ///< 3D velocity vector
    float brightness;     ///< Normalized probability density value
};

/**
 * @brief Quantum numbers representing the state of the electron.
 * 
 * These three integers uniquely define the atomic orbital in a hydrogen-like atom.
 */
struct QuantumState {
    int n = 1; ///< Principal quantum number (n > 0). Defines energy and shell size.
    int l = 0; ///< Azimuthal quantum number (0 <= l < n). Defines orbital shape (s, p, d, f).
    int m = 0; ///< Magnetic quantum number (-l <= m <= l). Defines orbital orientation.
};

#endif // QUANTUM_TYPES_H
