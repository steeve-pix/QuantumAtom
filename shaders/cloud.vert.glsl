#version 120
attribute vec3  aPos;
attribute float aNorm;
attribute float aOmega;

uniform float uTime;
uniform float uMFloat;
uniform float uColorIntensity;
uniform int   uClipEnabled;

varying float vNorm;
varying float vDiscard;

void main() {
    vec3 pos = aPos;

    if (abs(uMFloat) > 0.001) {
        float angle = uTime * aOmega * uMFloat;
        float s = sin(angle);
        float c = cos(angle);
        float newX = pos.x * c - pos.z * s;
        float newZ = pos.x * s + pos.z * c;
        pos.x = newX;
        pos.z = newZ;
    }

    vDiscard = (uClipEnabled == 1 && pos.x > 0.0 && pos.y > 0.0 && pos.z > 0.0) ? 1.0 : 0.0;
    vNorm    = aNorm;

    float t  = clamp(pow(clamp(aNorm, 0.0, 1.0), 0.50) * uColorIntensity, 0.0, 1.0);
    float s5 = t * 5.0;
    int   ci = int(min(s5, 4.0));
    float lt = s5 - float(ci);

    vec3 c0 = vec3(0.015, 0.000, 0.060);
    vec3 c1 = vec3(0.300, 0.000, 0.650);
    vec3 c2 = vec3(0.800, 0.000, 0.120);
    vec3 c3 = vec3(1.000, 0.420, 0.000);
    vec3 c4 = vec3(1.000, 0.900, 0.000);
    vec3 c5 = vec3(1.000, 1.000, 0.780);

    vec3 col;
    if      (ci == 0) col = mix(c0, c1, lt);
    else if (ci == 1) col = mix(c1, c2, lt);
    else if (ci == 2) col = mix(c2, c3, lt);
    else if (ci == 3) col = mix(c3, c4, lt);
    else              col = mix(c4, c5, lt);

    float alpha = 0.12 + pow(t, 0.80) * 0.58;
    if (t < 0.02) alpha *= (t / 0.02);

    gl_FrontColor = vec4(col, alpha);
    gl_PointSize  = 15.0;
    gl_Position   = gl_ModelViewProjectionMatrix * vec4(pos, 1.0);
}
