#version 330 core

// Render mode values:
// 0 = density points, 1 = glow billboards, 2 = iso shell,
// 3 = phase flow, 4 = halo fog.
//
// Clip mode values:
// 0 = adjustable +X plane, 1 = positive XY quadrant, 2 = positive XYZ octant.

layout(location = 0) in vec3 aPos;
layout(location = 1) in float aNorm;
layout(location = 2) in float aOmega;

uniform mat4 uViewProjection;
uniform float uTime;
uniform float uMFloat;
uniform float uColorIntensity;
uniform int uClipEnabled;
uniform float uClipPlane;
uniform int uClipMode;
uniform float uPointSize;
uniform float uAnimationSpeed;
uniform int uRenderMode;

out float vNorm;
out float vDiscard;
out float vHeat;
out float vPhase;

void main() {
    vec3 pos = aPos;
    float mAbs = abs(uMFloat);

    // Phase-flow mode rotates samples around the polar axis. This is not a full
    // time-dependent Schrodinger solve; it is a readable visual cue for m.
    if (mAbs > 0.001 && uAnimationSpeed > 0.001) {
        float speed = aOmega * mAbs * uAnimationSpeed;
        float angle = uTime * speed * sign(uMFloat);
        float s = sin(angle);
        float c = cos(angle);
        pos.xz = vec2(pos.x * c - pos.z * s,
                      pos.x * s + pos.z * c);
    }

    // aNorm is CPU-normalized density. vHeat applies a perceptual curve so dim
    // outer lobes remain visible without blowing out the dense core.
    vNorm = clamp(aNorm, 0.0, 1.0);
    vHeat = clamp(pow(vNorm, 0.45) * clamp(uColorIntensity, 0.1, 10.0) * 0.82, 0.0, 1.0);
    vPhase = sin(uTime * uAnimationSpeed * (0.6 + aOmega * 3.5) + length(aPos) * 0.045 + uMFloat);
    bool clipped = false;
    if (uClipEnabled == 1) {
        if (uClipMode == 0) {
            clipped = pos.x > uClipPlane;
        } else if (uClipMode == 1) {
            clipped = pos.x > 0.0 && pos.y > 0.0;
        } else {
            clipped = pos.x > 0.0 && pos.y > 0.0 && pos.z > 0.0;
        }
    }
    vDiscard = clipped ? 1.0 : 0.0;

    // Point size is scaled per render mode. The fragment shader still masks the
    // sprite into a circle, so larger modes become soft billboards rather than
    // square OpenGL points.
    float modeScale = 1.0;
    if (uRenderMode == 1) modeScale = 1.45;
    if (uRenderMode == 2) modeScale = 0.82;
    if (uRenderMode == 3) modeScale = 1.22 + 0.18 * vPhase;
    if (uRenderMode == 4) modeScale = 2.35;

    gl_PointSize = max(1.0, uPointSize * modeScale);
    gl_Position = uViewProjection * vec4(pos, 1.0);
}
