#include "QuantumSimulation.h"
#include <cmath>
#include <algorithm>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// Numerical evaluation of Associated Legendre Polynomials P_l^m(x)
// Uses the standard three-term recurrence relation for stability.
float QuantumSimulation::associatedLegendre(int l, int m, float x) {
    int absM = std::abs(m);
    if (absM > l) return 0.0f;

    // Rule 1: Compute P_m^m(x)
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

    // Rule 2: Compute P_{m+1}^m(x)
    float pmmp1 = x * (2.0f * absM + 1.0f) * pmm;
    if (l == absM + 1) return pmmp1;

    // Rule 3: Compute P_l^m(x) using recurrence for l > m+1
    float pll = 0.0f;
    for (int ll = absM + 2; ll <= l; ll++) {
        pll = (x * (2.0f * ll - 1.0f) * pmmp1 - (ll + absM - 1.0f) * pmm) / (ll - absM);
        pmm = pmmp1;
        pmmp1 = pll;
    }
    return pmmp1;
}

// Numerical evaluation of Associated Laguerre Polynomials L_k^alpha(x)
// These polynomials describe the radial behavior of the wave function.
float QuantumSimulation::associatedLaguerre(int k, int alpha, float x) {
    if (k == 0) return 1.0f;
    if (k == 1) return 1.0f + alpha - x;

    float L0 = 1.0f;
    float L1 = 1.0f + alpha - x;
    float L2 = 0.0f;

    // Recurrence relation: (j)L_j = (2j-1+alpha-x)L_{j-1} - (j-1+alpha)L_{j-2}
    for (int j = 2; j <= k; j++) {
        L2 = ((2.0f * j - 1.0f + alpha - x) * L1 - (j - 1.0f + alpha) * L0) / static_cast<float>(j);
        L0 = L1;
        L1 = L2;
    }
    return L2;
}

// Computes the real part of Spherical Harmonics Y_l^m
// These functions describe the angular distribution of the probability cloud.
float QuantumSimulation::sphericalHarmonic(int l, int m, float theta, float phi) {
    int absM = std::abs(m);
    float Plm = associatedLegendre(l, absM, std::cos(theta));

    // Lambda for factorial computation (used in normalization)
    auto factorial = [](int num) {
        float res = 1.0f;
        for (int i = 2; i <= num; ++i) res *= static_cast<float>(i);
        return res;
    };

    // Calculate normalization constant
    float num = (2.0f * l + 1.0f) * factorial(l - absM);
    float den = 4.0f * PI * factorial(l + absM);
    float norm = std::sqrt(num / den);

    // Apply real spherical harmonic transformation based on sign of m
    if (m > 0) {
        return std::sqrt(2.0f) * norm * Plm * std::cos(static_cast<float>(m) * phi);
    } else if (m < 0) {
        return std::sqrt(2.0f) * norm * Plm * std::sin(static_cast<float>(absM) * phi);
    }

    return norm * Plm; // m == 0 case
}

// Core function to evaluate the probability density |Psi|^2
// Based on the analytical solution for the Hydrogen Atom.
float QuantumSimulation::computeProbability(float r, float theta, float phi, const QuantumState &state) {
    // Bohr radius scale (approximated for visual clarity in simulation units)
    float a0 = 4.0f;
    float rho = (2.0f * r) / (static_cast<float>(state.n) * a0);

    int k = state.n - state.l - 1;   // Degree of Laguerre polynomial
    int alpha = 2 * state.l + 1;    // Parameter of Laguerre polynomial

    auto factorial = [](int num) {
        float res = 1.0f;
        for (int i = 2; i <= num; ++i) res *= static_cast<float>(i);
        return res;
    };

    // Radial normalization constant
    float radNorm = std::sqrt(std::pow(2.0f / (static_cast<float>(state.n) * a0), 3) * factorial(state.n - state.l - 1) /
                              (2.0f * state.n * factorial(state.n + state.l)));

    // Calculate radial part: R(r)
    float radial = radNorm * std::exp(-rho / 2.0f) * std::pow(rho, static_cast<float>(state.l)) * associatedLaguerre(k, alpha, rho);
    
    // Calculate angular part: Y(theta, phi)
    float angular = sphericalHarmonic(state.l, state.m, theta, phi);

    // Final wave function Psi = R * Y
    float psi = radial * angular;
    
    // Probability density is the square of the wave function
    return psi * psi;
}
