#include "MainComponent.h"

MainComponent::MainComponent()
{
    // Initialize WebGPU graphics
    webgpuGraphics = std::make_unique<WebGPUGraphics>();

    // Setup UI components
    statusLabel.setText ("Initializing WebGPU...", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);

    setSize (800, 600);

    // Initialize WebGPU on background thread
    std::thread ([this]()
                 {
        bool success = webgpuGraphics->initialize(getWidth(), getHeight());
        juce::MessageManager::callAsync([this, success]() {
            if (success) {
                statusLabel.setText("WebGPU initialized successfully!", juce::dontSendNotification);
                isInitialized = true;
                // Start timer for continuous rendering
                startTimer(16); // ~60 FPS
            } else {
                statusLabel.setText("Failed to initialize WebGPU", juce::dontSendNotification);
            }
        }); })
        .detach();
}

MainComponent::~MainComponent()
{
    stopTimer();
    if (webgpuGraphics)
        webgpuGraphics->shutdown();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    if (isInitialized && ! renderedImage.isNull())
    {
        // Draw the WebGPU rendered image
        g.drawImage (renderedImage, getLocalBounds().toFloat());
    }
    else
    {
        g.setColour (juce::Colours::white);
        g.setFont (20.0f);
        g.drawText ("JUCE WebGPU Graphics Example", getLocalBounds().removeFromTop (60), juce::Justification::centred, true);
    }
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    // Position status label at top
    auto statusArea = bounds.removeFromTop (30);
    statusLabel.setBounds (statusArea);

    // Resize WebGPU graphics if initialized
    if (isInitialized && webgpuGraphics)
    {
        webgpuGraphics->resize (getWidth(), getHeight() - 30); // Account for status label
    }
}

void MainComponent::timerCallback()
{
    if (isInitialized && webgpuGraphics)
    {
        renderGraphics();
    }
}

void MainComponent::renderGraphics()
{
    // Render frame on background thread to avoid blocking UI
    std::thread ([this]()
                 {
        juce::Image newImage = webgpuGraphics->renderFrame();
        
        juce::MessageManager::callAsync([this, newImage]() {
            renderedImage = newImage;
            repaint();
        }); })
        .detach();
}
