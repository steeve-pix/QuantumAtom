# ⚛️ QuantumAtom: Hydrogen-like Orbital Visualizer

![GitHub license](https://img.shields.io/github/license/steeve-pix/Atom)
![C++](https://img.shields.io/badge/language-C%2B%2B17-blue.svg)
![OpenGL](https://img.shields.io/badge/Graphics-OpenGL%203.3-orange.svg)

**QuantumAtom** is a high-performance, real-time 3D visualization engine for hydrogen-like atomic orbitals. It utilizes modern OpenGL and multithreaded computation to render electron probability densities (wavefunctions) through interactive point clouds.

---

## Features

- **Realistic Quantum Simulation**: Computes probability densities using Associated Laguerre and Legendre polynomials.
- **Multithreaded Engine**: Background cloud generation ensures smooth UI performance even with 250,000+ points.
- **Interactive UI**: Real-time adjustment of quantum numbers ($n$, $l$, $m$) and visualization parameters via ImGui.
- **Dynamic Heatmaps**: Visualizes density gradients using high-contrast "fire" heatmaps.
- **Cross-Section View**: Integrated clipping planes to inspect the internal structure of complex orbitals.
- **Smooth Navigation**: Full 3D camera control with orbital rotation and zoom.

---

## Getting Started

### Prerequisites

Ensure you have the following installed:
- **CMake** (3.20 or higher)
- **C++23 Compiler** (MSVC, GCC, or Clang)
- **OpenGL 3.3+** compatible drivers

### Installation

1. **Clone the repository**:
   ```bash
   git clone https://github.com/steeve-pix/Atom.git
   cd Atom
   ```

2. **Configure and Build**:
   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build . --config Release
   ```

3. **Run the executable**:
   ```bash
   ./QuantumAtom
   ```

---

## Controls & Usage

| Input | Action |
| :--- | :--- |
| **Mouse Left Click** | Rotate Camera |
| **Mouse Right Click** | Zoom In/Out |
| **ImGui Panel** | Adjust $n$, $l$, $m$ Quantum Numbers |
| **Space Bar** | Reset Simulation/Camera |
| **C Key** | Toggle Clipping Plane |

---

## How It Works

The simulation solves the time-independent Schrödinger equation for a hydrogen-like atom. The wavefunction $\psi_{nlm}(r, \theta, \phi)$ is decomposed into:
1. **Radial Part**: $R_{nl}(r)$ using Associated Laguerre polynomials.
2. **Angular Part**: $Y_{lm}(\theta, \phi)$ using Spherical Harmonics (Associated Legendre polynomials).

The density $\rho = |\psi|^2$ is sampled using a rejection sampling algorithm to generate a representative point cloud.

---

## License

This project is licensed under the MIT License – see the [LICENSE](LICENSE) file for details.

---

<p align="center"> Developed with ❤️ by <a href="https://github.com/steeve-pix">Steeve</a> </p>
