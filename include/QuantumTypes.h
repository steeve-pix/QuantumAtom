#ifndef QUANTUM_TYPES_H
#define QUANTUM_TYPES_H

#include <glm/glm.hpp>

/**
 * @brief Represents a single point in the probability cloud
 */
struct CloudPoint {
    glm::vec3 pos;
    glm::vec3 vel;
    float brightness;
};

/**
 * @brief Quantum numbers representing the state of the electron
 */
struct QuantumState {
    int n = 1; ///< Principal quantum number
    int l = 0; ///< Azimuthal quantum number
    int m = 0; ///< Magnetic quantum number
};

#endif // QUANTUM_TYPES_H
