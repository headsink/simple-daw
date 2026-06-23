#pragma once

#include <JuceHeader.h>

class MidiOutputRouter
{
public:
    MidiOutputRouter() = default;
    ~MidiOutputRouter() { closeOutput(); }

    juce::StringArray getAvailableOutputNames() const
    {
        juce::StringArray names;
        for (const auto& d : juce::MidiOutput::getAvailableDevices())
            names.add(d.name);
        return names;
    }

    bool openOutput(const juce::String& deviceName)
    {
        closeOutput();

        if (deviceName.isEmpty())
            return false;

        const auto devices = juce::MidiOutput::getAvailableDevices();
        for (const auto& d : devices)
        {
            if (d.name == deviceName)
            {
                auto out = juce::MidiOutput::openDevice(d.identifier);
                if (out != nullptr)
                {
                    out->startBackgroundThread();
                    output = std::move(out);
                    currentName = deviceName;
                    return true;
                }
            }
        }
        return false;
    }

    void closeOutput()
    {
        if (output != nullptr)
        {
            output->stopBackgroundThread();
            output.reset();
        }
        currentName.clear();
    }

    bool isOpen() const { return output != nullptr; }
    juce::String getCurrentName() const { return currentName; }

    // Called from the audio thread. Sends immediately for sample-accurate timing.
    // JUCE's MidiOutput::sendMessageNow is safe to call from the audio thread.
    void sendNow(const juce::MidiMessage& message)
    {
        if (output != nullptr)
            output->sendMessageNow(message);
    }

    // Called from the audio thread to send a whole MidiBuffer.
    void sendBuffer(const juce::MidiBuffer& buffer, int /*blockStartSample*/)
    {
        if (output == nullptr)
            return;

        for (const auto meta : buffer)
            output->sendMessageNow(meta.getMessage());
    }

    // Send all-notes-off on all 16 channels (call on close / device switch).
    void sendAllNotesOff()
    {
        if (output == nullptr)
            return;
        for (int ch = 1; ch <= 16; ++ch)
        {
            output->sendMessageNow(juce::MidiMessage::allNotesOff(ch));
        }
    }

private:
    std::unique_ptr<juce::MidiOutput> output;
    juce::String currentName;
};
