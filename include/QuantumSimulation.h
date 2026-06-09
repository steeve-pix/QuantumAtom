#ifndef QUANTUM_SIMULATION_H
#define QUANTUM_SIMULATION_H

#include "QuantumTypes.h"

/**
 * @brief Mathematical engine for computing quantum probability densities.
 * 
 * This class provides static methods to evaluate the wave function solutions
 * to the Schrödinger equation for a hydrogen-like atom. It uses spherical
 * coordinates (r, theta, phi) and analytical solutions involving associated
 * Laguerre polynomials and Spherical Harmonics.
 */
class QuantumSimulation {
public:
    /**
     * @brief Computes the probability density at a given spherical coordinate.
     * 
     * Calculates |Psi(r, theta, phi)|^2, where Psi is the wave function.
     * @param r Radial distance
     * @param theta Polar angle (colatitude)
     * @param phi Azimuthal angle (longitude)
     * @param state Current quantum state (n, l, m)
     * @return Calculated probability density
     */
    static float computeProbability(float r, float theta, float phi, const QuantumState &state);

private:
    /**
     * @brief Evaluates the Associated Legendre Polynomial P_l^m(x).
     */
    static float associatedLegendre(int l, int m, float x);

    /**
     * @brief Evaluates the Associated Laguerre Polynomial L_k^alpha(x).
     */
    static float associatedLaguerre(int k, int alpha, float x);

    /**
     * @brief Computes the real Spherical Harmonic Y_l^m(theta, phi).
     */
    static float sphericalHarmonic(int l, int m, float theta, float phi);
};

#endif // QUANTUM_SIMULATION_H
