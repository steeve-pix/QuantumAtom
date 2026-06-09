#include "Engine.h"
#include <iostream>
#include <chrono>

/**
 * @brief Entry point for the QuantumAtom simulation.
 * 
 * This project visualizes the probability density of electrons in hydrogen-like atoms.
 * It uses a Monte Carlo sampling approach to render the "orbital cloud" based on 
 * analytical solutions to the Schrödinger equation.
 * 
 * Controls:
 * - Left Mouse: Rotate camera
 * - Right Mouse / Shift + Left: Pan camera
 * - Scroll: Zoom in/out
 * - UP/DOWN: Increase/Decrease principal quantum number (n)
 * - R: Reset simulation
 */
int main() {
    try {
        // Initialize the simulation engine with desired resolution and title
        Engine engine(1280, 720, "QuantumAtom - Hydrogen-like Orbital Visualizer");

        auto lastTime = std::chrono::high_resolution_clock::now();

        // Main application loop
        while (!glfwWindowShouldClose(engine.window)) {
            // Calculate delta time for smooth animations and camera movement
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            // 1. Process OS events (input, window resize, etc.)
            glfwPollEvents();
            
            // 2. Update simulation logic
            engine.updatePhysics(deltaTime);
            
            // 3. Render the 3D scene and UI
            engine.drawScene(static_cast<float>(glfwGetTime()), deltaTime);
            
            // 4. Swap front and back buffers to display the frame
            glfwSwapBuffers(engine.window);
        }
    } catch (const std::exception &e) {
        // Log fatal errors before exiting
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
