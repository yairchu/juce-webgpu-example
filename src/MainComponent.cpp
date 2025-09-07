#include "MainComponent.h"

MainComponent::MainComponent()
{
    // Initialize WebGPU compute
    webgpuCompute = std::make_unique<WebGPUCompute>();
    
    // Setup UI components
    runButton.setButtonText("Run WebGPU Compute");
    runButton.onClick = [this] { runCompute(); };
    addAndMakeVisible(runButton);
    
    statusLabel.setText("Initializing WebGPU...", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);
    
    resultLabel.setText("Result: Not yet computed", juce::dontSendNotification);
    resultLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(resultLabel);
    
    setSize(400, 300);
    
    // Initialize WebGPU on background thread
    std::thread([this]() {
        bool success = webgpuCompute->initialize();
        juce::MessageManager::callAsync([this, success]() {
            if (success)
            {
                statusLabel.setText("WebGPU initialized successfully!", juce::dontSendNotification);
                runButton.setEnabled(true);
            }
            else
            {
                statusLabel.setText("Failed to initialize WebGPU", juce::dontSendNotification);
                runButton.setEnabled(false);
            }
        });
    }).detach();
    
    runButton.setEnabled(false);
}

MainComponent::~MainComponent()
{
    if (webgpuCompute)
        webgpuCompute->shutdown();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("JUCE WebGPU Compute Example", getLocalBounds().removeFromTop(60), 
               juce::Justification::centred, true);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(80); // Space for title
    
    auto buttonArea = bounds.removeFromTop(50);
    runButton.setBounds(buttonArea.reduced(50, 10));
    
    bounds.removeFromTop(20); // Spacing
    
    auto statusArea = bounds.removeFromTop(30);
    statusLabel.setBounds(statusArea);
    
    bounds.removeFromTop(20); // Spacing
    
    auto resultArea = bounds.removeFromTop(30);
    resultLabel.setBounds(resultArea);
}

void MainComponent::runCompute()
{
    if (isComputing || !webgpuCompute || !webgpuCompute->isInitialized())
        return;
    
    isComputing = true;
    runButton.setEnabled(false);
    statusLabel.setText("Running compute shader...", juce::dontSendNotification);
    resultLabel.setText("Result: Computing...", juce::dontSendNotification);
    
    webgpuCompute->runComputeAsync([this](uint32_t result) {
        onComputeResult(result);
    });
}

void MainComponent::onComputeResult(uint32_t result)
{
    isComputing = false;
    runButton.setEnabled(true);
    statusLabel.setText("Compute completed!", juce::dontSendNotification);
    resultLabel.setText("Result: " + juce::String(result), juce::dontSendNotification);
}
