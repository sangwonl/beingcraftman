# OpenGL output directory

Set every OpenGL executable target's `RUNTIME_OUTPUT_DIRECTORY` to its current binary directory, matching the existing OpenCV pattern. This places targets at `build/opengl/<target>` so the existing VS Code launch path works. Verification: build `32_cubemaps` and confirm that path exists.
