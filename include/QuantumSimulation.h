#ifndef QUANTUM_SIMULATION_H
#define QUANTUM_SIMULATION_H

#include "QuantumTypes.h"

// This class is the "brain" that calculates where the electron is likely to be.
// It uses complex math formulas to figure out the shape of the electron cloud.
class QuantumSimulation {
public:
    // This is the main function you call to get the probability of finding 
    // an electron at a specific spot (r, theta, phi) for a certain atom state.
    static float computeProbability(float r, float theta, float phi, const QuantumState &state);

private:
    // These are helper functions for the advanced math involved in quantum mechanics.
    // Think of them as special building blocks for the final calculation.
    static float associatedLegendre(int l, int m, float x);
    static float associatedLaguerre(int k, int alpha, float x);
    static float sphericalHarmonic(int l, int m, float theta, float phi);
};

#endif
