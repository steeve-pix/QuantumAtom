#version 330 core

in float vNorm;
in float vDiscard;
in float vHeat;
in float vPhase;

uniform int uRenderMode;
uniform int uColorMap;
uniform float uDensityThreshold;
uniform float uIsoLevel;
uniform float uIsoWidth;
uniform vec3 uTintColor;

out vec4 FragColor;

// Six anchor colors keep the shader lightweight while still approximating
// familiar scientific colormaps.
vec3 ramp6(float x, vec3 c0, vec3 c1, vec3 c2, vec3 c3, vec3 c4, vec3 c5) {
    float scaled = clamp(x, 0.0, 1.0) * 5.0;
    int band = int(min(scaled, 4.0));
    float t = scaled - float(band);
    if (band == 0) return mix(c0, c1, t);
    if (band == 1) return mix(c1, c2, t);
    if (band == 2) return mix(c2, c3, t);
    if (band == 3) return mix(c3, c4, t);
    return mix(c4, c5, t);
}

vec3 colormap(float x) {
    // These branch constants match ColorMap in include/utils/QuantumTypes.h.
    if (uColorMap == 1) {
        return ramp6(x,
            vec3(0.267, 0.005, 0.329),
            vec3(0.283, 0.141, 0.458),
            vec3(0.254, 0.265, 0.530),
            vec3(0.207, 0.372, 0.553),
            vec3(0.164, 0.471, 0.558),
            vec3(0.993, 0.906, 0.144));
    }
    if (uColorMap == 2) {
        return ramp6(x,
            vec3(0.050, 0.030, 0.528),
            vec3(0.326, 0.006, 0.639),
            vec3(0.568, 0.055, 0.640),
            vec3(0.798, 0.280, 0.470),
            vec3(0.955, 0.533, 0.286),
            vec3(0.940, 0.975, 0.131));
    }
    if (uColorMap == 3) {
        return ramp6(x,
            vec3(0.001, 0.000, 0.014),
            vec3(0.141, 0.063, 0.251),
            vec3(0.361, 0.071, 0.431),
            vec3(0.641, 0.122, 0.384),
            vec3(0.908, 0.353, 0.224),
            vec3(0.987, 0.991, 0.749));
    }
    if (uColorMap == 4) {
        return ramp6(x,
            vec3(0.000, 0.135, 0.304),
            vec3(0.153, 0.247, 0.450),
            vec3(0.310, 0.357, 0.531),
            vec3(0.488, 0.485, 0.537),
            vec3(0.706, 0.666, 0.458),
            vec3(0.996, 0.909, 0.218));
    }

    return ramp6(x,
        vec3(0.015, 0.000, 0.060),
        vec3(0.260, 0.020, 0.520),
        vec3(0.640, 0.030, 0.360),
        vec3(0.940, 0.210, 0.080),
        vec3(1.000, 0.600, 0.050),
        vec3(1.000, 0.900, 0.420));
}

void main() {
    // Vertex shader clipping and UI density thresholding both happen before any
    // point-sprite shaping so invisible samples cost as little as possible.
    if (vDiscard > 0.5) discard;
    if (vNorm < max(0.0001, uDensityThreshold)) discard;

    // Iso-shell mode keeps a thin normalized-density band, giving a simple
    // surface-like view without building a marching-cubes mesh.
    if (uRenderMode == 2) {
        float band = abs(vNorm - uIsoLevel);
        if (band > uIsoWidth) discard;
    }

    vec2 cc = gl_PointCoord - vec2(0.5);
    float distSq = dot(cc, cc);
    if (distSq > 0.25) discard;

    // gl_PointCoord is square by default; this radial mask turns every sample
    // into a soft circular sprite.
    float core = smoothstep(0.25, 0.0, distSq);
    float heat = vHeat;
    if (uRenderMode == 3) {
        heat = clamp(heat * (0.72 + 0.34 * vPhase), 0.0, 1.0);
    }

    vec3 color = colormap(heat) * uTintColor;
    float alpha = 0.10 + pow(heat, 0.75) * 0.62;

    if (uRenderMode == 1) {
        float glow = smoothstep(0.25, 0.0, distSq);
        alpha = (0.055 + heat * 0.28) * glow;
        color += colormap(min(1.0, heat + 0.22)) * 0.35;
    } else if (uRenderMode == 2) {
        float edge = 1.0 - smoothstep(0.0, uIsoWidth, abs(vNorm - uIsoLevel));
        alpha = (0.18 + edge * 0.76) * core;
    } else if (uRenderMode == 3) {
        alpha = (0.10 + heat * 0.58) * core;
        color = mix(color, vec3(0.82, 0.95, 1.0) * uTintColor, clamp(vPhase * 0.5 + 0.5, 0.0, 1.0) * 0.35);
    } else if (uRenderMode == 4) {
        float fog = exp(-distSq * 8.5);
        alpha = (0.025 + heat * 0.11) * fog;
        color = mix(color, vec3(0.55, 0.78, 1.0) * uTintColor, 0.28);
    } else {
        alpha *= core;
    }

    if (heat < 0.025) alpha *= heat / 0.025;
    FragColor = vec4(color, clamp(alpha, 0.0, 1.0));
}
