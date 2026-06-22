#include <JuceHeader.h>
#include "midi/SynthAudioSource.h"
#include "tracks/AudioTrack.h"
#include "tracks/TrackRow.h"
#include "plugin/PluginHost.h"

class MainComponent : public juce::AudioAppComponent,
                      public juce::MidiInputCallback,
                      public juce::Timer
{
public:
    MainComponent()
        : midiKeyboard(synthSource.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        setSize(960, 600);

        addAndMakeVisible(midiKeyboard);
        midiKeyboard.setKeyWidth(28.0f);
        midiKeyboard.setAvailableRange(36, 96);
        midiKeyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xfff0f0f0));
        midiKeyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff202020));
        midiKeyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff404040));
        midiKeyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x805080ff));

        addAndMakeVisible(statusLabel);
        statusLabel.setJustificationType(juce::Justification::centredLeft);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        addAndMakeVisible(addTrackButton);
        addTrackButton.setButtonText("+ Add Track");
        addTrackButton.onClick = [this] { addTrack(); };

        addAndMakeVisible(scanPluginsButton);
        scanPluginsButton.setButtonText("Scan VST3");
        scanPluginsButton.onClick = [this]
        {
            scanPluginsButton.setEnabled(false);
            juce::Timer::callAfterDelay(10, [this]
            {
                pluginHost.scanForPlugins();
                scanPluginsButton.setEnabled(true);
            });
        };

        addAndMakeVisible(tracksViewport);
        tracksViewport.setViewedComponent(&tracksContainer);
        tracksViewport.setScrollBarsShown(true, false);

        addAndMakeVisible(synthGainLabel);
        synthGainLabel.setText("Synth", juce::dontSendNotification);
        synthGainLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        synthGainLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(synthGainSlider);
        synthGainSlider.setRange(0.0, 2.0, 0.01);
        synthGainSlider.setValue(0.6);
        synthGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        synthGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        synthGainSlider.setTextBoxIsEditable(false);
        synthGainSlider.onValueChange = [this]
        {
            synthGain.store((float)synthGainSlider.getValue());
        };

        setAudioChannels(0, 2);
        openAllMidiInputs();

        addAndMakeVisible(masterGainLabel);
        masterGainLabel.setText("Master", juce::dontSendNotification);
        masterGainLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        masterGainLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(masterGainSlider);
        masterGainSlider.setRange(0.0, 2.0, 0.01);
        masterGainSlider.setValue(0.8);
        masterGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        masterGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        masterGainSlider.setTextBoxIsEditable(false);
        masterGainSlider.onValueChange = [this]
        {
            masterGainTarget.store((float)masterGainSlider.getValue());
        };

        addTrack();
        refreshLayout();

        startTimer(500);
    }

    ~MainComponent() override
    {
        stopTimer();
        closeAllMidiInputs();
        shutdownAudio();

        trackRows.clear();
        tracks.clear();
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        synthSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
        for (auto& t : tracks)
            t->prepareToPlay(samplesPerBlockExpected, sampleRate);

        pluginHost.setSampleRate(sampleRate);
        pluginHost.setBlockSize(samplesPerBlockExpected);

        masterGainSmoothed.reset(sampleRate, 0.05);
        masterGainSmoothed.setCurrentAndTargetValue(masterGainTarget.load());

        juce::String deviceName = "none";
        int bufferSize = 0;
        double actualSampleRate = sampleRate;
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            deviceName = device->getName();
            bufferSize = device->getCurrentBufferSizeSamples();
            actualSampleRate = device->getCurrentSampleRate();
        }
        updateStatus(deviceName, bufferSize, actualSampleRate);
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        bool anyTrackSoloed = false;
        for (auto& t : tracks)
            if (t->isSolo()) { anyTrackSoloed = true; break; }

        scratchBuffer.setSize(bufferToFill.buffer->getNumChannels(), bufferToFill.numSamples);
        scratchBuffer.clear();

        juce::AudioSourceChannelInfo scratch(&scratchBuffer, 0, bufferToFill.numSamples);
        synthSource.getNextAudioBlock(scratch);
        const float g = synthGain.load();
        for (int ch = 0; ch < scratchBuffer.getNumChannels(); ++ch)
            scratchBuffer.applyGain(ch, 0, bufferToFill.numSamples, g);

        bufferToFill.buffer->clear();
        for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch)
            bufferToFill.buffer->addFrom(ch, bufferToFill.startSample, scratchBuffer,
                                         ch, 0, bufferToFill.numSamples);

        for (auto& t : tracks)
            t->renderInto(*bufferToFill.buffer, bufferToFill.startSample,
                          bufferToFill.numSamples, anyTrackSoloed);

        masterGainSmoothed.setTargetValue(masterGainTarget.load());
        const float master = masterGainSmoothed.getNextValue();
        for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch)
            bufferToFill.buffer->applyGain(ch, bufferToFill.startSample, bufferToFill.numSamples, master);
    }

    void releaseResources() override
    {
        synthSource.releaseResources();
        for (auto& t : tracks)
            t->releaseResources();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a1a1a));
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(20.0f));
        g.drawText("Simple DAW", getLocalBounds().removeFromTop(40),
                   juce::Justification::centred);
    }

    void resized() override
    {
        refreshLayout();
    }

    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override
    {
        const juce::String src = source != nullptr ? source->getName() : juce::String("unknown");
        juce::Logger::writeToLog("MIDI in: " + src + " :: " + message.getDescription());

        if (message.isNoteOnOrOff())
            synthSource.keyboardState.processNextMidiEvent(message);
    }

    void timerCallback() override
    {
        juce::String deviceName = "none";
        int bufferSize = 0;
        double actualSampleRate = 0.0;
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            deviceName = device->getName();
            bufferSize = device->getCurrentBufferSizeSamples();
            actualSampleRate = device->getCurrentSampleRate();
        }
        updateStatus(deviceName, bufferSize, actualSampleRate);

        for (auto& row : trackRows)
            row->refreshTimeLabel();
    }

private:
    void addTrack()
    {
        auto track = std::make_unique<AudioTrack>();
        track->prepareToPlay(2048, 48000.0);

        auto* trackPtr = track.get();
        auto row = std::make_unique<TrackRow>(*trackPtr, pluginHost,
            [this](TrackRow* r) { removeTrack(r); },
            [this] { refreshLayout(); });

        tracks.push_back(std::move(track));
        trackRows.push_back(std::move(row));
        tracksContainer.addAndMakeVisible(trackRows.back().get());
        refreshLayout();
    }

    void removeTrack(TrackRow* row)
    {
        for (size_t i = 0; i < trackRows.size(); ++i)
        {
            if (trackRows[i].get() == row)
            {
                tracksContainer.removeChildComponent(row);
                trackRows.erase(trackRows.begin() + i);
                tracks.erase(tracks.begin() + i);
                refreshLayout();
                return;
            }
        }
    }

    void refreshLayout()
    {
        auto area = getLocalBounds().reduced(12);
        area.removeFromTop(40);
        area.removeFromTop(8);

        statusLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(6);

        auto topRow = area.removeFromTop(28);
        addTrackButton.setBounds(topRow.removeFromRight(110));
        topRow.removeFromRight(8);
        scanPluginsButton.setBounds(topRow.removeFromRight(90));
        topRow.removeFromRight(16);
        masterGainLabel.setBounds(topRow.removeFromRight(60));
        topRow.removeFromRight(4);
        masterGainSlider.setBounds(topRow.removeFromRight(180));
        area.removeFromTop(6);

        tracksViewport.setBounds(area.removeFromTop(220));
        area.removeFromTop(8);

        if (!trackRows.empty())
        {
            int totalHeight = 0;
            for (size_t i = 0; i < trackRows.size(); ++i)
            {
                const int h = trackRows[i]->getCurrentPreferredHeight();
                trackRows[i]->setBounds(0, totalHeight, tracksViewport.getWidth() - 4, h);
                totalHeight += h;
            }
            tracksContainer.setSize(tracksViewport.getWidth(), std::max(totalHeight, tracksViewport.getHeight()));
        }

        midiKeyboard.setBounds(area.removeFromTop(140));

        auto synthRow = area.removeFromTop(28);
        synthGainLabel.setBounds(synthRow.removeFromLeft(50));
        synthRow.removeFromLeft(8);
        synthGainSlider.setBounds(synthRow);
    }

    void openAllMidiInputs()
    {
        const auto devices = juce::MidiInput::getAvailableDevices();
        juce::Logger::writeToLog("MIDI devices available: " + juce::String((int)devices.size()));
        for (const auto& d : devices)
        {
            auto input = juce::MidiInput::openDevice(d.identifier, this);
            if (input)
            {
                input->start();
                openedInputs.add(input.release());
                juce::Logger::writeToLog("Opened MIDI input: " + d.name);
            }
        }
    }

    void closeAllMidiInputs()
    {
        for (auto* input : openedInputs)
            input->stop();
        openedInputs.clear();
    }

    void updateStatus(const juce::String& deviceName, int bufferSize, double actualSampleRate)
    {
        const int midiCount = (int)openedInputs.size();
        juce::String midiList = "none";
        if (midiCount > 0)
        {
            juce::StringArray names;
            for (auto* in : openedInputs)
                names.add(in->getName());
            midiList = names.joinIntoString(", ");
        }
        juce::String trackStates;
        for (size_t i = 0; i < tracks.size(); ++i)
        {
            if (i > 0) trackStates += "  ";
            const char* state = ".";
            if (tracks[i]->isMuted()) state = "M";
            else if (tracks[i]->getSource().isPlaying()) state = "P";
            trackStates += "T" + juce::String(i + 1) + ":" + state;
        }
        statusLabel.setText("Device: " + deviceName
            + "  |  Buffer: " + juce::String(bufferSize)
            + "  |  Rate: " + juce::String(actualSampleRate, 1) + " Hz"
            + "  |  MIDI (" + juce::String(midiCount) + "): " + midiList
            + "  |  " + trackStates,
            juce::dontSendNotification);
    }

    SynthAudioSource synthSource;
    PluginHost pluginHost;
    std::vector<std::unique_ptr<AudioTrack>> tracks;
    std::vector<std::unique_ptr<TrackRow>> trackRows;

    juce::MidiKeyboardComponent midiKeyboard;
    juce::Component tracksContainer;
    juce::Viewport tracksViewport;
    juce::TextButton addTrackButton;
    juce::TextButton scanPluginsButton;
    juce::Label statusLabel;
    juce::Label synthGainLabel;
    juce::Slider synthGainSlider;
    juce::Label masterGainLabel;
    juce::Slider masterGainSlider;
    juce::OwnedArray<juce::MidiInput> openedInputs;
    juce::AudioBuffer<float> scratchBuffer;
    std::atomic<float> synthGain{0.6f};
    std::atomic<float> masterGainTarget{0.8f};
    juce::SmoothedValue<float> masterGainSmoothed{0.8f};
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
