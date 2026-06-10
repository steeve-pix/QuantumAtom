#include "Camera.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <algorithm>

void Camera::update(int width, int height, float deltaTime) {
    // This blend factor decides how much of the "distance left to travel" 
    // we cover in this frame. It creates that smooth "gliding" effect.
    float blend = 1.0f - std::exp(-smoothness * deltaTime);

    // Move the current values a little bit closer to the target values.
    yaw += (targetYaw - yaw) * blend;
    pitch += (targetPitch - pitch) * blend;
    distance += (targetDistance - distance) * blend;
    targetPos += (destinationTargetPos - targetPos) * blend;

    // Use trigonometry (sin/cos) to figure out where the camera is in 3D space
    // based on the angles (yaw/pitch) and the zoom level (distance).
    glm::vec3 offset(
        distance * std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)),
        distance * std::sin(glm::radians(pitch)),
        distance * std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch))
    );

    // The final camera position is the center point plus the offset we just calculated.
    glm::vec3 camPos = targetPos + offset;

    // These matrices are instructions for the graphics card.
    // 'view' says "where am I looking from?"
    // 'proj' says "how should things look farther away?" (perspective).
    glm::mat4 view = glm::lookAt(camPos, targetPos, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height),
                                      0.1f, 6000.0f);

    // Send these instructions to the graphics card's old pipeline.
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(proj));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));
}

void Camera::pan(float deltaX, float deltaY) {
    float radYaw = glm::radians(yaw);
    float radPitch = glm::radians(pitch);

    // Calculate which way is "forward" from the camera's perspective.
    glm::vec3 forward(
        -std::cos(radYaw) * std::cos(radPitch),
        -std::sin(radPitch),
        -std::sin(radYaw) * std::cos(radPitch)
    );
    forward = glm::normalize(forward);

    // Use the "cross product" to find out which way is "right" and "up" 
    // relative to where the camera is looking.
    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
    glm::vec3 up = glm::cross(right, forward);

    // If we are zoomed out far, we should pan faster.
    float factor = distance * 0.0012f;

    // Move the focal point (what we are looking at) in the direction 
    // the user dragged the mouse.
    destinationTargetPos += right * (-deltaX * factor) + up * (-deltaY * factor);
}
