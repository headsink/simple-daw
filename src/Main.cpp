#include <JuceHeader.h>
#include "midi/SynthAudioSource.h"
#include "audio/AudioTrackSource.h"

class MainComponent : public juce::AudioAppComponent,
                      public juce::MidiInputCallback,
                      public juce::Timer
{
public:
    MainComponent()
        : midiKeyboard(synthSource.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        setSize(900, 460);

        addAndMakeVisible(midiKeyboard);
        midiKeyboard.setKeyWidth(28.0f);
        midiKeyboard.setAvailableRange(36, 96);
        midiKeyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xfff0f0f0));
        midiKeyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff202020));
        midiKeyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff404040));
        midiKeyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x805080ff));

        addAndMakeVisible(loadButton);
        loadButton.setButtonText("Load WAV");
        loadButton.onClick = [this] { openWavChooser(); };

        addAndMakeVisible(playButton);
        playButton.setButtonText("Play");
        playButton.onClick = [this] { audioTrack.togglePlay(); updateTransportButtons(); };

        addAndMakeVisible(stopButton);
        stopButton.setButtonText("Stop");
        stopButton.onClick = [this] { audioTrack.stop(); updateTransportButtons(); };

        addAndMakeVisible(statusLabel);
        statusLabel.setJustificationType(juce::Justification::centredLeft);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        addAndMakeVisible(trackLabel);
        trackLabel.setJustificationType(juce::Justification::centredLeft);
        trackLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        trackLabel.setText("Track: (no file loaded)", juce::dontSendNotification);

        setAudioChannels(0, 2);
        openAllMidiInputs();
        updateTransportButtons();

        startTimer(500);
    }

    ~MainComponent() override
    {
        stopTimer();
        closeAllMidiInputs();
        shutdownAudio();
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        synthSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
        audioTrack.prepareToPlay(samplesPerBlockExpected, sampleRate);

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
        juce::AudioBuffer<float> scratchBuffer(bufferToFill.buffer->getNumChannels(),
                                               bufferToFill.numSamples);
        juce::AudioSourceChannelInfo scratch(&scratchBuffer, 0, bufferToFill.numSamples);

        scratchBuffer.clear();
        synthSource.getNextAudioBlock(scratch);
        const float synthGain = 0.6f;

        juce::AudioBuffer<float> trackBuffer(bufferToFill.buffer->getNumChannels(),
                                             bufferToFill.numSamples);
        juce::AudioSourceChannelInfo trackInfo(&trackBuffer, 0, bufferToFill.numSamples);
        audioTrack.getNextAudioBlock(trackInfo);
        const float trackGain = 1.0f;

        bufferToFill.buffer->clear();
        for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch)
        {
            bufferToFill.buffer->addFrom(ch, bufferToFill.startSample, scratchBuffer,
                                         ch, 0, bufferToFill.numSamples, synthGain);
            bufferToFill.buffer->addFrom(ch, bufferToFill.startSample, trackBuffer,
                                         ch, 0, bufferToFill.numSamples, trackGain);
        }
    }

    void releaseResources() override
    {
        synthSource.releaseResources();
        audioTrack.releaseResources();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a1a1a));
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(20.0f));
        g.drawText("Simple DAW — audio track + MIDI synth", getLocalBounds().removeFromTop(40),
                   juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(20);
        area.removeFromTop(40);

        statusLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(8);

        trackLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(6);

        auto buttonRow = area.removeFromTop(36);
        loadButton.setBounds(buttonRow.removeFromLeft(120).reduced(2));
        buttonRow.removeFromLeft(8);
        playButton.setBounds(buttonRow.removeFromLeft(80).reduced(2));
        buttonRow.removeFromLeft(4);
        stopButton.setBounds(buttonRow.removeFromLeft(80).reduced(2));
        area.removeFromTop(8);

        midiKeyboard.setBounds(area.removeFromTop(180));
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
        updateTransportButtons();
    }

private:
    void openWavChooser()
    {
        juce::Logger::writeToLog("Load WAV button clicked");
        auto logFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("simple-daw-click.log");
        logFile.appendText("Load WAV button clicked\n");

        juce::FileChooser chooser("Select a WAV file to play",
                                  juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                  "*.wav;*.aif;*.aiff;*.flac;*.mp3");
        if (chooser.browseForFileToOpen())
        {
            logFile.appendText("FileChooser returned a file\n");
            auto file = chooser.getResult();
            logFile.appendText("Result file: " + file.getFullPathName() + "\n");
            if (file.existsAsFile())
            {
                audioTrack.loadFile(file);
                trackLabel.setText("Track: " + audioTrack.getLoadedFileName(),
                                   juce::dontSendNotification);
                updateTransportButtons();
            }
        }
        else
        {
            logFile.appendText("FileChooser cancelled\n");
        }
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
        {
            input->stop();
            delete input;
        }
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
        statusLabel.setText("Device: " + deviceName
            + "  |  Buffer: " + juce::String(bufferSize)
            + "  |  Rate: " + juce::String(actualSampleRate, 1) + " Hz"
            + "  |  MIDI (" + juce::String(midiCount) + "): " + midiList,
            juce::dontSendNotification);
    }

    void updateTransportButtons()
    {
        const bool loaded = audioTrack.isLoaded();
        loadButton.setEnabled(true);
        playButton.setEnabled(loaded);
        stopButton.setEnabled(loaded);
        playButton.setButtonText(audioTrack.isPlaying() ? "Pause" : "Play");
    }

    SynthAudioSource synthSource;
    AudioTrackSource audioTrack;

    juce::MidiKeyboardComponent midiKeyboard;
    juce::TextButton loadButton;
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::Label statusLabel;
    juce::Label trackLabel;
    juce::OwnedArray<juce::MidiInput> openedInputs;
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
