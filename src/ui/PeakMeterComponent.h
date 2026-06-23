#pragma once

#include <JuceHeader.h>

class PeakMeterComponent : public juce::Component,
                          public juce::Timer
{
public:
    explicit PeakMeterComponent(std::atomic<float>& peak)
        : peakRef(peak)
    {
        startTimerHz(30);
    }

    ~PeakMeterComponent() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        g.setColour(juce::Colour(0xff202020));
        g.fillRect(area);

        if (displayedLevel > 1.0e-6f)
        {
            const float db = 20.0f * std::log10(displayedLevel);
            const float pos = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
            const int barWidth = juce::roundToInt(pos * (float) area.getWidth());

            juce::Colour color = juce::Colour(0xff30c030);
            if (pos > 0.8f)        color = juce::Colour(0xffd03030);
            else if (pos > 0.6f)   color = juce::Colour(0xffd0c030);

            g.setColour(color);
            g.fillRect(area.getX(), area.getY(), barWidth, area.getHeight());
        }

        g.setColour(juce::Colour(0xff404040));
        g.drawRect(area);

        g.setColour(juce::Colour(0xff808080));
        for (int i = 1; i < 4; ++i)
        {
            const int x = area.getX() + (area.getWidth() * i) / 4;
            g.drawVerticalLine(x, (float) area.getY(), (float) area.getBottom());
        }
    }

private:
    void timerCallback() override
    {
        const float p = peakRef.exchange(0.0f);
        displayedLevel = std::max(p, displayedLevel * 0.88f);
        repaint();
    }

    std::atomic<float>& peakRef;
    float displayedLevel = 0.0f;
};
