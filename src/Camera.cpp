#include "Camera.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

void Camera::update(int width, int height, float deltaTime) {
    // Apply exponential smoothing for fluid camera movement
    float blend = 1.0f - expf(-smoothness * deltaTime);

    // Interpolate towards target orientation and position
    yaw += (targetYaw - yaw) * blend;
    pitch += (targetPitch - pitch) * blend;
    distance += (targetDistance - distance) * blend;
    targetPos += (destinationTargetPos - targetPos) * blend;

    // Convert spherical coordinates to Cartesian for camera placement
    vec3 offset(
        distance * cosf(radians(yaw)) * cosf(radians(pitch)),
        distance * sinf(radians(pitch)),
        distance * sinf(radians(yaw)) * cosf(radians(pitch))
    );

    vec3 camPos = targetPos + offset;

    // Construct view and projection matrices
    mat4 view = lookAt(camPos, targetPos, vec3(0.0f, 1.0f, 0.0f));
    mat4 proj = perspective(radians(45.0f), static_cast<float>(width) / static_cast<float>(height),
                                      0.1f, 6000.0f);

    // Load matrices into the legacy OpenGL pipeline
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(value_ptr(proj));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(value_ptr(view));
}

void Camera::pan(float deltaX, float deltaY) {
    float radYaw = radians(yaw);
    float radPitch = radians(pitch);

    // Calculate camera direction vectors for local movement
    vec3 forward(
        -cosf(radYaw) * cosf(radPitch),
        -sinf(radPitch),
        -sinf(radYaw) * cosf(radPitch)
    );
    forward = normalize(forward);

    vec3 worldUp(0.0f, 1.0f, 0.0f);
    vec3 right = normalize(cross(forward, worldUp));
    vec3 up = cross(right, forward);

    // Adjust panning speed relative to camera distance
    float factor = distance * 0.0012f;

    // Move the destination target on the plane perpendicular to view direction
    destinationTargetPos += right * (-deltaX * factor) + up * (-deltaY * factor);
}

mat4 Camera::getViewMatrix() const {
    vec3 offset(
        distance * cosf(radians(yaw)) * cosf(radians(pitch)),
        distance * sinf(radians(pitch)),
        distance * sinf(radians(yaw)) * cosf(radians(pitch))
    );
    vec3 camPos = targetPos + offset;
    return lookAt(camPos, targetPos, vec3(0.0f, 1.0f, 0.0f));
}
