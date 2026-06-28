#include "utils/Camera.h"
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
    glm::vec3 offset(
        distance * cosf(glm::radians(yaw)) * cosf(glm::radians(pitch)),
        distance * sinf(glm::radians(pitch)),
        distance * sinf(glm::radians(yaw)) * cosf(glm::radians(pitch))
    );

    glm::vec3 camPos = targetPos + offset;

    // Construct view and projection matrices
    glm::mat4 view = glm::lookAt(camPos, targetPos, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height),
                                      0.1f, 6000.0f);

    // Load matrices into the legacy OpenGL pipeline
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(proj));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));
}

void Camera::pan(float deltaX, float deltaY) {
    float radYaw = glm::radians(yaw);
    float radPitch = glm::radians(pitch);

    // Calculate camera direction vectors for local movement
    glm::vec3 forward(
        -cosf(radYaw) * cosf(radPitch),
        -sinf(radPitch),
        -sinf(radYaw) * cosf(radPitch)
    );
    forward = normalize(forward);

    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = normalize(cross(forward, worldUp));
    glm::vec3 up = cross(right, forward);

    // Adjust panning speed relative to camera distance
    float factor = distance * 0.0012f;

    // Move the destination target on the plane perpendicular to view direction
    destinationTargetPos += right * (-deltaX * factor) + up * (-deltaY * factor);
}

glm::mat4 Camera::getViewMatrix() const {
    glm::vec3 offset(
        distance * cosf(glm::radians(yaw)) * cosf(glm::radians(pitch)),
        distance * sinf(glm::radians(pitch)),
        distance * sinf(glm::radians(yaw)) * cosf(glm::radians(pitch))
    );
    glm::vec3 camPos = targetPos + offset;
    return glm::lookAt(camPos, targetPos, glm::vec3(0.0f, 1.0f, 0.0f));
}
