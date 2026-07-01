#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

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
        std::unique_ptr<juce::MidiOutput> newOut;
        juce::String newName;

        if (deviceName.isNotEmpty())
        {
            for (const auto& d : juce::MidiOutput::getAvailableDevices())
            {
                if (d.name == deviceName)
                {
                    newOut = juce::MidiOutput::openDevice(d.identifier);
                    if (newOut != nullptr)
                    {
                        newOut->startBackgroundThread();
                        newName = deviceName;
                    }
                    break;
                }
            }
        }

        juce::MidiOutput* old = nullptr;
        {
            const juce::CriticalSection::ScopedLockType sl(swapLock);
            old = outputPtr.exchange(newOut.release(), std::memory_order_acq_rel);
            currentName = newName;
        }
        if (old != nullptr)
        {
            old->stopBackgroundThread();
            delete old;
        }
        return newName.isNotEmpty();
    }

    void closeOutput() { openOutput({}); }

    bool isOpen() const
    {
        return outputPtr.load(std::memory_order_acquire) != nullptr;
    }

    juce::String getCurrentName() const
    {
        const juce::CriticalSection::ScopedLockType sl(swapLock);
        return currentName;
    }

    void sendNow(const juce::MidiMessage& message)
    {
        auto* out = outputPtr.load(std::memory_order_acquire);
        if (out != nullptr)
            out->sendMessageNow(message);
    }

    void sendBuffer(const juce::MidiBuffer& buffer, int /*blockStartSample*/)
    {
        auto* out = outputPtr.load(std::memory_order_acquire);
        if (out == nullptr)
            return;

        for (const auto meta : buffer)
            out->sendMessageNow(meta.getMessage());
    }

    void sendAllNotesOff()
    {
        auto* out = outputPtr.load(std::memory_order_acquire);
        if (out == nullptr)
            return;
        for (int ch = 1; ch <= 16; ++ch)
            out->sendMessageNow(juce::MidiMessage::allNotesOff(ch));
    }

private:
    mutable juce::CriticalSection swapLock;
    std::atomic<juce::MidiOutput*> outputPtr{nullptr};
    juce::String currentName;
};
