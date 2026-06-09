#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>

/**
 * @brief Handles 3D camera logic with smooth transitions and panning.
 * 
 * This class manages the orbit camera used to view the atom. It supports
 * smooth interpolation (lerping) between target values for a fluid experience.
 */
class Camera {
public:
    // Current camera state
    float yaw = -40.0f;    ///< Horizontal rotation in degrees
    float pitch = 25.0f;   ///< Vertical rotation in degrees
    float distance = 380.0f; ///< Distance from the focus target
    glm::vec3 targetPos = glm::vec3(0.0f); ///< Point the camera is looking at

    // Target values for smooth interpolation
    float targetYaw = -40.0f;
    float targetPitch = 25.0f;
    float targetDistance = 380.0f;
    glm::vec3 destinationTargetPos = glm::vec3(0.0f);

    float smoothness = 5.0f; ///< Control factor for interpolation speed

    /**
     * @brief Updates camera position and orientation based on current targets.
     * 
     * Applies exponential smoothing to transition current values towards target values.
     * @param width Viewport width
     * @param height Viewport height
     * @param deltaTime Time elapsed since last frame
     */
    void update(int width, int height, float deltaTime);

    /**
     * @brief Pans the camera target based on mouse input.
     * 
     * Moves the focus point (targetPos) along the camera's local right and up axes.
     * @param deltaX Horizontal mouse movement
     * @param deltaY Vertical mouse movement
     */
    void pan(float deltaX, float deltaY);
};

#endif // CAMERA_H
