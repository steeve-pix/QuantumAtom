#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>

/**
 * @class Camera
 * @brief Manages the 3D camera system with smooth interpolation.
 */
class Camera {
public:
    /** @brief Current orientation and distance */
    float yaw = -40.0f;
    float pitch = 25.0f;
    float distance = 380.0f;
    glm::vec3 targetPos = glm::vec3(0.0f);

    /** @brief Desired orientation and distance for interpolation */
    float targetYaw = -40.0f;
    float targetPitch = 25.0f;
    float targetDistance = 380.0f;
    glm::vec3 destinationTargetPos = glm::vec3(0.0f);

    float smoothness = 2.5f; /**< Controls how fast the camera reaches its target */

    /**
     * @brief Updates camera state and applies smoothing.
     * @param width Viewport width
     * @param height Viewport height
     * @param deltaTime Time elapsed since last frame
     */
    void update(int width, int height, float deltaTime);

    /**
     * @brief Pans the camera's look-at target.
     * @param deltaX Horizontal movement delta
     * @param deltaY Vertical movement delta
     */
    void pan(float deltaX, float deltaY);

    /**
     * @brief Computes the view matrix.
     * @return 4x4 view transformation matrix
     */
    glm::mat4 getViewMatrix() const;
};

#endif
