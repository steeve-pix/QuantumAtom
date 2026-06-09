#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>

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

    float smoothness = 2.5f;

    // This function calculates how the camera should move and look every frame.
    // It makes sure the movement feels smooth instead of jumping instantly.
    void update(int width, int height, float deltaTime);

    // This function moves the "center" of what the camera is looking at.
    // When you drag with the mouse to slide the view, this handles that math.
    void pan(float deltaX, float deltaY);
};

#endif
