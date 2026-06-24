#pragma once

#include <JuceHeader.h>

class Recorder;
class MidiTrack;
class MidiOutputRouter;
class TracksViewport;
class PluginHost;
class TransportBar;

class SessionIO
{
public:
    using LoadFloatSetter = std::function<void(float, juce::NotificationType)>;
    using LoadDoubleSetter = std::function<void(double, juce::NotificationType)>;
    using LoadFloatGetter = std::function<float()>;
    using LoadDoubleGetter = std::function<double()>;

    SessionIO(juce::AudioDeviceManager& dm,
              Recorder& rec,
              MidiOutputRouter& router,
              MidiTrack& track,
              TracksViewport& tracksVp,
              PluginHost& plugins,
              TransportBar& transport,
              LoadFloatGetter getSynthGain,
              LoadFloatGetter getMidiGain,
              LoadDoubleGetter getBpm,
              LoadFloatSetter setSynthGain,
              LoadFloatSetter setMidiGain,
              LoadDoubleSetter setBpm,
              std::function<void(const juce::String&)> onMidiOutSelectionChanged,
              std::function<void()> onPostLoadLayout);

    void saveSession();
    void loadSession();
    void toggleRecording();

private:
    juce::AudioDeviceManager& deviceManager;
    Recorder& recorder;
    MidiOutputRouter& midiOutputRouter;
    MidiTrack& midiTrack;
    TracksViewport& tracksViewport;
    PluginHost& pluginHost;
    TransportBar& transportBar;

    LoadFloatGetter getSynthGain;
    LoadFloatGetter getMidiGain;
    LoadDoubleGetter getBpm;
    LoadFloatSetter setSynthGain;
    LoadFloatSetter setMidiGain;
    LoadDoubleSetter setBpm;
    std::function<void(const juce::String&)> onMidiOutSelectionChanged;
    std::function<void()> onPostLoadLayout;
};
