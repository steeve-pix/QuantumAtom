# QuantumAtom

Real-time C++23 / OpenGL visualizer for hydrogen-like atomic orbitals.

QuantumAtom renders analytical hydrogen-like wavefunctions as interactive 3D probability clouds. It is built with CMake, GLFW, GLAD, GLM, Dear ImGui, multithreaded CPU sampling, and shader-driven point rendering.

Suggested GitHub topics: `cpp23`, `opengl`, `glfw`, `imgui`, `glm`, `cmake`, `scientific-visualization`, `quantum-mechanics`, `hydrogen-atom`, `atomic-orbitals`, `wavefunction`.

## Highlights

- Principal quantum number support up to `n = 8` by default.
- Responsive high-n generation using a background worker, cached finished clouds, and a quick preview pass before full refinement.
- OpenGL 3.3 core renderer with VAOs, GLSL 330 shaders, shader-side clipping, thresholding, colormaps, and phase animation.
- Rendering modes: density points, glowing billboards, iso-density shell, phase flow, and volumetric halo.
- Colormaps: inferno, viridis, plasma, magma, and cividis.
- Dear ImGui tabs for quantum numbers, rendering, camera, export, and physics information.
- Orbital presets from `1s` through high angular-momentum `n=8` states.
- PNG screenshots via `stb_image_write`.
- CMake install and CPack packaging, with GitHub Actions artifacts for Windows, Linux, and macOS.

## Gallery

Place screenshots and GIFs in `docs/media/` before publishing the repository page or a GitHub Release.

Recommended captures:

| File | Suggested content |
| --- | --- |
| `docs/media/1s-density.png` | Ground-state `1s` density points |
| `docs/media/2p-phase.gif` | `2p` phase-flow animation |
| `docs/media/3d-iso.png` | `3d` iso-density shell |
| `docs/media/4f-glow.png` | `4f` glowing billboard mode |
| `docs/media/8k-preview-to-refine.gif` | `n=8` preview/refinement workflow |
| `docs/media/ui-tabs.png` | ImGui tabbed controls |

Use the in-app `Export -> Screenshot PNG` button or press `S`; captures are written to `screenshots/`.

## Controls

| Input | Action |
| --- | --- |
| Left mouse drag | Orbit camera |
| Right mouse drag | Pan camera |
| Shift + left mouse drag | Pan camera |
| Mouse wheel | Zoom |
| Up / Down | Increment or decrement `n` |
| `C` | Toggle clip plane |
| `S` | Save PNG screenshot |
| `R` or Space | Reset simulation and camera |
| ImGui tabs | Change quantum numbers, rendering mode, colormap, point budget, thresholds, camera, export |

## Physics Model

QuantumAtom visualizes a single-electron hydrogen-like orbital:

```text
psi_nlm(r, theta, phi) = R_nl(r) Y_l^m(theta, phi)
rho(r, theta, phi) = |psi_nlm(r, theta, phi)|^2
```

The radial part uses associated Laguerre polynomials. The angular part uses normalized associated Legendre polynomials for the spherical harmonic probability. The renderer samples `rho * r^2` over a radial/theta grid and then samples `phi` uniformly, which is much more predictable for large orbitals than raw rejection sampling.

This is an educational visualization of hydrogen-like analytical orbitals, not a many-electron chemistry solver. The code is structured so future atom models can be added behind the simulation layer.

## Build

### Requirements

- CMake 3.24 or newer
- C++23 compiler
- OpenGL 3.3 capable GPU/driver
- Git for FetchContent dependencies when not using vcpkg

Optional dependency management:

- If you configure with a vcpkg toolchain, CMake first tries `find_package(glfw3 CONFIG)` and `find_package(glm CONFIG)`.
- If packages are not found, CMake falls back to local vendored sources when present, the tracked generated GLAD loader in `third_party/glad`, and FetchContent for GLFW, GLM, Dear ImGui, and stb.

### Configure and compile

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Run from the build directory or from an installed/package directory so `shaders/` and `config/` are beside the executable:

```bash
./build/QuantumAtom
```

On Windows with MSYS2 UCRT:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
cmake -S . -B build -G Ninja `
  -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe `
  -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe `
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Useful CMake options

| Option | Default | Purpose |
| --- | --- | --- |
| `QUANTUMATOM_MAX_N` | `8` | Compile-time maximum principal quantum number |
| `QUANTUMATOM_DEFAULT_POINTS` | `120000` | Initial point-cloud budget |
| `QUANTUMATOM_WITH_DEBUG_SYMBOLS` | `ON` | Emit symbols for Release builds |
| `QUANTUMATOM_FETCH_DEPS` | `ON` | Fetch missing GLFW/stb dependencies |
| `QUANTUMATOM_STATIC_MINGW_RUNTIME` | `ON` | Static MinGW runtime linkage |

Example:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DQUANTUMATOM_MAX_N=8 -DQUANTUMATOM_DEFAULT_POINTS=350000
```

### Install and package

```bash
cmake --install build --config Release --prefix dist/QuantumAtom
cmake --build build --target package --config Release
```

CPack emits `.zip` and `.tar.gz` packages that include the executable, `shaders/`, `config/`, `README.md`, and `LICENSE`.

## Releases

The workflow at `.github/workflows/build.yml` builds Windows, Linux, and macOS Release packages. It uploads artifacts for every push and pull request. Pushing a tag like `v0.3.0` creates a GitHub Release with generated notes and attaches the packaged artifacts.

## Runtime Configuration

Defaults live in `config/QuantumAtom.ini`. The app loads this file from common runtime locations and saves UI defaults on shutdown.

Important fields:

```ini
pointCount=120000
densityThreshold=0.0
pointSize=7.0
clipMode=2
renderMode=0
colorMap=0
backgroundColor=0.035,0.04,0.055
```

## Performance Notes for n=8

High principal quantum numbers are expensive because the spatial extent and number of nodes increase quickly. QuantumAtom keeps the app responsive with:

- background cloud generation;
- multithreaded density-grid evaluation;
- preview cloud generation for `n >= 7`;
- cache reuse for repeated `(n, l, m, pointCount)` states;
- GPU-side clipping, thresholding, colormaps, iso-shell filtering, and phase animation.

For older integrated GPUs, start with `100000` to `200000` points and increase once the target state is cached. The default upper UI budget is `600000` points.

## Repository Layout

```text
include/core/          Engine and runtime configuration
include/math/          Quantum probability calculations
include/utils/         Shared types, camera, shader loading
src/core/              Engine and config implementations
shaders/               GLSL 330 renderer programs
config/                Default runtime settings
samples/               Small educational/sample data files
.github/workflows/     CI/CD packaging workflow
```

## Web Demo Note

A WebGL/WebGPU port is intentionally separate from the native renderer. A future web demo should reuse the same orbital presets and formulas, but generate points in a web worker or compute pass to keep the browser UI responsive.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Useful first contributions include gallery captures, more orbital presets, measured performance profiles for `n=7/8`, and documentation improvements.

## License

QuantumAtom is released under the MIT License. See [LICENSE](LICENSE).
