/**
 * @file HowItWorks.cpp
 * @brief Human-readable architecture guide for QuantumAtom.
 *
 * This file is intentionally documentation-heavy. It exists so a new reader can
 * open one file and understand the job of every major source file, class,
 * function group, shader, and data structure in the project.
 *
 * Nothing in here is part of the runtime path. Think of it as the project's
 * annotated map.
 */

/*
================================================================================
QUANTUMATOM IN ONE SENTENCE
================================================================================

QuantumAtom is a real-time C++/OpenGL program that:

1. Lets the user choose hydrogen-like quantum numbers (n, l, m).
2. Computes the probability density |psi_nlm|^2 for that orbital.
3. Samples many 3D points from that density in a background thread.
4. Uploads those points to the GPU.
5. Draws them as an interactive, colored, animated point cloud.

The app is split into a few layers:

- main.cpp: starts the program and owns the frame loop.
- Engine: owns the window, OpenGL objects, simulation jobs, UI, and rendering.
- QuantumSimulation: pure math for orbital probability density.
- Camera: smooth orbit/pan/zoom camera math.
- AppConfig: load/save user defaults from config/QuantumAtom.ini.
- ShaderLoader: read, compile, and link GLSL shaders.
- QuantumTypes: shared enums and simple data structs.
- shaders/: GPU programs for drawing the cloud and axes.

When in doubt, follow this flow:

main.cpp
  -> Engine constructor
      -> create window and OpenGL context
      -> compile shaders
      -> create ImGui
      -> start regenerateCloud()
  -> while window is open
      -> poll input
      -> update animation
      -> drawScene()
          -> accept completed background cloud if one exists
          -> update camera matrices
          -> draw axes
          -> draw orbital cloud
          -> draw electron tracker
          -> draw ImGui controls
          -> save screenshot if requested
*/

/*
================================================================================
SOURCE TREE GUIDE
================================================================================

ROOT FILES
----------

CMakeLists.txt
    Build definition for the whole project.

    Main jobs:
    - declares the project version;
    - sets C++23;
    - defines compile-time defaults like QUANTUMATOM_MAX_N and
      QUANTUMATOM_DEFAULT_POINTS;
    - finds or fetches dependencies;
    - builds the executable;
    - copies shaders and config next to the executable;
    - defines install and CPack package rules.

    Important options:
    - QUANTUMATOM_MAX_N: maximum supported principal quantum number.
    - QUANTUMATOM_DEFAULT_POINTS: default point-cloud budget.
    - QUANTUMATOM_WITH_DEBUG_SYMBOLS: keeps useful symbols in release builds.
    - QUANTUMATOM_FETCH_DEPS: allows FetchContent dependency fallback.

README.md
    User-facing project overview, build instructions, controls, performance
    notes, and release/package information.

CONTRIBUTING.md
    Contributor checklist and development expectations.

config/QuantumAtom.ini
    Runtime defaults. The app loads this on startup and writes current UI
    settings back on shutdown.

    Examples:
    - pointCount: number of cloud samples to generate.
    - pointSize: OpenGL point sprite size.
    - clipEnabled / clipMode / clipPlane: clipping defaults.
    - renderMode / colorMap / theme: UI-selected visual defaults.

cmake/QuantumAtomVersion.h.in
    Template used by CMake to generate QuantumAtomVersion.h in the build
    directory. The app uses that generated header for version/about text.


ENTRY POINT
-----------

src/main.cpp
    The executable starts here.

    What it does:
    - loads AppConfig from config/QuantumAtom.ini;
    - builds the window title using QUANTUMATOM_VERSION_STRING;
    - constructs Engine;
    - runs the main loop:
        1. calculate deltaTime;
        2. glfwPollEvents();
        3. engine.updatePhysics(deltaTime);
        4. engine.drawScene(glfwGetTime(), deltaTime);
        5. glfwSwapBuffers(engine.window);
    - catches fatal exceptions and prints a useful error.

    main.cpp should stay simple. Most app behavior belongs in Engine.


CORE
----

include/core/Engine.h
src/core/Engine.cpp
    Engine is the coordinator. It is the largest class because it owns the
    application lifecycle.

    Engine responsibilities:
    - GLFW window creation and callbacks;
    - OpenGL initialization;
    - shader program setup;
    - GPU buffer and VAO ownership;
    - background orbital cloud generation;
    - cloud cache management;
    - camera update/render matrix usage;
    - Dear ImGui UI;
    - screenshot export;
    - reset-to-launch behavior.

    Important Engine state:
    - state:
        Current QuantumState (n, l, m).

    - cloudPoints:
        Active point cloud currently drawn by the renderer.

    - m_pendingCloud:
        Cloud produced by the background thread. drawScene() moves this into
        cloudPoints when ready.

    - m_buildThread:
        Background worker that computes point clouds without freezing the UI.

    - m_buildCancelled:
        Atomic flag used to stop an old cloud job when the user changes state.

    - m_buildProgress / m_buildStage:
        Progress display for the UI.

    - m_cloudCache:
        Small cache keyed by (n, l, m, pointCount). Returning to a recent
        orbital can skip regeneration.

    - m_launchConfig / m_launchState:
        Snapshot of the settings at app launch. resetSimulation() uses these so
        R/Space can restore the same state the executable started with.

    Engine initialization functions:

    - initGlfwWindow()
        Starts GLFW, requests an OpenGL 3.3 core context, creates the window,
        enables vsync, and stores the Engine pointer in the GLFW window.

    - initOpenGL()
        Loads OpenGL functions through GLAD, checks OpenGL 3.3 availability,
        sets global GL state, compiles shaders, and creates static geometry.

    - initShaders()
        Builds shader programs and caches uniform locations.

    - initStaticGeometry()
        Creates VAOs/VBOs for the colored XYZ axes and the white electron
        tracker point.

    - initImGui()
        Creates the Dear ImGui context and connects the GLFW/OpenGL3 backends.

    - setupCallbacks()
        Registers keyboard, mouse, scroll, char, and resize callbacks. These
        callbacks update camera targets, toggle clipping, trigger reset, and
        forward input to ImGui.

    Cloud generation:

    - regenerateCloud()
        Starts or restarts background point-cloud generation.

        High-level algorithm:
        1. Clamp quantum numbers to valid ranges.
        2. Cancel and join any previous build thread.
        3. Check the cache for the same (state, pointCount).
        4. If no cache hit, start a new worker thread.
        5. Worker evaluates a radial/theta density grid.
        6. Worker builds a cumulative distribution function (CDF).
        7. Worker samples points from the CDF and random phi angles.
        8. Worker publishes a preview cloud first when the target is expensive.
        9. Worker publishes the final cloud and marks it ready.

        The important idea is that the UI thread never sits there computing
        hundreds of thousands of orbital samples. It just checks whether the
        worker has new data ready.

    - previewPointCount()
        Chooses a smaller preview budget for high-cost states. This makes n=7
        and n=8 feel responsive sooner.

    - cacheCloud() / findCachedCloud()
        Store and retrieve recent final clouds. Cache entries avoid recomputing
        when the user returns to the same orbital and point budget.

    Rendering:

    - drawScene()
        One-frame render coordinator.

        It:
        1. swaps in a completed pending cloud;
        2. clears the framebuffer;
        3. updates the camera;
        4. draws axes;
        5. draws the point cloud;
        6. draws the electron tracker;
        7. draws the UI;
        8. saves a screenshot if requested.

    - drawCloud()
        Uploads cloud data to GPU buffers when dirty, then draws GL_POINTS with
        cloud.vert.glsl and cloud.frag.glsl.

        GPU attributes:
        - location 0: position xyz
        - location 1: normalized density
        - location 2: per-point angular velocity factor

        Uniforms control:
        - camera matrix;
        - time and animation speed;
        - magnetic quantum number m;
        - color intensity;
        - clipping mode;
        - density threshold;
        - render mode;
        - colormap;
        - point size;
        - iso-shell parameters;
        - tint color.

    - drawAxes()
        Draws RGB world axes using axes.vert.glsl / axes.frag.glsl.

    - drawActiveElectron()
        Draws a small white analytical tracker point. This is not an exact
        quantum electron path. It is a visual orientation aid.

    UI:

    - renderUI()
        Builds the ImGui interface.

        Main tabs:
        - Quantum Numbers:
            n, l, m sliders and orbital presets.

        - Rendering:
            render mode, colormap, point count, density threshold, point size,
            color intensity, animation speed, clipping controls, axes/tracker
            toggles, colors, theme.

        - Camera:
            yaw, pitch, distance, smoothing, reset view.

        - Export:
            screenshot button and cloud-cache controls.

        - Info:
            plain-language orbital explanation and project metadata.

    Reset:

    - resetSimulation()
        Cancels current generation, restores launch state/config, resets camera
        and UI toggles, clears cloud/cache state, and starts fresh generation.

    Screenshots:

    - requestScreenshot()
        Sets a flag.

    - saveScreenshotPng()
        Reads the framebuffer, vertically flips it, and writes PNG output using
        stb_image_write.


include/core/AppConfig.h
src/core/AppConfig.cpp
    AppConfig stores runtime defaults.

    What it owns:
    - window size;
    - point count;
    - density threshold;
    - point size;
    - color intensity;
    - animation speed;
    - clipping settings;
    - vsync;
    - render mode;
    - colormap;
    - UI theme;
    - point/background colors;
    - path to the loaded config file.

    loadDefaultLocations()
        Looks for QuantumAtom.ini in common runtime locations:
        - config/QuantumAtom.ini
        - ../config/QuantumAtom.ini
        - ../../config/QuantumAtom.ini
        - QuantumAtom.ini

    save()
        Writes the current settings back to the config file on shutdown.

    The config format is deliberately simple INI-style key=value text. That is
    enough for app defaults and avoids adding a heavier dependency.
*/

/*
================================================================================
MATH AND SIMULATION
================================================================================

include/math/QuantumSimulation.h
src/QuantumSimulation.cpp
    QuantumSimulation is the pure math layer.

    It does not know about:
    - OpenGL;
    - GLFW;
    - ImGui;
    - threads;
    - cameras;
    - files.

    It only answers this question:

        "Given r, theta, phi and quantum numbers (n, l, m), what is the
         probability density at that location?"

    Main function:

    - computeProbability(r, theta, phi, state)
        Validates the state and coordinates, evaluates the radial part and the
        angular part, and returns radialProbability * angularProbability.

    Helper functions:

    - associatedLegendre(l, m, x)
        Computes the associated Legendre polynomial used by the angular part of
        the spherical harmonic.

    - associatedLaguerre(k, alpha, x)
        Computes the associated Laguerre polynomial used by the radial part.

    - sphericalHarmonicProbability(l, m, theta)
        Computes |Y_l^m(theta, phi)|^2 for the stationary state. Since this is
        probability magnitude, the phi phase factor drops out.

    Numerical stability:
    - lgammaf is used for factorial-like normalization terms.
    - inputs are clamped where appropriate.
    - invalid or non-finite values return zero probability.

    Physical model:
    - This is a hydrogen-like one-electron analytical orbital.
    - It is not a many-electron atom or chemistry solver.
    - The Bohr radius is scaled for visualization so the cloud fits nicely in
      the scene.
*/

/*
================================================================================
SHARED TYPES
================================================================================

include/utils/QuantumTypes.h
    This file contains simple data types shared across the project.

    Enums:

    - RenderMode
        Controls how the point cloud is shaded:
        - DensityPoints: solid depth-tested density cloud.
        - GlowBillboards: additive glowing points.
        - IsoShell: keeps points near a normalized density band.
        - PhaseFlow: animated phase-like color/size variation.
        - HaloFog: larger soft points for a fog-like effect.

    - ColorMap
        Shader-side color ramps:
        - Inferno
        - Viridis
        - Plasma
        - Magma
        - Cividis

    - UiTheme
        Dear ImGui theme choice:
        - Dark
        - Classic
        - Light

    - ClipMode
        Cloud clipping behavior:
        - XPlane: hide points with x greater than clipPlane.
        - PositiveXY: hide points where x > 0 and y > 0.
        - PositiveXYZ: hide points where x > 0, y > 0, and z > 0.

    Structs:

    - CloudPoint
        One sampled point in the orbital cloud.

        Fields:
        - pos: 3D position.
        - vel: reserved for possible future simulation behavior.
        - brightness: raw probability density at this point.
        - omega: per-point animation speed factor.

    - QuantumState
        The current orbital quantum numbers.

        Fields:
        - n: principal quantum number.
        - l: azimuthal quantum number.
        - m: magnetic quantum number.

        Rules:
        - 1 <= n <= QUANTUMATOM_MAX_N
        - 0 <= l < n
        - -l <= m <= l

    - OrbitalPreset
        Label plus quantum numbers for quick UI buttons like 1s, 2p, 3d, 4f.

    Constants:
    - kMinimumPointCount
    - kMaximumPointCount
    - kOrbitalPresets
*/

/*
================================================================================
CAMERA
================================================================================

include/utils/Camera.h
src/Camera.cpp
    Camera manages the user's view of the 3D scene.

    Public state:
    - yaw / pitch / distance:
        Current camera orientation and distance.

    - targetYaw / targetPitch / targetDistance:
        Desired camera values. update() smoothly moves current values toward
        these targets.

    - targetPos / destinationTargetPos:
        Current and desired look-at point.

    Important functions:

    - update(width, height, deltaTime)
        Applies exponential smoothing to yaw, pitch, distance, and target
        position. The camera does not directly write OpenGL matrices anymore;
        it only updates its own state.

    - pan(deltaX, deltaY)
        Moves the look-at target in the camera's local right/up plane.

    - getViewMatrix()
        Builds the view matrix using glm::lookAt.

    - getProjectionMatrix(width, height)
        Builds a perspective projection matrix.

    - getViewProjectionMatrix(width, height)
        Returns projection * view. This matrix is uploaded to shaders.

    Input mapping lives in Engine callbacks:
    - left mouse drag: rotate
    - right mouse drag or Shift + left mouse drag: pan
    - scroll wheel: zoom
*/

/*
================================================================================
OPENGL LOADING AND SHADERS
================================================================================

include/utils/OpenGLLoader.h
    Small compatibility wrapper around GLAD.

    Why it exists:
    - The project can use the local generated GLAD 1 loader.
    - It can also support a generated/fallback loader path.
    - Engine only calls loadOpenGLFunctions() and does not care which loader
      implementation is underneath.


include/utils/ShaderLoader.h
src/ShaderLoader.cpp
    Utility namespace for shader file loading and GLSL program creation.

    Functions:

    - loadTextFile(relativePath)
        Searches common shader locations and returns the file contents.

    - compile(type, source, debugName)
        Compiles one shader and throws an exception with the GLSL compiler log
        if compilation fails.

    - createProgramFromFiles(vertexPath, fragmentPath)
        Loads, compiles, links, and validates a vertex/fragment shader pair.

    Shader errors should fail loudly. A black screen with no explanation is the
    enemy of graphics debugging.


shaders/cloud.vert.glsl
    Vertex shader for every orbital point.

    Input attributes:
    - aPos: point position.
    - aNorm: normalized density.
    - aOmega: per-point animation factor.

    Main work:
    - optionally rotates point positions for phase-flow style animation;
    - computes heat/phase values sent to the fragment shader;
    - applies clipping rules;
    - calculates perspective-scaled gl_PointSize based on camera depth and render mode;
    - transforms the point by uViewProjection.

    Clipping is done here so discarded points never reach expensive fragment
    shading.


shaders/cloud.frag.glsl
    Fragment shader for point sprites.

    Main work:
    - discards clipped or low-density points;
    - makes each point circular using gl_PointCoord;
    - applies the selected colormap;
    - changes alpha/color behavior based on RenderMode;
    - implements iso-shell, glow, phase-flow, and halo looks.


shaders/axes.vert.glsl
shaders/axes.frag.glsl
    Simple shaders for colored axes and the electron tracker point.
*/

/*
================================================================================
DATA FLOW: FROM UI TO PIXELS
================================================================================

This is the most useful mental model for changing the project.

1. The user changes n/l/m or point count in ImGui.

2. Engine::renderUI() notices the edit is complete.

3. Engine::regenerateCloud() starts a new background job.

4. The background job captures the current QuantumState and point count. It
   does not keep reading mutable UI state while running.

5. The job evaluates QuantumSimulation::computeProbability() many times to
   build a radial/theta probability grid.

6. The grid is converted into a CDF so the code can sample important regions
   more directly than blind rejection sampling.

7. The job creates CloudPoint entries:
   - spherical sample -> cartesian position;
   - raw density -> brightness;
   - random animation factor -> omega.

8. The job publishes the vector into m_pendingCloud under m_swapMutex and sets
   m_cloudReady.

9. Engine::drawScene() sees m_cloudReady, moves m_pendingCloud into cloudPoints,
   and marks GPU buffers dirty.

10. Engine::drawCloud() rebuilds VBO data from cloudPoints:
    - positions buffer;
    - normalized density buffer;
    - omega buffer.

11. drawCloud() binds shaders, uploads uniforms, and calls glDrawArrays.

12. cloud.vert.glsl and cloud.frag.glsl turn each CloudPoint into pixels.
*/

/*
================================================================================
THREADING RULES
================================================================================

The code uses one background build thread for cloud generation.

Rules to protect yourself:

1. Do not call OpenGL from the build thread.
   OpenGL context ownership is on the main/UI thread.

2. The worker should capture a copy of user settings it needs.
   Do not constantly read mutable Engine fields from the worker.

3. Use m_buildCancelled to stop old work.
   When the user changes orbital state, old work becomes irrelevant.

4. Use m_swapMutex only for the final handoff.
   Holding the mutex while doing heavy math would freeze the main thread.

5. m_cloudReady is the handoff signal.
   The main thread checks it once per frame.

6. Join old threads before starting new ones.
   This prevents multiple workers fighting over Engine state.
*/

/*
================================================================================
CONFIGURATION FLOW
================================================================================

Startup:
    main.cpp
      -> AppConfig::loadDefaultLocations()
      -> Engine(..., config)
      -> Engine::applyRuntimeConfig(config)

Shutdown:
    Engine::~Engine()
      -> copies current UI/runtime values into m_config
      -> m_config.save()

Reset:
    Engine::resetSimulation()
      -> restores m_launchConfig and m_launchState
      -> clears cloud/cache/pending state
      -> starts generation again

This means:
- closing the app saves current preferences;
- launching again uses those saved preferences;
- pressing R/Space restores whatever the app launched with.
*/

/*
================================================================================
ADDING NEW FEATURES SAFELY
================================================================================

Add a new render mode:
    1. Add an enum value to RenderMode in QuantumTypes.h.
    2. Add a label in renderModeName() in Engine.cpp.
    3. Extend the combo loop if needed.
    4. Pass any new uniforms in Engine::drawCloud().
    5. Implement visual behavior in cloud.vert.glsl or cloud.frag.glsl.

Add a new colormap:
    1. Add an enum value to ColorMap.
    2. Add a label in colorMapName().
    3. Add a branch in cloud.frag.glsl colormap().

Add a new clipping style:
    1. Add an enum value to ClipMode.
    2. Add a label in clipModeName().
    3. Add shader logic in cloud.vert.glsl.
    4. Save/load it in AppConfig if it should persist.

Add a new config field:
    1. Add a field to AppConfig.
    2. Parse it in AppConfig.cpp fromIni().
    3. Write it in AppConfig::save().
    4. Apply it in Engine::applyRuntimeConfig().
    5. Copy current value back in Engine::~Engine().

Add a new orbital/atom model:
    1. Keep QuantumSimulation pure.
    2. Add a new simulation abstraction instead of mixing new physics into UI.
    3. Make Engine select which model to call.

Improve performance:
    - Reduce point count.
    - Reduce density grid resolution.
    - Keep expensive work off the main thread.
    - Avoid rebuilding VBOs unless cloud data changed.
    - Prefer shader uniforms for visual changes that do not require new points.
*/

/*
================================================================================
COMMON DEBUGGING NOTES
================================================================================

Black screen:
    - Check shader compile/link exceptions.
    - Confirm shaders were copied beside the executable.
    - Confirm OpenGL 3.3 is available.

UI works but no cloud:
    - Check build progress in HUD.
    - Confirm point count is not zero.
    - Disable density threshold.
    - Disable clipping.
    - Try n=1, l=0, m=0.

Cloud is slow:
    - Lower point count.
    - Use DensityPoints instead of HaloFog or GlowBillboards.
    - Lower point size.
    - Wait for the final cloud to cache, then revisit the same state.

New quantum state looks invalid:
    - QuantumState::isValid() enforces n/l/m rules.
    - When n changes, Engine clamps l and m into valid ranges.

Shader uniform seems ignored:
    - Confirm the uniform name exactly matches in C++ and GLSL.
    - If a uniform is optimized away by GLSL, glGetUniformLocation may return
      -1. That is normal for unused uniforms.
*/

/**
 * @brief Documentation anchor.
 *
 * This function is intentionally empty. The value of this file is the guide
 * above. Keeping a tiny function here makes the file valid C++ if someone
 * chooses to compile it in a documentation or teaching build.
 */
void HowItWorks() {
    // Read the comments above. They are the implementation guide.
}
