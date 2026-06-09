#include "Engine.h"
#include <iostream>
#include <chrono>

/**
 * @brief Entry point for the QuantumAtom simulation
 * 
 * This project visualizes the probability density of electrons in hydrogen-like atoms.
 * Refactored for better performance and readability.
 * Enjoy the quantum beauty!
 */
int main() {
    try {
        Engine engine(1280, 720, "QuantumAtom - Hydrogen-like Orbital Visualizer");

        auto lastTime = std::chrono::high_resolution_clock::now();

        while (!glfwWindowShouldClose(engine.window)) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            glfwPollEvents();
            engine.updatePhysics(deltaTime);
            engine.drawScene(static_cast<float>(glfwGetTime()), deltaTime);
            glfwSwapBuffers(engine.window);
        }
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
