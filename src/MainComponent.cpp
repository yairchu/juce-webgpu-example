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
        bool success = webgpuGraphics->initialize(400, 300);
        juce::MessageManager::callAsync([this, success]() {
            if (success)
            {
                statusLabel.setText("WebGPU initialized! Rendering triangle...", juce::dontSendNotification);
                isInitialized = true;
                startTimer(16); // ~60 FPS
            }
            else
            {
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

    g.setColour (juce::Colours::white);
    g.setFont (20.0f);
    g.drawText ("JUCE WebGPU Graphics Example", getLocalBounds().removeFromTop (60), juce::Justification::centred, true);

    // Draw the WebGPU rendered image if available
    if (renderedImage.isValid())
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop (100); // Space for title and status
        bounds.removeFromBottom (50); // Space for status

        // Center the image
        auto imageArea = bounds.withSizeKeepingCentre (
            juce::jmin (bounds.getWidth(), renderedImage.getWidth()),
            juce::jmin (bounds.getHeight(), renderedImage.getHeight()));

        g.drawImage (renderedImage, imageArea.toFloat());
    }
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (80); // Space for title

    auto statusArea = bounds.removeFromBottom (30);
    statusLabel.setBounds (statusArea);

    // Update graphics renderer size if needed
    if (isInitialized && webgpuGraphics)
    {
        auto renderArea = bounds.reduced (50);
        if (renderArea.getWidth() > 0 && renderArea.getHeight() > 0)
        {
            webgpuGraphics->resize (renderArea.getWidth(), renderArea.getHeight());
        }
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
    // Render on background thread to avoid blocking UI
    std::thread ([this]()
                 {
        if (webgpuGraphics && webgpuGraphics->isInitialized())
        {
            auto newImage = webgpuGraphics->renderFrame();
            
            juce::MessageManager::callAsync([this, newImage]() {
                if (newImage.isValid())
                {
                    renderedImage = newImage;
                    repaint();
                }
            });
        } })
        .detach();
}
