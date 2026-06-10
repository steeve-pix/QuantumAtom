#ifndef QUANTUM_TYPES_H
#define QUANTUM_TYPES_H

#include <glm/glm.hpp>

/*
 * CloudPoint represents a single vertex in our orbital visualization.
 * It stores physical properties that determine its behavior and appearance.
 */
struct CloudPoint {
    glm::vec3 pos; // Position in 3D space
    glm::vec3 vel; // Velocity (used for legacy or future physics)
    float brightness; // Probability density at this point
    float speedFactor; // Individual rotation speed variation
};

/*
 * QuantumState defines the specific electronic configuration of the atom.
 * These correspond to the principal (n), azimuthal (l), and magnetic (m) quantum numbers.
 */
struct QuantumState {
    int n = 1; // Principal quantum number (energy level)
    int l = 0; // Azimuthal quantum number (orbital shape)
    int m = 0; // Magnetic quantum number (orientation)
};

#endif
