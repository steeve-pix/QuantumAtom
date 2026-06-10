#include "Camera.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <algorithm>

void Camera::update(int width, int height, float deltaTime) {
    /* 
     * Exponential decay for smooth interpolation.
     * This creates the "weighted" feel where the camera slows down as it approaches its target.
     */
    float blend = 1.0f - std::exp(-smoothness * deltaTime);

    // Update current values towards the targets
    yaw += (targetYaw - yaw) * blend;
    pitch += (targetPitch - pitch) * blend;
    distance += (targetDistance - distance) * blend;
    targetPos += (destinationTargetPos - targetPos) * blend;

    /*
     * Convert spherical coordinates (distance, yaw, pitch) to Cartesian (x, y, z).
     * This defines the camera's relative position to the target.
     */
    glm::vec3 offset(
        distance * std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)),
        distance * std::sin(glm::radians(pitch)),
        distance * std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch))
    );

    glm::vec3 camPos = targetPos + offset;

    /*
     * Build the View and Projection matrices.
     * View: Transforms world coordinates to camera space.
     * Projection: Handles perspective (objects getting smaller in the distance).
     */
    glm::mat4 view = glm::lookAt(camPos, targetPos, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height),
                                      0.1f, 6000.0f);

    // Apply matrices to the OpenGL pipeline
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(proj));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));
}

void Camera::pan(float deltaX, float deltaY) {
    float radYaw = glm::radians(yaw);
    float radPitch = glm::radians(pitch);

    // Identify the forward vector based on current rotation
    glm::vec3 forward(
        -std::cos(radYaw) * std::cos(radPitch),
        -std::sin(radPitch),
        -std::sin(radYaw) * std::cos(radPitch)
    );
    forward = glm::normalize(forward);

    // Calculate 'right' and 'up' vectors using cross products
    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
    glm::vec3 up = glm::cross(right, forward);

    // Scale panning speed based on distance to keep movement intuitive
    float factor = distance * 0.0012f;

    // Shift the target position in the camera's local plane
    destinationTargetPos += right * (-deltaX * factor) + up * (-deltaY * factor);
}

glm::mat4 Camera::getViewMatrix() const {
    glm::vec3 offset(
        distance * std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)),
        distance * std::sin(glm::radians(pitch)),
        distance * std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch))
    );
    glm::vec3 camPos = targetPos + offset;
    return glm::lookAt(camPos, targetPos, glm::vec3(0.0f, 1.0f, 0.0f));
}
