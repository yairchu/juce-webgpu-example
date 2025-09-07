# JUCE WebGPU Example

This is a JUCE application that demonstrates WebGPU compute shader integration using [WebGPU-distribution](https://github.com/eliemichel/WebGPU-distribution)'s RAII wrappers. The application provides a GUI interface to run WebGPU compute shaders and display the results.

## Features

- JUCE GUI application framework
- WebGPU compute shader integration
- Asynchronous compute execution
- Cross-platform compatibility (macOS, Windows, Linux)

## Building

This project uses CMake and CPM (CMake Package Manager) to automatically download and configure dependencies:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Dependencies

The following dependencies are automatically downloaded and configured:

- [JUCE](https://github.com/juce-framework/JUCE) - Audio application framework
- [WebGPU-distribution](https://github.com/eliemichel/WebGPU-distribution) - WebGPU bindings

## Project Structure

```text
├── CMakeLists.txt          # Build configuration
├── src/
│   ├── main.cpp           # JUCE application entry point
│   ├── MainComponent.h    # Main GUI component header
│   ├── MainComponent.cpp  # Main GUI component implementation
│   ├── WebGPUCompute.h    # WebGPU compute wrapper header
│   └── WebGPUCompute.cpp  # WebGPU compute wrapper implementation
└── shaders/
    └── comp.wgsl          # WebGPU compute shader
```

## Usage

1. Build and run the application
2. Wait for WebGPU initialization to complete
3. Click "Run WebGPU Compute" to execute the compute shader
4. The result (1337) will be displayed in the GUI

The compute shader simply writes the value 1337 to a storage buffer, demonstrating the basic WebGPU compute workflow within a JUCE application.
