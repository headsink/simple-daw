#include "SessionIO.h"
#include "../audio/Recorder.h"
#include "../midi/MidiTrack.h"
#include "../midi/MidiOutputRouter.h"
#include "../tracks/TracksViewport.h"
#include "../plugin/PluginHost.h"
#include "../ui/TransportBar.h"

SessionIO::SessionIO(juce::AudioDeviceManager& dm,
                     Recorder& rec,
                     MidiOutputRouter& router,
                     MidiTrack& track,
                     TracksViewport& tracksVp,
                     PluginHost& plugins,
                     TransportBar& transport,
                     LoadFloatGetter getSynthGainCb,
                     LoadFloatGetter getMidiGainCb,
                     LoadDoubleGetter getBpmCb,
                     LoadFloatSetter setSynthGainCb,
                     LoadFloatSetter setMidiGainCb,
                     LoadDoubleSetter setBpmCb,
                     std::function<void(const juce::String&)> onMidiOutSelectionChangedCb,
                     std::function<void()> onPostLoadLayoutCb)
    : deviceManager(dm),
      recorder(rec),
      midiOutputRouter(router),
      midiTrack(track),
      tracksViewport(tracksVp),
      pluginHost(plugins),
      transportBar(transport),
      getSynthGain(std::move(getSynthGainCb)),
      getMidiGain(std::move(getMidiGainCb)),
      getBpm(std::move(getBpmCb)),
      setSynthGain(std::move(setSynthGainCb)),
      setMidiGain(std::move(setMidiGainCb)),
      setBpm(std::move(setBpmCb)),
      onMidiOutSelectionChanged(std::move(onMidiOutSelectionChangedCb)),
      onPostLoadLayout(std::move(onPostLoadLayoutCb))
{
}

void SessionIO::saveSession()
{
    juce::FileChooser chooser("Save Session",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.sdaw");
    if (! chooser.browseForFileToSave(true)) return;

    auto file = chooser.getResult();
    if (file.existsAsFile() && ! file.deleteFile()) return;

    auto* obj = new juce::DynamicObject();
    obj->setProperty("version", 1);
    obj->setProperty("masterGain", (double) transportBar.getMasterGain());
    obj->setProperty("synthGain", (double) getSynthGain());
    obj->setProperty("midiGain", (double) getMidiGain());
    obj->setProperty("bpm", getBpm());
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
    for (const auto& t : tracksViewport.getTracks())
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

        if (t->hasPlugin() && t->getPluginDesc() != nullptr && t->getPlugin() != nullptr)
        {
            const auto* desc = t->getPluginDesc();
            auto* pObj = new juce::DynamicObject();
            pObj->setProperty("identifier", desc->createIdentifierString());
            pObj->setProperty("name", desc->name);
            pObj->setProperty("manufacturer", desc->manufacturerName);
            pObj->setProperty("format", desc->pluginFormatName);
            pObj->setProperty("bypass", t->isPluginBypassed());

            juce::MemoryBlock stateBlock;
            t->getPlugin()->getStateInformation(stateBlock);
            pObj->setProperty("state",
                juce::Base64::toBase64(stateBlock.getData(), stateBlock.getSize()));

            tObj->setProperty("plugin", juce::var(pObj));
        }

        tracksArray.add(juce::var(tObj));
    }
    obj->setProperty("tracks", tracksArray);

    if (auto stream = std::unique_ptr<juce::FileOutputStream>(file.createOutputStream()))
    {
        juce::JSON::writeToStream(*stream, juce::var(obj), true);
        stream->flush();
    }
}

void SessionIO::loadSession()
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

    const float newMaster = (float)(double) json["masterGain"];
    setSynthGain((float)(double) json["synthGain"], juce::dontSendNotification);
    setMidiGain((float)(double) json["midiGain"], juce::dontSendNotification);
    transportBar.setMasterGain(newMaster, juce::dontSendNotification);

    const double bpmVal = json["bpm"].isVoid() ? 120.0 : (double) json["bpm"];
    midiTrack.setBpm(bpmVal);
    setBpm(juce::jlimit(40.0, 240.0, bpmVal), juce::dontSendNotification);

    {
        const juce::String savedOut = json["midiOutput"].toString();
        midiOutputRouter.sendAllNotesOff();
        if (savedOut.isNotEmpty() && savedOut != TransportBar::noneOutputEntry)
        {
            if (midiOutputRouter.openOutput(savedOut))
                onMidiOutSelectionChanged(savedOut);
            else
                onMidiOutSelectionChanged(TransportBar::noneOutputEntry);
        }
        else
        {
            midiOutputRouter.closeOutput();
            onMidiOutSelectionChanged(TransportBar::noneOutputEntry);
        }
    }

    auto* clipObj = json["midiClip"].getDynamicObject();
    if (clipObj != nullptr)
    {
        std::vector<MidiNote> newNotes;
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
                newNotes.push_back(note);
            }
        }
        const juce::ScopedLock sl(midiTrack.getClip().lock);
        midiTrack.getClip().setLengthBeats((double) clipObj->getProperty("lengthBeats"));
        midiTrack.getClip().replaceAllNotes(newNotes);
        midiTrack.getClip().clearUndoHistory();
    }

    tracksViewport.clearAll();

    auto* tracksArray = json["tracks"].getArray();
    if (tracksArray != nullptr)
    {
        for (const auto& trackVar : *tracksArray)
        {
            auto* tObj = trackVar.getDynamicObject();
            if (tObj == nullptr) continue;

            juce::File audioFile(tObj->getProperty("filePath").toString());
            if (! audioFile.existsAsFile()) continue;

            const float savedGain = (float)(double) tObj->getProperty("gain");
            const int savedLoopStart = (int)(double) tObj->getProperty("loopStart");
            const int savedLoopEnd   = (int)(double) tObj->getProperty("loopEnd");
            TrackRow* rowPtr = tracksViewport.createTrackFromFileAsync(audioFile, savedGain,
                                                                       savedLoopStart, savedLoopEnd);
            auto& track = rowPtr->getTrack();

            track.setPan((float)(double) tObj->getProperty("pan"));
            track.setMute((bool) tObj->getProperty("mute"));
            track.setSolo((bool) tObj->getProperty("solo"));
            track.getSource().setLooping((bool) tObj->getProperty("looping"));

            juce::String pluginIdentifier;
            juce::String pluginStateB64;
            bool pluginBypass = false;
            if (auto* pObj = tObj->getProperty("plugin").getDynamicObject())
            {
                pluginIdentifier = pObj->getProperty("identifier").toString();
                pluginStateB64 = pObj->getProperty("state").toString();
                pluginBypass = (bool) pObj->getProperty("bypass");
            }

            if (pluginIdentifier.isNotEmpty())
            {
                juce::PluginDescription matchDesc;
                bool found = false;
                for (const auto& t : pluginHost.getKnownList().getTypes())
                {
                    if (t.createIdentifierString() == pluginIdentifier)
                    {
                        matchDesc = t;
                        found = true;
                        break;
                    }
                }
                if (found)
                {
                    auto token = track.getLifetimeToken();
                    const int myVersion = token->load();
                    const juce::String stateB64 = pluginStateB64;
                    const bool bypass = pluginBypass;
                    const juce::String ident = pluginIdentifier;

                    pluginHost.createInstanceAsync(matchDesc,
                        [this, rowPtr, token, myVersion, stateB64, bypass, ident]
                        (std::unique_ptr<juce::AudioPluginInstance> inst, const juce::String& err)
                        {
                            if (token->load() != myVersion) return;
                            if (rowPtr == nullptr) return;
                            if (! inst) return;
                            if (stateB64.isNotEmpty())
                            {
                                juce::MemoryOutputStream decoded;
                                juce::Base64::convertFromBase64(decoded, stateB64);
                                decoded.flush();
                                if (decoded.getDataSize() > 0)
                                {
                                    try
                                    {
                                        inst->setStateInformation(decoded.getData(),
                                                                   (int) decoded.getDataSize());
                                    }
                                    catch (...)
                                    {
                                        juce::Logger::writeToLog("SessionIO: plugin "
                                            + inst->getName() + " rejected saved state.");
                                    }
                                }
                            }
                            auto descCopy = std::make_unique<juce::PluginDescription>();
                            for (const auto& t : pluginHost.getKnownList().getTypes())
                            {
                                if (t.createIdentifierString() == ident) { *descCopy = t; break; }
                            }
                            rowPtr->getTrack().setPlugin(std::move(inst), std::move(descCopy));
                            rowPtr->getTrack().setPluginBypass(bypass);
                            juce::MessageManager::getInstance()->callAsync(
                                [rowPtr, token, myVersion]
                                {
                                    if (token->load() != myVersion) return;
                                    if (rowPtr == nullptr) return;
                                    rowPtr->refreshPluginLabel();
                                    rowPtr->updateButtons();
                                });
                        });
                }
            }
        }
    }

    if (onPostLoadLayout) onPostLoadLayout();
}

void SessionIO::toggleRecording()
{
    if (recorder.isRecording())
    {
        const juce::String path = recorder.stopRecording();
        transportBar.setRecToggleState(false, juce::dontSendNotification);

        if (path.isEmpty()) return;

        juce::File file(path);
        if (! file.existsAsFile()) return;

        tracksViewport.createTrackFromFileAsync(file, 0.6f);
        if (onPostLoadLayout) onPostLoadLayout();
    }
    else
    {
        recorder.startRecording();
    }
}
