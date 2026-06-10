#include "Engine.h"
#include <iostream>
#include <chrono>

int main() {
    try {
        /* 
         * First, we initialize the Engine. 
         * This sets up our window, the graphics context, and our simulation state.
         */
        Engine engine(1280, 720, "QuantumAtom - Hydrogen-like Orbital Visualizer");

        auto lastTime = std::chrono::high_resolution_clock::now();

        /*
         * This is the "Game Loop". 
         * It runs continuously, updating and rendering until the user decides to quit.
         */
        while (!glfwWindowShouldClose(engine.window)) {
            // We calculate 'deltaTime' to ensure smooth movement regardless of frame rate.
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            // Handle user inputs (mouse, keyboard).
            glfwPollEvents();

            // Update the internal physics of our world.
            engine.updatePhysics(deltaTime);

            // Draw the atom cloud and the user interface.
            engine.drawScene(static_cast<float>(glfwGetTime()), deltaTime);

            // Swap the back buffer to the front so we see the new frame.
            glfwSwapBuffers(engine.window);
        }
    } catch (const std::exception &e) {
        /*
         * If the program crashes or fails to start, 
         * we catch the error here and let the user know what happened.
         */
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
