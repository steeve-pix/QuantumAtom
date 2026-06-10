#ifndef QUANTUM_SIMULATION_H
#define QUANTUM_SIMULATION_H

#include "QuantumTypes.h"

/*
 * The 'QuantumSimulation' class serves as the mathematical core of the program.
 * It contains the physics formulas required to calculate electron probability density.
 */
class QuantumSimulation {
public:
    /* 
     * Calculates the probability of an electron being at a specific 
     * spherical coordinate (r, theta, phi) given a specific quantum state.
     */
    static float computeProbability(float r, float theta, float phi, const QuantumState &state);

private:
    /*
     * Associated Legendre polynomials: Used for the angular part of the wavefunction.
     */
    static float associatedLegendre(int l, int m, float x);

    /*
     * Associated Laguerre polynomials: Used for the radial part of the wavefunction.
     */
    static float associatedLaguerre(int k, int alpha, float x);

    /*
     * Spherical Harmonics: Combines angular components to define the orbital shape in 3D.
     */
    static float sphericalHarmonic(int l, int m, float theta, float phi);
};

#endif
