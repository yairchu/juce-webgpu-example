#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "WebGPUCompute.h"

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void runCompute();
    void onComputeResult(uint32_t result);

    std::unique_ptr<WebGPUCompute> webgpuCompute;
    
    juce::TextButton runButton;
    juce::Label statusLabel;
    juce::Label resultLabel;
    
    bool isComputing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
