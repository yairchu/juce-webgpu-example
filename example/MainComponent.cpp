#include "MainComponent.h"

MainComponent::MainComponent()
{
    // Initialize WebGPU graphics
    webgpuGraphics = std::make_shared<WebGPUGraphics>();

    // Setup UI components
    statusLabel.setText ("Initializing WebGPU...", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);

    // Try to create OpenGL component (can be disabled by setting useOpenGLRendering to false)
    if (useOpenGLRendering)
    {
        try
        {
            openglComponent = std::make_unique<OpenGLWebGPUComponent>();
            openglComponent->setWebGPUGraphics (webgpuGraphics);
            addAndMakeVisible (*openglComponent);

            juce::Logger::writeToLog ("Using OpenGL-based WebGPU rendering (GPU-only path)");
        }
        catch (...)
        {
            useOpenGLRendering = false;
            juce::Logger::writeToLog ("OpenGL not available, falling back to CPU-based rendering");
        }
    }
    else
    {
        juce::Logger::writeToLog ("Using CPU-based WebGPU rendering (legacy path)");
    }

    setSize (800, 600);

    // Initialize WebGPU on background thread
    std::thread ([this]()
                 {
        bool success = webgpuGraphics->initialize(getWidth(), getHeight());
        juce::MessageManager::callAsync([this, success]() {
            if (success) {
                statusLabel.setText (useOpenGLRendering ? "WebGPU + OpenGL initialized successfully!" : "WebGPU initialized successfully (CPU fallback)!",
                                     juce::dontSendNotification);
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
    // Stop the timer first to prevent new render calls
    stopTimer();

    // Mark as not initialized to prevent further operations
    isInitialized = false;

    // Shutdown WebGPU before OpenGL context is destroyed
    if (webgpuGraphics)
    {
        webgpuGraphics->shutdown();
        webgpuGraphics.reset(); // Explicitly release the unique_ptr
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    if (useOpenGLRendering && openglComponent)
    {
        // OpenGL component handles its own rendering
        // Just draw status label if needed
    }
    else if (isInitialized && ! renderedImage.isNull())
    {
        // Traditional CPU-based rendering
        g.drawImage (renderedImage, getLocalBounds().removeFromBottom (getHeight() - 30).toFloat());
    }

    if (! isInitialized)
    {
        g.setColour (juce::Colours::white);
        g.setFont (20.0f);
        g.drawText ("JUCE WebGPU Graphics Example", getLocalBounds().removeFromTop (60), juce::Justification::centred, true);
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    // Reserve space for status label at the top
    statusLabel.setBounds (area.removeFromTop (30));

    // Update OpenGL component or WebGPU graphics size
    if (useOpenGLRendering && openglComponent)
    {
        openglComponent->setBounds (area);
    }

    if (webgpuGraphics)
    {
        webgpuGraphics->resize (area.getWidth(), area.getHeight());
    }
}

void MainComponent::timerCallback()
{
    // Check both flags to prevent rendering during shutdown
    if (isInitialized && webgpuGraphics && webgpuGraphics->isInitialized())
    {
        if (! useOpenGLRendering || ! openglComponent)
        {
            // Use traditional CPU-based rendering for fallback
            renderGraphics();
        }
        // OpenGL component handles its own rendering in its render() method
    }
}

void MainComponent::renderGraphics()
{
    // Render frame on background thread to avoid blocking UI
    std::thread ([this]()
                 {
        juce::Image newImage = webgpuGraphics->renderFrameToImage();
        
        juce::MessageManager::callAsync([this, newImage]() {
            renderedImage = newImage;
            repaint();
        }); })
        .detach();
}
