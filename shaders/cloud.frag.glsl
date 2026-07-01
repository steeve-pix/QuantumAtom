#version 120
varying float vNorm;
varying float vDiscard;

void main() {
    if (vDiscard > 0.5) discard;
    if (vNorm < 0.0001) discard;

    vec2  cc     = gl_PointCoord - vec2(0.5);
    float distSq = dot(cc, cc);
    if (distSq > 0.25) discard;

    float a = smoothstep(0.25, 0.0, distSq);
    gl_FragColor = vec4(gl_Color.rgb, gl_Color.a * a);
}
