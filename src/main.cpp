#include "Engine.h"
#include <iostream>
#include <chrono>

// This is the starting point of the whole program.
int main() {
    try {
        // Create the engine which opens the window and sets up the 3D world.
        Engine engine(1280, 720, "QuantumAtom - Hydrogen-like Orbital Visualizer");

        auto lastTime = std::chrono::high_resolution_clock::now();

        // Keep the program running until the user closes the window.
        while (!glfwWindowShouldClose(engine.window)) {
            // Measure how much time has passed since the last frame.
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            // Check if the user pressed any keys or moved the mouse.
            glfwPollEvents();

            // Move things in the simulation (like the rotating electron).
            engine.updatePhysics(deltaTime);

            // Tell the graphics card to draw the current scene.
            engine.drawScene(static_cast<float>(glfwGetTime()), deltaTime);

            // Show the frame we just drew on the screen.
            glfwSwapBuffers(engine.window);
        }
    } catch (const std::exception &e) {
        // If something goes wrong, print the error message.
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
