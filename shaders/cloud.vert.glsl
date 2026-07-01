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

vec3 infernoColor(float x) {
    vec3 c0 = vec3(0.015, 0.000, 0.060);
    vec3 c1 = vec3(0.260, 0.020, 0.520);
    vec3 c2 = vec3(0.640, 0.030, 0.360);
    vec3 c3 = vec3(0.940, 0.210, 0.080);
    vec3 c4 = vec3(1.000, 0.600, 0.050);
    vec3 c5 = vec3(1.000, 0.900, 0.420);

    float scaled = clamp(x, 0.0, 1.0) * 5.0;
    int band = int(min(scaled, 4.0));
    float localT = scaled - float(band);

    if      (band == 0) return mix(c0, c1, localT);
    else if (band == 1) return mix(c1, c2, localT);
    else if (band == 2) return mix(c2, c3, localT);
    else if (band == 3) return mix(c3, c4, localT);
    return mix(c4, c5, localT);
}

void main() {
    vec3 pos = aPos;

    if (abs(uMFloat) > 0.001) {
        float speed = aOmega * abs(uMFloat);
        float angle = uTime * speed * sign(uMFloat);
        float s = sin(angle);
        float c = cos(angle);
        pos.xz = vec2(pos.x * c - pos.z * s,
                      pos.x * s + pos.z * c);
    }

    vNorm = clamp(aNorm, 0.0, 1.0);
    vDiscard = (uClipEnabled == 1 && pos.x > 0.0 && pos.y > 0.0 && pos.z > 0.0) ? 1.0 : 0.0;

    float contrast = clamp(uColorIntensity, 0.2, 3.0);
    float heat = clamp(pow(vNorm, 0.45) * contrast * 0.82, 0.0, 1.0);
    vec3 color = infernoColor(heat);
    float alpha = 0.10 + pow(heat, 0.75) * 0.62;
    if (heat < 0.025) alpha *= heat / 0.025;

    gl_FrontColor = vec4(color, alpha);
    gl_PointSize = 18.0;
    gl_Position = gl_ModelViewProjectionMatrix * vec4(pos, 1.0);
}
