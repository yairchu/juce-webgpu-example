# Copilot Instructions for JUCE WebGPU Example

## Development Philosophy

### Change Management
- When making changes, unless requested otherwise, go for a small forward step that can be easily reviewed
- This helps make development a conversation and allows for proper direction
- Prefer incremental improvements over large refactors

### Project Goals
- This is an example project, so we optimize for **clarity over performance** even more than usual
- Code should be educational and easy to understand

## Code Style & Formatting

### General C++ Style
- Use **Allman brace style** (braces on new lines)
- **4-space indentation**, no tabs
- C++17 standard
- Left-aligned pointers (`Type* var` not `Type *var`)

### Naming Conventions
- **camelCase** for variables, functions, and methods (`webgpuCompute`, `runCompute`, `onComputeResult`)
- **PascalCase** for classes (`MainComponent`, `TriangleScene`, `WebGPUContext`)
- **SCREAMING_SNAKE_CASE** for constants and macros
- **Private member variables** without special prefixes (just camelCase)

### JUCE Specific Patterns
- Use JUCE's smart pointer conventions (`std::unique_ptr` for ownership)
- Follow JUCE component patterns with `paint()`, `resized()`, constructor setup
- Use `juce::` namespace prefix explicitly
- Use JUCE's `dontSendNotification` for programmatic UI updates
- Use `addAndMakeVisible()` for component setup

### WebGPU Integration Style
- Use **RAII wrappers** from WebGPU-distribution (`wgpu::raii::`)
- Prefer **designated initializers** for WebGPU structs (C++20 style even in C++17)
- Use **raw string literals** for shaders (`R"(shader code)"`)
- Organize WebGPU code into logical methods (`createShaders`, `createVertexBuffer`, `createPipeline`)
- Use anonymous namespaces for shader source and local constants

### Error Handling & Safety
- Use **RAII** for automatic cleanup (prefer over manual resource management)
- Initialize member variables inline where possible (`bool initialized = false`)

### Threading & Async Patterns
- Use lambda callbacks for async operations
- Always check thread safety when updating UI

### Code Organization
- **Header files**: Keep minimal, focus on public interface
- **Implementation files**: Group related functionality into private methods
- Use **anonymous namespaces** for file-local constants and helpers
- Include necessary headers explicitly, don't rely on transitive includes

### Comments & Documentation
- Use `//` for comments
- Prefer self-documenting code over excessive comments

### CMake Style
- Use **CPM** for dependency management
- Group related CMake commands logically
- Use descriptive variable names

## Project-Specific Patterns

### Memory Management
- Prefer RAII and smart pointers over manual resource management
- Use WebGPU RAII wrappers to avoid manual cleanup
- Let C++ destructors handle cleanup automatically
- Use `std::make_unique` for factory creation

## Code Quality Preferences
- **Favor clarity over cleverness**
- **Logical grouping** of related operations
