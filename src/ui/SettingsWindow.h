#pragma once

#include <JuceHeader.h>

class SettingsWindow : public juce::DocumentWindow
{
public:
    using OnClosed = std::function<void()>;

    SettingsWindow(juce::AudioDeviceManager& dm, OnClosed cb)
        : juce::DocumentWindow("Audio Settings",
                               juce::Desktop::getInstance().getDefaultLookAndFeel()
                                   .findColour(juce::ResizableWindow::backgroundColourId),
                               juce::DocumentWindow::closeButton),
          onClosed(std::move(cb))
    {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setContentOwned(new juce::AudioDeviceSelectorComponent(
            dm,
            0, 2,
            0, 2,
            false, false,
            true, false), true);
        setSize(500, 400);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
        toFront(true);
    }

    void closeButtonPressed() override
    {
        bool expected = false;
        if (! closing.compare_exchange_strong(expected, true))
            return;

        juce::MessageManager::getInstance()->callAsync(
            [cb = onClosed, w = this]
            {
                if (cb) cb();
                delete w;
            });
    }

private:
    OnClosed onClosed;
    std::atomic<bool> closing{false};
};
