#include "QuantumSimulation.h"
#include <cmath>
#include <algorithm>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

/*
 * Implementation of the Associated Legendre Polynomials.
 * In quantum mechanics, these describe the angular variation of the wavefunction.
 */
float QuantumSimulation::associatedLegendre(int l, int m, float x) {
    int absM = std::abs(m);
    if (absM > l) return 0.0f;

    float pmm = 1.0f;
    if (absM > 0) {
        float somx2 = std::sqrt((1.0f - x) * (1.0f + x));
        float fact = 1.0f;
        for (int i = 1; i <= absM; i++) {
            pmm *= -fact * somx2;
            fact += 2.0f;
        }
    }
    if (l == absM) return pmm;

    float pmmp1 = x * (2.0f * static_cast<float>(absM) + 1.0f) * pmm;
    if (l == absM + 1) return pmmp1;

    float pll = 0.0f;
    for (int ll = absM + 2; ll <= l; ll++) {
        float f_ll = static_cast<float>(ll);
        pll = (x * (2.0f * f_ll - 1.0f) * pmmp1 - (f_ll + static_cast<float>(absM) - 1.0f) * pmm) / (
                  f_ll - static_cast<float>(absM));
        pmm = pmmp1;
        pmmp1 = pll;
    }
    return pmmp1;
}

/*
 * Implementation of the Associated Laguerre Polynomials.
 * These describe the radial component of the electron's probability distribution.
 */
float QuantumSimulation::associatedLaguerre(int k, int alpha, float x) {
    float f_alpha = static_cast<float>(alpha);
    if (k == 0) return 1.0f;
    if (k == 1) return 1.0f + f_alpha - x;

    float L0 = 1.0f;
    float L1 = 1.0f + f_alpha - x;
    float L2 = 0.0f;

    for (int j = 2; j <= k; j++) {
        float f_j = static_cast<float>(j);
        L2 = ((2.0f * f_j - 1.0f + f_alpha - x) * L1 - (f_j - 1.0f + f_alpha) * L0) / f_j;
        L0 = L1;
        L1 = L2;
    }
    return L2;
}

/*
 * Spherical Harmonics: Defines the complex spatial shapes of the orbitals.
 * Combines the angular terms into a single probability value.
 */
float QuantumSimulation::sphericalHarmonic(int l, int m, float theta, float phi) {
    int absM = std::abs(m);
    float Plm = associatedLegendre(l, absM, std::cos(theta));

    // Normalization factor using Gamma functions for precision at higher quantum numbers.
    float logNumFact = std::lgammaf(static_cast<float>(l - absM + 1));
    float logDenFact = std::lgammaf(static_cast<float>(l + absM + 1));

    float logNormSq = std::log(2.0f * static_cast<float>(l) + 1.0f) - std::log(4.0f * PI) + logNumFact - logDenFact;
    float norm = std::exp(0.5f * logNormSq);

    if (m > 0) {
        return std::sqrt(2.0f) * norm * Plm * std::cos(static_cast<float>(m) * phi);
    } else if (m < 0) {
        return std::sqrt(2.0f) * norm * Plm * std::sin(static_cast<float>(absM) * phi);
    }

    return norm * Plm;
}

/*
 * Main entry point for probability calculations.
 * Computes the total wavefunction (radial * angular) and returns the squared intensity.
 */
float QuantumSimulation::computeProbability(float r, float theta, float phi, const QuantumState &state) {
    float a0 = 4.0f; // Bohr radius (scaled for visualization)
    float n_f = static_cast<float>(state.n);
    float rho = (2.0f * r) / (static_cast<float>(state.n) * a0);

    int k = state.n - state.l - 1;
    int alpha = 2 * state.l + 1;

    // Radial normalization
    float logCubeScale = 3.0f * std::log(2.0f / (n_f * a0));
    float logTopFact = std::lgammaf(static_cast<float>(state.n - state.l));
    float logBottomFact = std::lgammaf(static_cast<float>(state.n + state.l + 1));

    float logRadNormSq = logCubeScale - std::log(2.0f * n_f) + logTopFact - logBottomFact;
    float radNorm = std::exp(0.5f * logRadNormSq);

    // Radial component
    float radial = radNorm * std::exp(-rho / 2.0f) * std::pow(rho, static_cast<float>(state.l)) *
                   associatedLaguerre(k, alpha, rho);

    // Angular component
    float angular = sphericalHarmonic(state.l, state.m, theta, phi);

    // Wavefunction (psi) = Radial * Angular
    float psi = radial * angular;

    // Probability Density = |psi|^2
    return psi * psi;
}
