#include <JuceHeader.h>

class SineAudioSource : public juce::AudioSource
{
public:
    void prepareToPlay(int, double sampleRate) override
    {
        currentSampleRate = sampleRate;
        phase = 0.0;
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        const float freq = frequencyHz.load();
        const double phaseIncrement = freq / currentSampleRate;

        for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
        {
            auto* samples = info.buffer->getWritePointer(ch, info.startSample);
            for (int n = 0; n < info.numSamples; ++n)
            {
                samples[n] = 0.2f * std::sin(phase * juce::MathConstants<double>::twoPi);
                phase += phaseIncrement;
                if (phase >= 1.0) phase -= 1.0;
            }
        }
    }

    void releaseResources() override {}

    std::atomic<float> frequencyHz{440.0f};

private:
    double currentSampleRate = 44100.0;
    double phase = 0.0;
};

class MainComponent : public juce::AudioAppComponent
{
public:
    MainComponent()
    {
        setSize(600, 400);

        addAndMakeVisible(frequencySlider);
        frequencySlider.setRange(20.0, 5000.0, 1.0);
        frequencySlider.setValue(440.0);
        frequencySlider.setSliderStyle(juce::Slider::LinearHorizontal);
        frequencySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 24);
        frequencySlider.onValueChange = [this]
        {
            sineSource.frequencyHz.store((float)frequencySlider.getValue());
        };

        addAndMakeVisible(statusLabel);
        statusLabel.setJustificationType(juce::Justification::centredLeft);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        sineSource.frequencyHz.store((float)frequencySlider.getValue());

        setAudioChannels(0, 2);
    }

    ~MainComponent() override
    {
        shutdownAudio();
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        sineSource.prepareToPlay(samplesPerBlockExpected, sampleRate);

        juce::String deviceName = "none";
        int bufferSize = 0;
        double actualSampleRate = sampleRate;
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            deviceName = device->getName();
            bufferSize = device->getCurrentBufferSizeSamples();
            actualSampleRate = device->getCurrentSampleRate();
        }
        statusLabel.setText("Device: " + deviceName
            + "  |  Buffer: " + juce::String(bufferSize) + " samples"
            + "  |  Rate: " + juce::String(actualSampleRate, 1) + " Hz",
            juce::dontSendNotification);
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        sineSource.getNextAudioBlock(bufferToFill);
    }

    void releaseResources() override
    {
        sineSource.releaseResources();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a1a1a));
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(20.0f));
        g.drawText("Simple DAW — Week 3 (sine source)", getLocalBounds().removeFromTop(40),
                   juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(20);
        area.removeFromTop(40);
        frequencySlider.setBounds(area.removeFromTop(40));
        area.removeFromTop(20);
        statusLabel.setBounds(area.removeFromTop(24));
    }

private:
    SineAudioSource sineSource;
    juce::Slider frequencySlider;
    juce::Label statusLabel;
};

class SimpleDawApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "Simple DAW"; }
    const juce::String getApplicationVersion() override    { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& commandLine) override
    {
        mainWindow.reset(new MainWindow("Simple DAW", new MainComponent()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name, juce::Component* c)
            : juce::DocumentWindow(name,
                                   juce::Desktop::getInstance().getDefaultLookAndFeel()
                                       .findColour(juce::ResizableWindow::backgroundColourId),
                                   juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(c, true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(SimpleDawApplication)
