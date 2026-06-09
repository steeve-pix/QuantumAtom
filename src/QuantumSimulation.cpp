#include "QuantumSimulation.h"
#include <cmath>
#include <algorithm>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

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

    float pmmp1 = x * (2.0f * absM + 1.0f) * pmm;
    if (l == absM + 1) return pmmp1;

    float pll = 0.0f;
    for (int ll = absM + 2; ll <= l; ll++) {
        pll = (x * (2.0f * ll - 1.0f) * pmmp1 - (ll + absM - 1.0f) * pmm) / (ll - absM);
        pmm = pmmp1;
        pmmp1 = pll;
    }
    return pmmp1;
}

float QuantumSimulation::associatedLaguerre(int k, int alpha, float x) {
    if (k == 0) return 1.0f;
    if (k == 1) return 1.0f + alpha - x;

    float L0 = 1.0f;
    float L1 = 1.0f + alpha - x;
    float L2 = 0.0f;

    for (int j = 2; j <= k; j++) {
        L2 = ((2 * j - 1 + alpha - x) * L1 - (j - 1 + alpha) * L0) / j;
        L0 = L1;
        L1 = L2;
    }
    return L2;
}

float QuantumSimulation::sphericalHarmonic(int l, int m, float theta, float phi) {
    int absM = std::abs(m);
    float Plm = associatedLegendre(l, absM, std::cos(theta));

    auto factorial = [](int num) {
        float res = 1.0f;
        for (int i = 2; i <= num; ++i) res *= i;
        return res;
    };

    float num = (2 * l + 1) * factorial(l - absM);
    float den = 4.0f * PI * factorial(l + absM);
    float norm = std::sqrt(num / den);

    if (m > 0) {
        return std::sqrt(2.0f) * norm * Plm * std::cos(m * phi);
    } else if (m < 0) {
        return std::sqrt(2.0f) * norm * Plm * std::sin(absM * phi);
    }

    return norm * Plm;
}

float QuantumSimulation::computeProbability(float r, float theta, float phi, const QuantumState &state) {
    float a0 = 4.0f;
    float rho = (2.0f * r) / (state.n * a0);

    int k = state.n - state.l - 1;
    int alpha = 2 * state.l + 1;

    auto factorial = [](int num) {
        float res = 1.0f;
        for (int i = 2; i <= num; ++i) res *= i;
        return res;
    };

    float radNorm = std::sqrt(std::pow(2.0f / (state.n * a0), 3) * factorial(state.n - state.l - 1) /
                              (2.0f * state.n * factorial(state.n + state.l)));

    float radial = radNorm * std::exp(-rho / 2.0f) * std::pow(rho, state.l) * associatedLaguerre(k, alpha, rho);
    float angular = sphericalHarmonic(state.l, state.m, theta, phi);

    float psi = radial * angular;
    return psi * psi;
}
