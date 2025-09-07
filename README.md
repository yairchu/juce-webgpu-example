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

## Usage

1. Build and run the application
2. Wait for WebGPU initialization to complete
3. Click "Run WebGPU Compute" to execute the compute shader
4. The result (1337) will be displayed in the GUI

The compute shader simply writes the value 1337 to a storage buffer, demonstrating the basic WebGPU compute workflow within a JUCE application.
