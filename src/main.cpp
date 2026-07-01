#include "core/Engine.h"
#include "core/AppConfig.h"
#include "QuantumAtomVersion.h"
#include <iostream>
#include <chrono>
#include <string>

int main() {
    try {
        AppConfig config = AppConfig::loadDefaultLocations();
        const std::string title = std::string("QuantumAtom ") + QUANTUMATOM_VERSION_STRING +
                                  " - Hydrogen-like Orbital Visualizer";
        Engine engine(config.windowWidth, config.windowHeight, title, config);

        auto lastTime = std::chrono::high_resolution_clock::now();

        // Main application loop: continues until the window is closed
        while (!glfwWindowShouldClose(engine.window)) {
            // Compute time elapsed since the last frame for smooth animations
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            // Process operating system events and user input
            glfwPollEvents();

            // Update simulation logic and physics
            engine.updatePhysics(deltaTime);

            // Render the 3D scene and the ImGui overlay
            engine.drawScene(static_cast<float>(glfwGetTime()), deltaTime);

            // Display the rendered frame by swapping buffers
            glfwSwapBuffers(engine.window);
        }
    } catch (const std::exception &e) {
        // Log any unhandled exceptions to the standard error stream
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
