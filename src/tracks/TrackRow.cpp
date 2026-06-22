#include "TrackRow.h"

TrackRow::TrackRow(AudioTrack& t, std::function<void(TrackRow*)> removeCb)
    : track(t), onRemove(std::move(removeCb))
{
    addAndMakeVisible(nameLabel);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    nameLabel.setText("(no file loaded)", juce::dontSendNotification);

    addAndMakeVisible(loadButton);
    loadButton.onClick = [this] { openFile(); };

    addAndMakeVisible(playButton);
    playButton.onClick = [this]
    {
        if (!track.getSource().isLoaded()) return;
        track.togglePlay();
        updateButtons();
    };

    addAndMakeVisible(stopButton);
    stopButton.onClick = [this]
    {
        track.stop();
        updateButtons();
    };

    addAndMakeVisible(gainSlider);
    gainSlider.setRange(0.0, 2.0, 0.01);
    gainSlider.setValue(0.25);
    gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    gainSlider.setTextBoxIsEditable(false);
    gainSlider.onValueChange = [this]
    {
        track.setGain((float)gainSlider.getValue());
    };

    addAndMakeVisible(muteButton);
    muteButton.setClickingTogglesState(true);
    muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffaa3030));
    muteButton.onClick = [this] { track.setMute(muteButton.getToggleState()); };

    addAndMakeVisible(removeButton);
    removeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff803030));
    removeButton.onClick = [this]
    {
        if (onRemove) onRemove(this);
    };

    updateButtons();
}

void TrackRow::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff252525));
    g.fillRect(getLocalBounds());
    g.setColour(juce::Colour(0xff404040));
    g.drawLine(0.0f, 0.0f, (float)getWidth(), 0.0f, 1.0f);
}

void TrackRow::resized()
{
    auto area = getLocalBounds().reduced(6);
    const int buttonW = 60;
    const int playW = 60;
    const int stopW = 60;
    const int muteW = 36;
    const int removeW = 32;
    const int nameW = 220;
    const int gainW = 140;

    nameLabel.setBounds(area.removeFromLeft(nameW));
    area.removeFromLeft(6);

    loadButton.setBounds(area.removeFromLeft(buttonW).reduced(2, 4));
    area.removeFromLeft(4);
    playButton.setBounds(area.removeFromLeft(playW).reduced(2, 4));
    area.removeFromLeft(4);
    stopButton.setBounds(area.removeFromLeft(stopW).reduced(2, 4));
    area.removeFromLeft(10);

    gainSlider.setBounds(area.removeFromLeft(gainW).reduced(0, 8));
    area.removeFromLeft(10);

    muteButton.setBounds(area.removeFromLeft(54).reduced(2, 4));
    area.removeFromLeft(10);

    removeButton.setBounds(area.removeFromRight(removeW).reduced(2, 4));
}

void TrackRow::openFile()
{
    juce::FileChooser chooser("Select a file to load",
                              juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                              "*.wav;*.aif;*.aiff;*.flac;*.mp3");
    if (chooser.browseForFileToOpen())
    {
        auto file = chooser.getResult();
        if (file.existsAsFile())
        {
            track.loadFile(file);
            nameLabel.setText(track.getSource().getLoadedFileName(), juce::dontSendNotification);
            updateButtons();
        }
    }
}

void TrackRow::updateButtons()
{
    const bool loaded = track.getSource().isLoaded();
    playButton.setEnabled(loaded);
    stopButton.setEnabled(loaded);
    playButton.setButtonText(track.getSource().isPlaying() ? "Pause" : "Play");
}
