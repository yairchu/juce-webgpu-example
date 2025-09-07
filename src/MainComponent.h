#pragma once

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

    std::unique_ptr<WebGPUGraphics> webgpuGraphics;

    juce::Label statusLabel;
    juce::Image renderedImage;

    bool isInitialized = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
