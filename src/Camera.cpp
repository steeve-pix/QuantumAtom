#include "Camera.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <algorithm>

void Camera::update(int width, int height, float deltaTime) {
    float blend = 1.0f - std::exp(-smoothness * deltaTime);
    yaw += (targetYaw - yaw) * blend;
    pitch += (targetPitch - pitch) * blend;
    distance += (targetDistance - distance) * blend;
    targetPos += (destinationTargetPos - targetPos) * blend;

    glm::vec3 offset(
        distance * std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch)),
        distance * std::sin(glm::radians(pitch)),
        distance * std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch))
    );
    glm::vec3 camPos = targetPos + offset;

    glm::mat4 view = glm::lookAt(camPos, targetPos, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height),
                                      0.1f, 6000.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(proj));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));
}

void Camera::pan(float deltaX, float deltaY) {
    float radYaw = glm::radians(yaw);
    float radPitch = glm::radians(pitch);

    glm::vec3 forward(
        -std::cos(radYaw) * std::cos(radPitch),
        -std::sin(radPitch),
        -std::sin(radYaw) * std::cos(radPitch)
    );
    forward = glm::normalize(forward);

    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
    glm::vec3 up = glm::cross(right, forward);

    float factor = distance * 0.0012f;
    destinationTargetPos += right * (-deltaX * factor) + up * (-deltaY * factor);
}
