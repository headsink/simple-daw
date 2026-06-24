#include <JuceHeader.h>
#include "midi/SynthAudioSource.h"
#include "midi/MidiTrack.h"
#include "midi/MidiOutputRouter.h"
#include "audio/Recorder.h"
#include "tracks/TracksViewport.h"
#include "plugin/PluginHost.h"
#include "ui/PianoRollComponent.h"
#include "ui/SettingsWindow.h"
#include "ui/TransportBar.h"
#include "session/SessionIO.h"

class MainComponent : public juce::AudioAppComponent,
                      public juce::MidiInputCallback,
                      public juce::Timer,
                      public juce::DragAndDropContainer
{
public:
    MainComponent()
        : midiKeyboard(synthSource.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
          transportBar(masterPeak,
                       [this] { tracksViewport.addTrack(); },
                       [this] { onScanPlugins(); },
                       [this] { onSettings(); },
                       [this] { sessionIO.saveSession(); },
                       [this] { sessionIO.loadSession(); },
                       [this] { sessionIO.toggleRecording(); },
                       [this](const juce::String& name) { onMidiOutputChanged(name); },
                       [this](float v) { masterGainTarget.store(v); }),
          tracksViewport(deviceManager, pluginHost,
                         [this](TrackRow* r) { tracksViewport.removeTrack(r); },
                         [this] { refreshLayout(); }),
          sessionIO(deviceManager, recorder, midiOutputRouter, midiTrack,
                    tracksViewport, pluginHost, transportBar,
                    [this] { return (float) synthGainSlider.getValue(); },
                    [this] { return (float) midiGainSlider.getValue(); },
                    [this] { return bpmSlider.getValue(); },
                    [this](float v, juce::NotificationType nt) { synthGainSlider.setValue((double) v, nt); },
                    [this](float v, juce::NotificationType nt) { midiGainSlider.setValue((double) v, nt); },
                    [this](double v, juce::NotificationType nt) { bpmSlider.setValue(v, nt); },
                    [this](const juce::String& name) { onMidiOutputRestored(name); },
                    [this] { refreshLayout(); })
    {
        setSize(960, 600);

        addAndMakeVisible(midiKeyboard);
        midiKeyboard.setKeyWidth(28.0f);
        midiKeyboard.setAvailableRange(36, 96);
        midiKeyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xfff0f0f0));
        midiKeyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff202020));
        midiKeyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff404040));
        midiKeyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x805080ff));

        addAndMakeVisible(transportBar);

        addAndMakeVisible(seqPlayButton);
        seqPlayButton.setButtonText("Seq Play");
        seqPlayButton.onClick = [this]
        {
            midiTrack.play();
            updateSeqButtons();
        };

        addAndMakeVisible(seqStopButton);
        seqStopButton.setButtonText("Seq Stop");
        seqStopButton.onClick = [this]
        {
            midiTrack.stop();
            updateSeqButtons();
        };

        addAndMakeVisible(bpmLabel);
        bpmLabel.setText("BPM", juce::dontSendNotification);
        bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        bpmLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(bpmSlider);
        bpmSlider.setRange(40.0, 240.0, 0.5);
        bpmSlider.setValue(120.0);
        bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        bpmSlider.setTextBoxIsEditable(false);
        bpmSlider.onValueChange = [this]
        {
            midiTrack.setBpm(bpmSlider.getValue());
        };

        addAndMakeVisible(seqLoopButton);
        seqLoopButton.setButtonText("Loop");
        seqLoopButton.setClickingTogglesState(true);
        seqLoopButton.setToggleState(true, juce::dontSendNotification);
        seqLoopButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff308030));
        seqLoopButton.onClick = [this]
        {
            midiTrack.setLooping(seqLoopButton.getToggleState());
        };

        addAndMakeVisible(pianoRollButton);
        pianoRollButton.setButtonText("Piano Roll");
        pianoRollButton.onClick = [this]
        {
            if (pianoRollWindow == nullptr)
            {
                auto aliveFlag = alive;
                pianoRollWindow = new PianoRollWindow(midiTrack, synthSource.keyboardState,
                    [this, aliveFlag] { if (*aliveFlag) pianoRollWindow = nullptr; });
            }
            else
            {
                pianoRollWindow->toFront(true);
            }
        };

        addAndMakeVisible(seqBeatLabel);
        seqBeatLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        seqBeatLabel.setJustificationType(juce::Justification::centredLeft);
        seqBeatLabel.setText("beat: 0.0 / 8.0", juce::dontSendNotification);

        addAndMakeVisible(midiGainLabel);
        midiGainLabel.setText("Seq", juce::dontSendNotification);
        midiGainLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        midiGainLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(midiGainSlider);
        midiGainSlider.setRange(0.0, 2.0, 0.01);
        midiGainSlider.setValue(0.5);
        midiGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        midiGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        midiGainSlider.setTextBoxIsEditable(false);
        midiGainSlider.onValueChange = [this]
        {
            midiGain.store((float) midiGainSlider.getValue());
        };

        addAndMakeVisible(tracksViewport);

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
            synthGain.store((float) synthGainSlider.getValue());
        };

        setAudioChannels(0, 2);
        openAllMidiInputs();
        refreshMidiOutputCombo();

        tracksViewport.addTrack();
        refreshLayout();
        updateSeqButtons();

        setWantsKeyboardFocus(true);
        grabKeyboardFocus();

        startTimer(500);
    }

    ~MainComponent() override
    {
        *alive = false;
        stopTimer();
        closeAllMidiInputs();
        midiOutputRouter.sendAllNotesOff();
        midiOutputRouter.closeOutput();
        shutdownAudio();

        if (pianoRollWindow) { delete pianoRollWindow; pianoRollWindow = nullptr; }
        if (settingsWindow) { delete settingsWindow; settingsWindow = nullptr; }
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        synthSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
        midiTrack.prepareToPlay(samplesPerBlockExpected, sampleRate);
        tracksViewport.prepareAllToPlay(samplesPerBlockExpected, sampleRate);

        pluginHost.setSampleRate(sampleRate);
        pluginHost.setBlockSize(samplesPerBlockExpected);

        scratchBuffer.setSize(2, samplesPerBlockExpected);
        midiTrackScratch.setSize(2, samplesPerBlockExpected);

        deviceManager.addAudioCallback(&recorder);

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
        const bool anyTrackSoloed = tracksViewport.anyTrackSoloed();

        if (scratchBuffer.getNumChannels() < bufferToFill.buffer->getNumChannels()
            || scratchBuffer.getNumSamples() < bufferToFill.numSamples)
            scratchBuffer.setSize(bufferToFill.buffer->getNumChannels(), bufferToFill.numSamples);
        scratchBuffer.clear();

        juce::AudioSourceChannelInfo scratch(&scratchBuffer, 0, bufferToFill.numSamples);
        synthSource.getNextAudioBlock(scratch);
        midiOutputRouter.sendBuffer(synthSource.getLastMidiBuffer(), 0);
        const float g = synthGain.load();
        for (int ch = 0; ch < scratchBuffer.getNumChannels(); ++ch)
            scratchBuffer.applyGain(ch, 0, bufferToFill.numSamples, g);

        bufferToFill.buffer->clear();
        for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch)
            bufferToFill.buffer->addFrom(ch, bufferToFill.startSample, scratchBuffer,
                                         ch, 0, bufferToFill.numSamples);

        if (midiTrackScratch.getNumChannels() < bufferToFill.buffer->getNumChannels()
            || midiTrackScratch.getNumSamples() < bufferToFill.numSamples)
            midiTrackScratch.setSize(bufferToFill.buffer->getNumChannels(), bufferToFill.numSamples);
        midiTrackScratch.clear();
        juce::AudioSourceChannelInfo midiScratch(&midiTrackScratch, 0, bufferToFill.numSamples);
        midiTrack.getNextAudioBlock(midiScratch);
        midiOutputRouter.sendBuffer(midiTrack.getLastMidiBuffer(), 0);
        const float mg = midiGain.load();
        for (int ch = 0; ch < midiTrackScratch.getNumChannels(); ++ch)
            bufferToFill.buffer->addFrom(ch, bufferToFill.startSample, midiTrackScratch,
                                         ch, 0, bufferToFill.numSamples, mg);

        tracksViewport.tryRenderAll(*bufferToFill.buffer, bufferToFill.startSample,
                                    bufferToFill.numSamples, anyTrackSoloed);

        masterGainSmoothed.setTargetValue(masterGainTarget.load());
        const float master = masterGainSmoothed.getNextValue();
        for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch)
            bufferToFill.buffer->applyGain(ch, bufferToFill.startSample, bufferToFill.numSamples, master);

        float blockPeak = 0.0f;
        for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch)
            blockPeak = std::max(blockPeak, bufferToFill.buffer->getMagnitude(
                ch, bufferToFill.startSample, bufferToFill.numSamples));
        const float currentPeak = masterPeak.load();
        if (blockPeak > currentPeak)
            masterPeak.store(blockPeak);
    }

    void releaseResources() override
    {
        deviceManager.removeAudioCallback(&recorder);
        synthSource.releaseResources();
        midiTrack.releaseResources();
        tracksViewport.releaseAllResources();
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

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::spaceKey)
        {
            if (midiTrack.isPlaying())
                midiTrack.stop();
            else
                midiTrack.play();
            updateSeqButtons();
            return true;
        }
        return false;
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

        for (const auto& row : tracksViewport.getTrackRows())
            row->refreshTimeLabel();

        updateSeqButtons();
        seqBeatLabel.setText(
            "beat: " + juce::String(midiTrack.getCurrentBeat(), 1) + " / "
            + juce::String(midiTrack.getClipLengthBeats(), 1),
            juce::dontSendNotification);
    }

private:
    void onScanPlugins()
    {
        transportBar.setScanButtonEnabled(false);
        pluginHost.scanForPluginsAsync();
        juce::Timer::callAfterDelay(2000, [this]
        {
            transportBar.setScanButtonEnabled(! pluginHost.isScanning());
        });
    }

    void onSettings()
    {
        if (settingsWindow == nullptr)
        {
            auto aliveFlag = alive;
            settingsWindow = new SettingsWindow(deviceManager,
                [this, aliveFlag] { if (*aliveFlag) settingsWindow = nullptr; });
        }
        else
        {
            settingsWindow->toFront(true);
        }
    }

    void onMidiOutputChanged(const juce::String& name)
    {
        midiOutputRouter.sendAllNotesOff();
        if (name == TransportBar::noneOutputEntry || name.isEmpty())
            midiOutputRouter.closeOutput();
        else
            midiOutputRouter.openOutput(name);
    }

    void onMidiOutputRestored(const juce::String& name)
    {
        // Called from SessionIO::loadSession after the router has opened or
        // closed the device. The combo has not been refreshed yet, so push
        // the new selection explicitly.
        transportBar.setMidiOutputSelection(name, juce::dontSendNotification);
    }

    void updateSeqButtons()
    {
        seqPlayButton.setEnabled(! midiTrack.isPlaying());
        seqStopButton.setEnabled(midiTrack.isPlaying());
    }

    void refreshLayout()
    {
        auto area = getLocalBounds().reduced(12);
        area.removeFromTop(40);
        area.removeFromTop(8);

        transportBar.setBounds(area.removeFromTop(54));
        area.removeFromTop(6);

        auto seqRow = area.removeFromTop(28);
        seqPlayButton.setBounds(seqRow.removeFromLeft(70).reduced(2, 3));
        seqRow.removeFromLeft(4);
        seqStopButton.setBounds(seqRow.removeFromLeft(70).reduced(2, 3));
        seqRow.removeFromLeft(4);
        seqLoopButton.setBounds(seqRow.removeFromLeft(50).reduced(2, 3));
        seqRow.removeFromLeft(4);
        pianoRollButton.setBounds(seqRow.removeFromLeft(80).reduced(2, 3));
        seqRow.removeFromLeft(8);
        bpmLabel.setBounds(seqRow.removeFromLeft(40));
        seqRow.removeFromLeft(4);
        bpmSlider.setBounds(seqRow.removeFromLeft(120).reduced(0, 5));
        seqRow.removeFromLeft(8);
        seqBeatLabel.setBounds(seqRow.removeFromLeft(140).reduced(2, 4));
        seqRow.removeFromLeft(10);
        midiGainLabel.setBounds(seqRow.removeFromLeft(36));
        seqRow.removeFromLeft(4);
        midiGainSlider.setBounds(seqRow.removeFromLeft(140).reduced(0, 5));
        area.removeFromTop(6);

        tracksViewport.setBounds(area.removeFromTop(180));
        area.removeFromTop(8);

        midiKeyboard.setBounds(area.removeFromTop(140));

        auto synthRow = area.removeFromTop(28);
        synthGainLabel.setBounds(synthRow.removeFromLeft(50));
        synthRow.removeFromLeft(8);
        synthGainSlider.setBounds(synthRow);
    }

    void refreshMidiOutputCombo()
    {
        transportBar.refreshMidiOutputCombo(
            midiOutputRouter.getAvailableOutputNames(),
            midiOutputRouter.isOpen() ? midiOutputRouter.getCurrentName() : TransportBar::noneOutputEntry);
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
        const int midiCount = (int) openedInputs.size();
        juce::String midiList = "none";
        if (midiCount > 0)
        {
            juce::StringArray names;
            for (auto* in : openedInputs)
                names.add(in->getName());
            midiList = names.joinIntoString(", ");
        }
        juce::String trackStates;
        const auto& tracks = tracksViewport.getTracks();
        for (size_t i = 0; i < tracks.size(); ++i)
        {
            if (i > 0) trackStates += "  ";
            const char* state = ".";
            if (tracks[i]->isMuted()) state = "M";
            else if (tracks[i]->getSource().isPlaying()) state = "P";
            trackStates += "T" + juce::String(i + 1) + ":" + state;
        }
        transportBar.setStatusText("Device: " + deviceName
            + "  |  Buffer: " + juce::String(bufferSize)
            + "  |  Rate: " + juce::String(actualSampleRate, 1) + " Hz"
            + "  |  MIDI (" + juce::String(midiCount) + "): " + midiList
            + "  |  " + trackStates);
    }

    SynthAudioSource synthSource;
    MidiTrack midiTrack;
    Recorder recorder;
    MidiOutputRouter midiOutputRouter;
    PluginHost pluginHost;

    std::atomic<float> synthGain{0.6f};
    std::atomic<float> midiGain{0.5f};
    std::atomic<float> masterGainTarget{0.8f};
    std::atomic<float> masterPeak{0.0f};
    juce::SmoothedValue<float> masterGainSmoothed{0.8f};

    juce::AudioBuffer<float> scratchBuffer;
    juce::AudioBuffer<float> midiTrackScratch;
    juce::OwnedArray<juce::MidiInput> openedInputs;

    TransportBar transportBar;
    TracksViewport tracksViewport;
    SessionIO sessionIO;

    juce::MidiKeyboardComponent midiKeyboard;
    juce::TextButton seqPlayButton;
    juce::TextButton seqStopButton;
    juce::TextButton seqLoopButton;
    juce::TextButton pianoRollButton;
    juce::Label bpmLabel;
    juce::Slider bpmSlider;
    juce::Label seqBeatLabel;
    juce::Label midiGainLabel;
    juce::Slider midiGainSlider;
    juce::Label synthGainLabel;
    juce::Slider synthGainSlider;

    PianoRollWindow* pianoRollWindow = nullptr;
    SettingsWindow* settingsWindow = nullptr;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
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
