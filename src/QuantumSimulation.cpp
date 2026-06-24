#include "QuantumSimulation.h"
#include <algorithm>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// Computes the Associated Legendre Polynomials for the angular component of the wavefunction
float QuantumSimulation::associatedLegendre(int l, int m, float x) {
    int absM = std::abs(m);
    if (absM > l) return 0.0f;

    float pmm = 1.0f;
    if (absM > 0) {
        float somx2 = sqrtf((1.0f - x) * (1.0f + x));
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
        auto f_ll = static_cast<float>(ll);
        pll = (x * (2.0f * f_ll - 1.0f) * pmmp1 - (f_ll + static_cast<float>(absM) - 1.0f) * pmm) / (
                  f_ll - static_cast<float>(absM));
        pmm = pmmp1;
        pmmp1 = pll;
    }
    return pmmp1;
}

// Computes the Associated Laguerre Polynomials for the radial component of the wavefunction
float QuantumSimulation::associatedLaguerre(int k, int alpha, float x) {
    auto f_alpha = static_cast<float>(alpha);
    if (k == 0) return 1.0f;
    if (k == 1) return 1.0f + f_alpha - x;

    float L0 = 1.0f;
    float L1 = 1.0f + f_alpha - x;
    float L2 = 0.0f;

    for (int j = 2; j <= k; j++) {
        auto f_j = static_cast<float>(j);
        L2 = ((2.0f * f_j - 1.0f + f_alpha - x) * L1 - (f_j - 1.0f + f_alpha) * L0) / f_j;
        L0 = L1;
        L1 = L2;
    }
    return L2;
}

// Computes the spherical harmonic component, defining the 3D shape of the orbital
float QuantumSimulation::sphericalHarmonic(int l, int m, float theta, float phi) {
    int absM = abs(m);
    float Plm = associatedLegendre(l, absM, cosf(theta));

    // Factorial normalization using log-gamma for numerical stability
    float logNumFact = lgammaf(static_cast<float>(l - absM + 1));
    float logDenFact = lgammaf(static_cast<float>(l + absM + 1));

    float logNormSq = logf(2.0f * static_cast<float>(l) + 1.0f) - logf(4.0f * PI) + logNumFact - logDenFact;
    float norm = expf(0.5f * logNormSq);

    // Real-valued spherical harmonics for visualization
    if (m > 0) {
        return sqrtf(2.0f) * norm * Plm * cosf(static_cast<float>(m) * phi);
    } else if (m < 0) {
        return sqrtf(2.0f) * norm * Plm * sinf(static_cast<float>(absM) * phi);
    }

    return norm * Plm;
}

// Entry point for probability density calculations based on quantum numbers
float QuantumSimulation::computeProbability(float r, float theta, float phi, const QuantumState &state) {
    float a0 = 4.0f; // Scaled Bohr radius
    auto n_f = static_cast<float>(state.n);
    float rho = (2.0f * r) / (static_cast<float>(state.n) * a0);

    int k = state.n - state.l - 1;
    int alpha = 2 * state.l + 1;

    // Normalize the radial component
    float logCubeScale = 3.0f * logf(2.0f / (n_f * a0));
    float logTopFact = lgammaf(static_cast<float>(state.n - state.l));
    float logBottomFact = lgammaf(static_cast<float>(state.n + state.l + 1));

    float logRadNormSq = logCubeScale - logf(2.0f * n_f) + logTopFact - logBottomFact;
    float radNorm = expf(0.5f * logRadNormSq);

    // Calculate radial part of the wavefunction
    float radial = radNorm * expf(-rho / 2.0f) * powf(rho, static_cast<float>(state.l)) *
                   associatedLaguerre(k, alpha, rho);

    // Calculate angular part of the wavefunction
    float angular = sphericalHarmonic(state.l, state.m, theta, phi);

    // Total wavefunction (psi) and its square (probability density)
    float psi = radial * angular;
    return psi * psi;
}
