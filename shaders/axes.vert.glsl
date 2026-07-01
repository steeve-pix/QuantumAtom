#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 uViewProjection;
uniform float uPointSize;

out vec3 vColor;

void main() {
    vColor = aColor;
    gl_PointSize = max(1.0, uPointSize);
    gl_Position = uViewProjection * vec4(aPos, 1.0);
}
