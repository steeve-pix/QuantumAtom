#ifndef QUANTUM_SIMULATION_H
#define QUANTUM_SIMULATION_H

#include "QuantumTypes.h"

/**
 * @brief Mathematical engine for computing quantum probability densities
 */
class QuantumSimulation {
public:
    /**
     * @brief Computes the probability density at a given spherical coordinate
     */
    static float computeProbability(float r, float theta, float phi, const QuantumState &state);

private:
    static float associatedLegendre(int l, int m, float x);

    static float associatedLaguerre(int k, int alpha, float x);

    static float sphericalHarmonic(int l, int m, float theta, float phi);
};

#endif // QUANTUM_SIMULATION_H
