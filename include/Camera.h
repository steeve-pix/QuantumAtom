#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>

/**
 * @brief Handles 3D camera logic with smooth transitions and panning
 */
class Camera {
public:
    float yaw = -40.0f;
    float pitch = 25.0f;
    float distance = 380.0f;
    glm::vec3 targetPos = glm::vec3(0.0f);

    float targetYaw = -40.0f;
    float targetPitch = 25.0f;
    float targetDistance = 380.0f;
    glm::vec3 destinationTargetPos = glm::vec3(0.0f);

    float smoothness = 5.0f;

    /**
     * @brief Updates camera position and orientation based on current targets
     */
    void update(int width, int height, float deltaTime);

    /**
     * @brief Pans the camera target based on mouse input
     */
    void pan(float deltaX, float deltaY);
};

#endif // CAMERA_H
