#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>

/*
 * The Camera class manages the user's perspective in the 3D scene.
 * It uses target-based interpolation to provide smooth, cinematic movement.
 */
class Camera {
public:
    // Current state (updated every frame)
    float yaw = -40.0f;
    float pitch = 25.0f;
    float distance = 380.0f;
    glm::vec3 targetPos = glm::vec3(0.0f);

    // Target state (where we want to be)
    float targetYaw = -40.0f;
    float targetPitch = 25.0f;
    float targetDistance = 380.0f;
    glm::vec3 destinationTargetPos = glm::vec3(0.0f);

    float smoothness = 2.5f;

    /* 
     * Updates the camera position and orientation based on the elapsed time.
     * Calculates the view and projection matrices and sends them to the GPU.
     */
    void update(int width, int height, float deltaTime);

    /* 
     * Pans the camera target based on mouse movement.
     */
    void pan(float deltaX, float deltaY);

    /*
     * Returns the view matrix for the current camera position and orientation.
     */
    glm::mat4 getViewMatrix() const;
};

#endif
