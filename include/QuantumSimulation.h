#pragma once

#include "QuantumTypes.h"

/**
 * @class QuantumSimulation
 * @brief Mathematical engine for calculating electron probability distributions.
 */
class QuantumSimulation {
public:
    /**
     * @brief Computes the probability density at a point in spherical coordinates.
     * @param r Radial distance from the nucleus
     * @param theta Polar angle
     * @param phi Azimuthal angle
     * @param state Current quantum numbers (n, l, m)
     * @return Probability density value
     */
    static float computeProbability(float r, float theta, float phi, const QuantumState &state);

private:
    /**
     * @brief Computes the Associated Legendre polynomial.
     */
    static float associatedLegendre(int l, int m, float x);

    /**
     * @brief Computes the Associated Laguerre polynomial.
     */
    static float associatedLaguerre(int k, int alpha, float x);

    /**
     * @brief Computes the spherical harmonic component of the wavefunction.
     */
    static float sphericalHarmonic(int l, int m, float theta, float phi);
};