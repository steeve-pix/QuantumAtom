# Contributing to QuantumAtom

Thanks for helping improve QuantumAtom. Keep contributions focused on fast, educational scientific visualization.

## Development Setup

1. Configure a Release build:

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   ```

2. Build:

   ```bash
   cmake --build build --config Release --parallel
   ```

3. Run the app and verify at least one low-n and one high-n state, for example `1s`, `3d`, and `8k`.

## Pull Request Checklist

- Keep the MIT license and lightweight dependency philosophy.
- Keep rendering responsive while clouds are regenerating.
- Include shaders/config files in any packaging changes.
- Update README controls or screenshots when UI behavior changes.
- Prefer small, focused changes over broad refactors.
- Mention tested OS, compiler, GPU, and OpenGL version in the PR.

## Code Style

- Use C++23 cleanly, but avoid cleverness where simple code is clearer.
- Keep math/simulation code independent from UI code.
- Keep shader uniforms and C++ enum values synchronized.
- Add comments only when they explain non-obvious math, threading, or graphics state.

## Performance Guidance

For `n=7` and `n=8`, test with at least `250000` points. Watch generation progress, frame rate after upload, and cache-hit behavior when returning to a previous state.

## Documentation and Gallery

Screenshots and GIFs should go in `docs/media/`. Prefer filenames that identify both the orbital and render mode, such as `4f-glow.png` or `8k-preview-to-refine.gif`.
