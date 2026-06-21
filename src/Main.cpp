#include <JuceHeader.h>

class SineVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<juce::SynthesiserSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        currentAngle = 0.0;
        level = velocity * 0.15;
        const double cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        const double cyclesPerSample = cyclesPerSecond / getSampleRate();
        angleDelta = cyclesPerSample * juce::MathConstants<double>::twoPi;
    }

    void stopNote(float, bool allowTailOff) override
    {
        level = 0.0;
        clearCurrentNote();
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (level <= 0.0)
            return;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float currentSample = (float)(level * std::sin(currentAngle));
            for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
                outputBuffer.addSample(ch, startSample + sample, currentSample);

            currentAngle += angleDelta;
        }
    }

private:
    double currentAngle = 0.0;
    double angleDelta = 0.0;
    double level = 0.0;
};

class DemoSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override        { return true; }
    bool appliesToChannel(int) override     { return true; }
};

class SynthAudioSource : public juce::AudioSource
{
public:
    SynthAudioSource()
    {
        for (int i = 0; i < 8; ++i)
            synth.addVoice(new SineVoice());

        synth.clearSounds();
        synth.addSound(new DemoSound());
    }

    void prepareToPlay(int, double sampleRate) override
    {
        synth.setCurrentPlaybackSampleRate(sampleRate);
    }

    void releaseResources() override {}

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        info.buffer->clear();
        juce::MidiBuffer midi;
        keyboardState.processNextMidiBuffer(midi, info.startSample, info.numSamples, true);
        synth.renderNextBlock(*info.buffer, midi, info.startSample, info.numSamples);
    }

    juce::MidiKeyboardState keyboardState;

private:
    juce::Synthesiser synth;
};

class AudioTrackSource : public juce::AudioSource
{
public:
    void prepareToPlay(int, double) override {}
    void releaseResources() override {}

    void loadFile(const juce::File& file)
    {
        juce::AudioFormatManager mgr;
        mgr.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(mgr.createReaderFor(file));
        if (reader == nullptr)
        {
            fileBuffer.setSize(0, 0);
            loadedFileName = "(failed to load)";
            return;
        }

        fileBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
        reader->read(&fileBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
        playPosition = 0;
        playing = false;
        loadedFileName = file.getFileName() + "  (" + juce::String(reader->numChannels)
            + " ch, " + juce::String(reader->lengthInSamples) + " samples, "
            + juce::String((int)reader->sampleRate) + " Hz)";
    }

    void setPlaying(bool shouldPlay) { playing = shouldPlay; }
    void stop() { playing = false; playPosition = 0; }
    void togglePlay() { playing = !playing; }
    void seekToStart() { playPosition = 0; }

    bool isLoaded() const { return fileBuffer.getNumSamples() > 0; }
    const juce::String& getLoadedFileName() const { return loadedFileName; }
    bool isPlaying() const { return playing; }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        info.buffer->clear();
        if (!playing || fileBuffer.getNumSamples() == 0)
            return;

        const int numSamples = info.numSamples;
        const int remaining = fileBuffer.getNumSamples() - playPosition;
        if (remaining <= 0)
        {
            playing = false;
            playPosition = 0;
            return;
        }

        const int toCopy = std::min(numSamples, remaining);
        for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
        {
            const int srcCh = ch % fileBuffer.getNumChannels();
            info.buffer->copyFrom(ch, info.startSample, fileBuffer, srcCh,
                                  playPosition, toCopy);
        }
        playPosition += toCopy;
    }

private:
    juce::AudioBuffer<float> fileBuffer;
    int playPosition = 0;
    bool playing = false;
    juce::String loadedFileName;
};

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
        juce::FileChooser chooser("Select a WAV file to play",
                                  juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                  "*.wav;*.aif;*.aiff;*.flac;*.mp3");
        const int flags = juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectFiles;
        chooser.launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                audioTrack.loadFile(file);
                trackLabel.setText("Track: " + audioTrack.getLoadedFileName(),
                                   juce::dontSendNotification);
                updateTransportButtons();
            }
        });
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
