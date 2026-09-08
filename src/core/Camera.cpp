#include "utils/Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

void Camera::update(int width, int height, float deltaTime) {
    // Exponential smoothing is frame-rate independent: the same smoothness value
    // feels similar at 30 FPS and 144 FPS.
    float blend = 1.0f - expf(-smoothness * deltaTime);

    // Interpolate towards target orientation and position.
    yaw += (targetYaw - yaw) * blend;
    pitch += (targetPitch - pitch) * blend;
    distance += (targetDistance - distance) * blend;
    targetPos += (destinationTargetPos - targetPos) * blend;

    (void) width;
    (void) height;
}

void Camera::pan(float deltaX, float deltaY) {
    float radYaw = glm::radians(yaw);
    float radPitch = glm::radians(pitch);

    // Derive the camera's local right/up vectors so panning follows the current
    // view instead of world axes.
    glm::vec3 forward(
        -cosf(radYaw) * cosf(radPitch),
        -sinf(radPitch),
        -sinf(radYaw) * cosf(radPitch)
    );
    forward = normalize(forward);

    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = normalize(cross(forward, worldUp));
    glm::vec3 up = cross(right, forward);

    // Adjust panning speed relative to camera distance.
    float factor = distance * 0.0012f;

    // Move the destination target on the plane perpendicular to view direction.
    destinationTargetPos += right * (-deltaX * factor) + up * (-deltaY * factor);
}

glm::mat4 Camera::getViewMatrix() const {
    // Orbit camera: position is computed from yaw/pitch/distance, then it looks
    // back at targetPos.
    glm::vec3 offset(
        distance * cosf(glm::radians(yaw)) * cosf(glm::radians(pitch)),
        distance * sinf(glm::radians(pitch)),
        distance * sinf(glm::radians(yaw)) * cosf(glm::radians(pitch))
    );
    glm::vec3 camPos = targetPos + offset;
    return glm::lookAt(camPos, targetPos, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::getProjectionMatrix(int width, int height) const {
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    // The far plane is intentionally large enough for n=8 orbitals.
    return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 6000.0f);
}

glm::mat4 Camera::getViewProjectionMatrix(int width, int height) const {
    return getProjectionMatrix(width, height) * getViewMatrix();
}
