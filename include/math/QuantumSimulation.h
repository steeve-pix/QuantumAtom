#pragma once

#include "utils/QuantumTypes.h"

/**
 * @class QuantumSimulation
 * @brief Pure mathematical engine for hydrogen-like orbital probability.
 *
 * This class owns no OpenGL or UI state. Keeping it pure makes the expensive
 * density scan easy to run from a background thread.
 */
class QuantumSimulation {
public:
    /**
     * @brief Computes the probability density at a point in spherical coordinates.
     * @param r Radial distance from the nucleus
     * @param theta Polar angle measured from the scene's Y axis
     * @param phi Azimuthal angle around the Y axis
     * @param state Current quantum numbers (n, l, m)
     * @return Probability density value
     */
    static float computeProbability(float r, float theta, float phi, const QuantumState &state);

private:
    /**
     * @brief Computes the associated Legendre polynomial P_l^m(x).
     */
    static float associatedLegendre(int l, int m, float x);

    /**
     * @brief Computes the associated Laguerre polynomial L_k^alpha(x).
     */
    static float associatedLaguerre(int k, int alpha, float x);

    /**
     * @brief Computes |Y_l^m(theta, phi)|^2 for a stationary hydrogen-like state.
     */
    static float sphericalHarmonicProbability(int l, int m, float theta);
};
