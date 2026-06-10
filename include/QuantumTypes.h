#ifndef QUANTUM_TYPES_H
#define QUANTUM_TYPES_H

#include <glm/glm.hpp>

// This structure represents a single dot in the 3D cloud.
// It keeps track of where the dot is (pos) and how bright it should look.
struct CloudPoint {
    glm::vec3 pos;
    glm::vec3 vel;
    float brightness;
    float speedFactor;
};

// This structure holds the "settings" for the atom's state.
// In physics, these numbers (n, l, m) decide the shape of the electron's home.
struct QuantumState {
    int n = 1; // Size/Energy level
    int l = 0; // Shape (0 is a sphere, higher numbers get more complex)
    int m = 0; // Direction (which way the shape is pointing)
};

#endif
