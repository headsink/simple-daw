#include <JuceHeader.h>
#include "midi/SynthAudioSource.h"
#include "midi/MidiTrack.h"
#include "midi/MidiOutputRouter.h"
#include "tracks/AudioTrack.h"
#include "tracks/TrackRow.h"
#include "plugin/PluginHost.h"
#include "ui/PianoRollComponent.h"
#include "ui/PeakMeterComponent.h"
#include "ui/SettingsWindow.h"
#include "audio/Recorder.h"

class MainComponent : public juce::AudioAppComponent,
                      public juce::MidiInputCallback,
                      public juce::Timer
{
    static inline const juce::String noneOutputEntry { "(none)" };
public:
    MainComponent()
        : midiKeyboard(synthSource.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
          peakMeter(masterPeak)
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

        addAndMakeVisible(settingsButton);
        settingsButton.setButtonText("Settings");
        settingsButton.onClick = [this]
        {
            if (settingsWindow == nullptr)
            {
                settingsWindow = new SettingsWindow(deviceManager,
                    [this] { settingsWindow = nullptr; });
            }
            else
            {
                settingsWindow->toFront(true);
            }
        };

        addAndMakeVisible(saveButton);
        saveButton.setButtonText("Save");
        saveButton.onClick = [this] { saveSession(); };

        addAndMakeVisible(loadButton);
        loadButton.setButtonText("Load");
        loadButton.onClick = [this] { loadSession(); };

        addAndMakeVisible(recordButton);
        recordButton.setButtonText("Rec");
        recordButton.setClickingTogglesState(true);
        recordButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffaa3030));
        recordButton.onClick = [this] { toggleRecording(); };

        addAndMakeVisible(midiOutputLabel);
        midiOutputLabel.setText("MIDI Out", juce::dontSendNotification);
        midiOutputLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        midiOutputLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(midiOutputCombo);
        midiOutputCombo.setTooltip("Route on-screen keyboard and sequencer notes to a hardware/software MIDI output");
        midiOutputCombo.onChange = [this]
        {
            const auto name = midiOutputCombo.getText();
            midiOutputRouter.sendAllNotesOff();
            if (name == noneOutputEntry || name.isEmpty())
                midiOutputRouter.closeOutput();
            else
                midiOutputRouter.openOutput(name);
        };
        refreshMidiOutputCombo();

        addAndMakeVisible(tracksViewport);
        tracksViewport.setViewedComponent(&tracksContainer);
        tracksViewport.setScrollBarsShown(true, false);

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
                pianoRollWindow = new PianoRollWindow(midiTrack, synthSource.keyboardState,
                    [this] { pianoRollWindow = nullptr; });
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
            midiGain.store((float)midiGainSlider.getValue());
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

        addAndMakeVisible(peakMeter);
        addAndMakeVisible(peakLabel);
        peakLabel.setText("Peak", juce::dontSendNotification);
        peakLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        peakLabel.setJustificationType(juce::Justification::centredRight);

        addTrack();
        refreshLayout();
        updateSeqButtons();

        startTimer(500);
    }

    ~MainComponent() override
    {
        stopTimer();
        closeAllMidiInputs();
        midiOutputRouter.sendAllNotesOff();
        midiOutputRouter.closeOutput();
        shutdownAudio();

        if (pianoRollWindow) { delete pianoRollWindow; pianoRollWindow = nullptr; }
        if (settingsWindow) { delete settingsWindow; settingsWindow = nullptr; }

        {
            const juce::SpinLock::ScopedLockType sl(tracksLock);
            trackRows.clear();
            tracks.clear();
        }
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        synthSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
        midiTrack.prepareToPlay(samplesPerBlockExpected, sampleRate);
        for (auto& t : tracks)
            t->prepareToPlay(samplesPerBlockExpected, sampleRate);

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
        bool anyTrackSoloed = false;
        {
            const juce::SpinLock::ScopedTryLockType sl(tracksLock);
            if (sl.isLocked())
            {
                for (auto& t : tracks)
                    if (t->isSolo()) { anyTrackSoloed = true; break; }
            }
        }

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

        {
            const juce::SpinLock::ScopedTryLockType sl(tracksLock);
            if (sl.isLocked())
            {
                for (auto& t : tracks)
                    t->renderInto(*bufferToFill.buffer, bufferToFill.startSample,
                                  bufferToFill.numSamples, anyTrackSoloed);
            }
        }

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

        updateSeqButtons();
        seqBeatLabel.setText(
            "beat: " + juce::String(midiTrack.getCurrentBeat(), 1) + " / "
            + juce::String(midiTrack.getClipLengthBeats(), 1),
            juce::dontSendNotification);
    }

private:
    void addTrack()
    {
        double sr = 48000.0;
        int bs = 512;
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            sr = device->getCurrentSampleRate();
            bs = device->getCurrentBufferSizeSamples();
        }

        auto track = std::make_unique<AudioTrack>();
        track->prepareToPlay(bs, sr);

        auto* trackPtr = track.get();
        auto row = std::make_unique<TrackRow>(*trackPtr, pluginHost,
            [this](TrackRow* r) { removeTrack(r); },
            [this] { refreshLayout(); });

        {
            const juce::SpinLock::ScopedLockType sl(tracksLock);
            tracks.push_back(std::move(track));
            trackRows.push_back(std::move(row));
        }
        tracksContainer.addAndMakeVisible(trackRows.back().get());
        refreshLayout();
    }

    void removeTrack(TrackRow* row)
    {
        // Defer the actual destruction: removeTrack is invoked from inside the
        // TrackRow's own removeButton.onClick lambda, so deleting the TrackRow
        // synchronously here would destroy the Button mid-callback and corrupt
        // the heap (trips _CrtIsValidHeapPointer). Detach from the parent now
        // (safe — just unparents), then destroy on the message thread.
        tracksContainer.removeChildComponent(row);

        juce::MessageManager::getInstance()->callAsync(
            [this, row]
            {
                for (size_t i = 0; i < trackRows.size(); ++i)
                {
                    if (trackRows[i].get() == row)
                    {
                        const juce::SpinLock::ScopedLockType sl(tracksLock);
                        trackRows.erase(trackRows.begin() + i);
                        tracks.erase(tracks.begin() + i);
                        break;
                    }
                }
                refreshLayout();
            });
    }

    void updateSeqButtons()
    {
        seqPlayButton.setEnabled(! midiTrack.isPlaying());
        seqStopButton.setEnabled(midiTrack.isPlaying());
    }

    void saveSession()
    {
        juce::FileChooser chooser("Save Session",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.sdaw");
        if (! chooser.browseForFileToSave(true)) return;

        auto file = chooser.getResult();
        if (file.existsAsFile() && ! file.deleteFile()) return;

        auto* obj = new juce::DynamicObject();
        obj->setProperty("version", 1);
        obj->setProperty("masterGain", (double) masterGainTarget.load());
        obj->setProperty("synthGain", (double) synthGain.load());
        obj->setProperty("midiGain", (double) midiGain.load());
        obj->setProperty("bpm", midiTrack.getBpm());
        obj->setProperty("midiOutput", midiOutputRouter.isOpen() ? midiOutputRouter.getCurrentName() : juce::String());

        auto* clipObj = new juce::DynamicObject();
        clipObj->setProperty("lengthBeats", midiTrack.getClip().getLengthBeats());
        {
            const juce::ScopedLock sl(midiTrack.getClip().lock);
            juce::Array<juce::var> notesArray;
            for (const auto& note : midiTrack.getClip().getNotes())
            {
                auto* n = new juce::DynamicObject();
                n->setProperty("pitch", note.pitch);
                n->setProperty("startBeat", note.startBeat);
                n->setProperty("lengthBeats", note.lengthBeats);
                n->setProperty("velocity", (int) note.velocity);
                notesArray.add(juce::var(n));
            }
            clipObj->setProperty("notes", notesArray);
        }
        obj->setProperty("midiClip", juce::var(clipObj));

        juce::Array<juce::var> tracksArray;
        for (const auto& t : tracks)
        {
            if (! t->getSource().isLoaded()) continue;
            auto* tObj = new juce::DynamicObject();
            tObj->setProperty("filePath", t->getSource().getFilePath());
            tObj->setProperty("gain", (double) t->getGain());
            tObj->setProperty("pan", (double) t->getPan());
            tObj->setProperty("mute", t->isMuted());
            tObj->setProperty("solo", t->isSolo());
            tObj->setProperty("looping", t->getSource().isLooping());
            tObj->setProperty("loopStart", t->getLoopStart());
            tObj->setProperty("loopEnd", t->getLoopEnd());
            tracksArray.add(juce::var(tObj));
        }
        obj->setProperty("tracks", tracksArray);

        if (auto stream = std::unique_ptr<juce::FileOutputStream>(file.createOutputStream()))
        {
            juce::JSON::writeToStream(*stream, juce::var(obj), true);
            stream->flush();
        }
    }

    void loadSession()
    {
        juce::FileChooser chooser("Load Session",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.sdaw");
        if (! chooser.browseForFileToOpen()) return;

        auto file = chooser.getResult();
        if (! file.existsAsFile()) return;

        auto json = juce::JSON::parse(file.loadFileAsString());
        if (! json.isObject())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Load Error", "Invalid session file.");
            return;
        }

        midiTrack.stop();

        masterGainTarget.store((float)(double) json["masterGain"]);
        masterGainSlider.setValue((double) masterGainTarget.load(), juce::dontSendNotification);
        synthGain.store((float)(double) json["synthGain"]);
        synthGainSlider.setValue((double) synthGain.load(), juce::dontSendNotification);
        midiGain.store((float)(double) json["midiGain"]);
        midiGainSlider.setValue((double) midiGain.load(), juce::dontSendNotification);

        midiTrack.setBpm((double) json["bpm"]);
        bpmSlider.setValue((double) json["bpm"], juce::dontSendNotification);

        {
            const juce::String savedOut = json["midiOutput"].toString();
            refreshMidiOutputCombo();
            if (savedOut.isNotEmpty() && savedOut != noneOutputEntry)
            {
                if (midiOutputRouter.openOutput(savedOut))
                    midiOutputCombo.setText(savedOut, juce::dontSendNotification);
                else
                    midiOutputCombo.setText(noneOutputEntry, juce::dontSendNotification);
            }
            else
            {
                midiOutputCombo.setText(noneOutputEntry, juce::dontSendNotification);
            }
        }

        auto* clipObj = json["midiClip"].getDynamicObject();
        if (clipObj != nullptr)
        {
            const juce::ScopedLock sl(midiTrack.getClip().lock);
            midiTrack.getClip().clearNotes();
            midiTrack.getClip().setLengthBeats((double) clipObj->getProperty("lengthBeats"));
            auto* notesArray = clipObj->getProperty("notes").getArray();
            if (notesArray != nullptr)
            {
                for (const auto& noteVar : *notesArray)
                {
                    auto* nObj = noteVar.getDynamicObject();
                    if (nObj == nullptr) continue;
                    MidiNote note;
                    note.pitch = (int) nObj->getProperty("pitch");
                    note.startBeat = (double) nObj->getProperty("startBeat");
                    note.lengthBeats = (double) nObj->getProperty("lengthBeats");
                    note.velocity = (uint8_t)(int) nObj->getProperty("velocity");
                    midiTrack.getClip().addNote(note);
                }
            }
        }

        {
            const juce::SpinLock::ScopedLockType sl(tracksLock);
            trackRows.clear();
            tracks.clear();
        }

        double sr = 48000.0;
        int bs = 512;
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            sr = device->getCurrentSampleRate();
            bs = device->getCurrentBufferSizeSamples();
        }

        auto* tracksArray = json["tracks"].getArray();
        if (tracksArray != nullptr)
        {
            for (const auto& trackVar : *tracksArray)
            {
                auto* tObj = trackVar.getDynamicObject();
                if (tObj == nullptr) continue;

                juce::File audioFile(tObj->getProperty("filePath").toString());
                if (! audioFile.existsAsFile()) continue;

                auto track = std::make_unique<AudioTrack>();
                track->prepareToPlay(bs, sr);
                track->loadFile(audioFile);
                track->setGain((float)(double) tObj->getProperty("gain"));
                track->setPan((float)(double) tObj->getProperty("pan"));
                track->setMute((bool) tObj->getProperty("mute"));
                track->setSolo((bool) tObj->getProperty("solo"));
                track->getSource().setLooping((bool) tObj->getProperty("looping"));
                track->setLoopStart((int)(double) tObj->getProperty("loopStart"));
                track->setLoopEnd((int)(double) tObj->getProperty("loopEnd"));

                auto* trackPtr = track.get();
                auto row = std::make_unique<TrackRow>(*trackPtr, pluginHost,
                    [this](TrackRow* r) { removeTrack(r); },
                    [this] { refreshLayout(); });
                row->setNameText(track->getSource().getLoadedFileName());

                {
                    const juce::SpinLock::ScopedLockType sl(tracksLock);
                    tracks.push_back(std::move(track));
                    trackRows.push_back(std::move(row));
                }
                tracksContainer.addAndMakeVisible(trackRows.back().get());
            }
        }

        refreshLayout();
    }

    void toggleRecording()
    {
        if (recorder.isRecording())
        {
            const juce::String path = recorder.stopRecording();
            recordButton.setToggleState(false, juce::dontSendNotification);

            if (path.isEmpty()) return;

            juce::File file(path);
            if (! file.existsAsFile()) return;

            double sr = 48000.0;
            int bs = 512;
            if (auto* device = deviceManager.getCurrentAudioDevice())
            {
                sr = device->getCurrentSampleRate();
                bs = device->getCurrentBufferSizeSamples();
            }

            auto track = std::make_unique<AudioTrack>();
            track->prepareToPlay(bs, sr);
            track->loadFile(file);

            auto* trackPtr = track.get();
            auto row = std::make_unique<TrackRow>(*trackPtr, pluginHost,
                [this](TrackRow* r) { removeTrack(r); },
                [this] { refreshLayout(); });
            row->setNameText(track->getSource().getLoadedFileName());

            {
                const juce::SpinLock::ScopedLockType sl(tracksLock);
                tracks.push_back(std::move(track));
                trackRows.push_back(std::move(row));
            }
            tracksContainer.addAndMakeVisible(trackRows.back().get());
            refreshLayout();
        }
        else
        {
            recorder.startRecording();
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
        topRow.removeFromRight(8);
        settingsButton.setBounds(topRow.removeFromRight(80));
        topRow.removeFromRight(16);
        masterGainLabel.setBounds(topRow.removeFromRight(60));
        topRow.removeFromRight(4);
        masterGainSlider.setBounds(topRow.removeFromRight(180));
        topRow.removeFromRight(12);
        peakLabel.setBounds(topRow.removeFromRight(40));
        topRow.removeFromRight(4);
        peakMeter.setBounds(topRow.removeFromRight(140).reduced(0, 6));
        saveButton.setBounds(topRow.removeFromLeft(50).reduced(2, 3));
        topRow.removeFromLeft(4);
        loadButton.setBounds(topRow.removeFromLeft(50).reduced(2, 3));
        topRow.removeFromLeft(4);
        recordButton.setBounds(topRow.removeFromLeft(50).reduced(2, 3));
        topRow.removeFromLeft(8);
        midiOutputLabel.setBounds(topRow.removeFromLeft(56));
        topRow.removeFromLeft(4);
        midiOutputCombo.setBounds(topRow.removeFromLeft(160).reduced(0, 4));
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

    void refreshMidiOutputCombo()
    {
        midiOutputCombo.clear();
        const auto names = midiOutputRouter.getAvailableOutputNames();
        midiOutputCombo.addItem(noneOutputEntry, 1);
        for (int i = 0; i < names.size(); ++i)
            midiOutputCombo.addItem(names[i], i + 2);
        midiOutputCombo.setText(midiOutputRouter.isOpen() ? midiOutputRouter.getCurrentName() : noneOutputEntry,
                                juce::dontSendNotification);
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
    MidiTrack midiTrack;
    Recorder recorder;
    MidiOutputRouter midiOutputRouter;
    juce::SpinLock tracksLock;
    PluginHost pluginHost;
    std::vector<std::unique_ptr<AudioTrack>> tracks;
    std::vector<std::unique_ptr<TrackRow>> trackRows;

    juce::MidiKeyboardComponent midiKeyboard;
    juce::Component tracksContainer;
    juce::Viewport tracksViewport;
    juce::TextButton addTrackButton;
    juce::TextButton scanPluginsButton;
    juce::TextButton settingsButton;
    juce::TextButton saveButton;
    juce::TextButton loadButton;
    juce::TextButton recordButton;
    juce::Label midiOutputLabel;
    juce::ComboBox midiOutputCombo;
    juce::TextButton seqPlayButton;
    juce::TextButton seqStopButton;
    juce::TextButton seqLoopButton;
    juce::TextButton pianoRollButton;
    juce::Label bpmLabel;
    juce::Slider bpmSlider;
    juce::Label seqBeatLabel;
    PianoRollWindow* pianoRollWindow = nullptr;
    SettingsWindow* settingsWindow = nullptr;
    juce::Label statusLabel;
    juce::Label synthGainLabel;
    juce::Slider synthGainSlider;
    juce::Label midiGainLabel;
    juce::Slider midiGainSlider;
    juce::Label masterGainLabel;
    juce::Slider masterGainSlider;
    juce::Label peakLabel;
    std::atomic<float> masterPeak{0.0f};
    PeakMeterComponent peakMeter;
    juce::OwnedArray<juce::MidiInput> openedInputs;
    juce::AudioBuffer<float> scratchBuffer;
    juce::AudioBuffer<float> midiTrackScratch;
    std::atomic<float> synthGain{0.6f};
    std::atomic<float> midiGain{0.5f};
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
