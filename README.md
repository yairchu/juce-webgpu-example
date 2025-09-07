# JUCE WebGPU Example

This is a JUCE application that demonstrates WebGPU graphics rendering using [WebGPU-distribution](https://github.com/eliemichel/WebGPU-distribution)'s RAII wrappers. The application provides a GUI interface to render graphics using WebGPU and display them in real-time.

## Features

- JUCE GUI application framework
- WebGPU graphics rendering integration
- Real-time rendering at ~60 FPS
- Cross-platform compatibility (macOS, Windows, Linux)
- Colorful animated triangle example

## Building

This project uses CMake and CPM (CMake Package Manager) to automatically download and configure dependencies:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Usage

1. Build and run the application
2. Wait for WebGPU initialization to complete
3. The application will automatically start rendering a colorful triangle
4. The triangle is rendered using WebGPU vertex and fragment shaders
5. The rendered image is displayed in the JUCE GUI in real-time

The graphics renderer creates a triangle with vertices colored red, green, and blue, demonstrating the basic WebGPU graphics pipeline within a JUCE application.
