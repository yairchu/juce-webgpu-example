#pragma once

#include "OpenGLWebGPUComponent.h"
#include "WebGPUGraphics.h"
#include <juce_gui_basics/juce_gui_basics.h>

class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void renderGraphics();

    std::shared_ptr<WebGPUGraphics> webgpuGraphics;

    juce::Label statusLabel;

    // Traditional CPU-based rendering
    juce::Image renderedImage;

    // New OpenGL-based rendering (when available)
    std::unique_ptr<OpenGLWebGPUComponent> openglComponent;

    // Flag to control which rendering method to use
    bool useOpenGLRendering = true; // Try OpenGL first, fallback to CPU if needed

    bool isInitialized = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
