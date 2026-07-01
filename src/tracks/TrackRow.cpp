#include "TrackRow.h"
#include "../plugin/PluginWindows.h"

TrackRow::~TrackRow()
{
    *alive = false;
    if (editorWindow != nullptr)
    {
        editorWindow->closeButtonPressed();
        editorWindow = nullptr;
    }
}

TrackRow::TrackRow(AudioTrack& t, PluginHost& host,
                   std::function<void(TrackRow*)> removeCb,
                   std::function<void()> layoutCb,
                   std::function<void(TrackRow*, TrackRow*, bool)> reorderCb)
    : track(t), pluginHost(host), onRemove(std::move(removeCb)),
      onLayoutChanged(std::move(layoutCb)),
      onReorderRequested(std::move(reorderCb)),
      trackMeter(t.getPeakRef())
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

    addAndMakeVisible(timeLabel);
    timeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    timeLabel.setJustificationType(juce::Justification::centred);
    timeLabel.setText("0:00 / 0:00", juce::dontSendNotification);

    addAndMakeVisible(trackMeter);

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

    addAndMakeVisible(panSlider);
    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setValue(0.0);
    panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    panSlider.setTextBoxIsEditable(false);
    panSlider.onValueChange = [this]
    {
        track.setPan((float)panSlider.getValue());
    };

    addAndMakeVisible(muteButton);
    muteButton.setClickingTogglesState(true);
    muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffaa3030));
    muteButton.onClick = [this] { track.setMute(muteButton.getToggleState()); };

    addAndMakeVisible(soloButton);
    soloButton.setClickingTogglesState(true);
    soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff3060a0));
    soloButton.onClick = [this] { track.setSolo(soloButton.getToggleState()); };

    addAndMakeVisible(loopButton);
    loopButton.setClickingTogglesState(true);
    loopButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff308030));
    loopButton.onClick = [this] { track.getSource().setLooping(loopButton.getToggleState()); };

    addAndMakeVisible(abButton);
    abButton.setClickingTogglesState(true);
    abButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff505050));
    abButton.onClick = [this] { toggleAbPanel(); };

    addAndMakeVisible(setAButton);
    setAButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff305030));
    setAButton.onClick = [this]
    {
        track.setLoopStartFromPlayhead();
        refreshAbLabels();
    };
    setAButton.setVisible(false);

    addAndMakeVisible(setBButton);
    setBButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff503030));
    setBButton.onClick = [this]
    {
        track.setLoopEndFromPlayhead();
        refreshAbLabels();
    };
    setBButton.setVisible(false);

    addAndMakeVisible(clearAbButton);
    clearAbButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff404040));
    clearAbButton.onClick = [this]
    {
        track.clearLoopRegion();
        refreshAbLabels();
    };
    clearAbButton.setVisible(false);

    addAndMakeVisible(startSlider);
    startSlider.setRange(0.0, 1.0, 1.0);
    startSlider.setValue(0.0);
    startSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    startSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    startSlider.onValueChange = [this]
    {
        const int total = track.getSource().getNumSamples();
        if (total <= 0) return;
        const int s = (int) startSlider.getValue();
        track.setLoopStart(s);
        if (track.getLoopEnd() < s) track.setLoopEnd(s);
        refreshAbLabels();
    };
    startSlider.setVisible(false);

    addAndMakeVisible(endSlider);
    endSlider.setRange(0.0, 1.0, 1.0);
    endSlider.setValue(1.0);
    endSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    endSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    endSlider.onValueChange = [this]
    {
        const int total = track.getSource().getNumSamples();
        if (total <= 0) return;
        const int e = (int) endSlider.getValue();
        track.setLoopEnd(e);
        if (track.getLoopStart() > e) track.setLoopStart(e);
        refreshAbLabels();
    };
    endSlider.setVisible(false);

    addAndMakeVisible(startValueLabel);
    startValueLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    startValueLabel.setJustificationType(juce::Justification::centredLeft);
    startValueLabel.setText("A: 0:00.0", juce::dontSendNotification);
    startValueLabel.setVisible(false);

    addAndMakeVisible(endValueLabel);
    endValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff8080));
    endValueLabel.setJustificationType(juce::Justification::centredLeft);
    endValueLabel.setText("B: 0:00.0", juce::dontSendNotification);
    endValueLabel.setVisible(false);

    addAndMakeVisible(abStatusLabel);
    abStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    abStatusLabel.setJustificationType(juce::Justification::centredLeft);
    abStatusLabel.setText("", juce::dontSendNotification);
    abStatusLabel.setVisible(false);

    addAndMakeVisible(loadPluginButton);
    loadPluginButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff504020));
    loadPluginButton.onClick = [this] { openPluginChooser(); };

    addAndMakeVisible(bypassButton);
    bypassButton.setClickingTogglesState(true);
    bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff806020));
    bypassButton.onClick = [this] { track.setPluginBypass(bypassButton.getToggleState()); };
    bypassButton.setEnabled(false);

    addAndMakeVisible(editPluginButton);
    editPluginButton.onClick = [this] { showPluginEditor(); };
    editPluginButton.setEnabled(false);

    addAndMakeVisible(pluginLabel);
    pluginLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    pluginLabel.setText("(no plugin)", juce::dontSendNotification);

    addAndMakeVisible(removeButton);
    removeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff803030));
    removeButton.onClick = [this]
    {
        if (onRemove) onRemove(this);
    };

    refreshTimeLabel();
    refreshPluginLabel();
    updateButtons();

    dragStarter = std::make_unique<TrackRowDragStarter>(this);
    addMouseListener(dragStarter.get(), false);
    nameLabel.addMouseListener(dragStarter.get(), false);
    timeLabel.addMouseListener(dragStarter.get(), false);
    pluginLabel.addMouseListener(dragStarter.get(), false);
}

void TrackRow::paint(juce::Graphics& g)
{
    g.setColour(dragHighlight ? juce::Colour(0xff3050a0) : juce::Colour(0xff252525));
    g.fillRect(getLocalBounds());
    g.setColour(juce::Colour(0xff404040));
    g.drawLine(0.0f, 0.0f, (float)getWidth(), 0.0f, 1.0f);
    g.drawLine(0.0f, (float)getHeight(), (float)getWidth(), (float)getHeight(), 1.0f);

    if (dropHoverActive)
    {
        g.setColour(juce::Colours::yellow);
        const float y = dropInsertAbove ? 0.0f : (float) getHeight();
        g.drawLine(0.0f, y, (float) getWidth(), y, 2.0f);
    }
}

void TrackRowDragStarter::mouseDown(const juce::MouseEvent& e)
{
    if (! e.mods.isLeftButtonDown() || row == nullptr) return;
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(row))
    {
        static const juce::String sourceDesc("TrackRowReorder");
        container->startDragging(sourceDesc, row);
    }
}

bool TrackRow::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details)
{
    return details.description == "TrackRowReorder" && details.sourceComponent.get() != this;
}

void TrackRow::itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details)
{
    dropInsertAbove = details.localPosition.y < (float) getHeight() * 0.5f;
    dropHoverActive = true;
    repaint();
}

void TrackRow::itemDragMove(const juce::DragAndDropTarget::SourceDetails& details)
{
    const bool above = details.localPosition.y < (float) getHeight() * 0.5f;
    if (above != dropInsertAbove)
    {
        dropInsertAbove = above;
        repaint();
    }
}

void TrackRow::itemDragExit(const juce::DragAndDropTarget::SourceDetails&)
{
    dropHoverActive = false;
    repaint();
}

void TrackRow::itemDropped(const juce::DragAndDropTarget::SourceDetails& details)
{
    dropHoverActive = false;
    repaint();

    auto* source = dynamic_cast<TrackRow*>(details.sourceComponent.get());
    if (source == nullptr || source == this) return;

    if (onReorderRequested)
        onReorderRequested(source, this, dropInsertAbove);
}

static bool isAudioFile(const juce::String& path)
{
    static const char* exts[] = { ".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg", nullptr };
    for (int i = 0; exts[i] != nullptr; ++i)
        if (path.endsWithIgnoreCase(exts[i])) return true;
    return false;
}

bool TrackRow::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
        if (isAudioFile(f)) return true;
    return false;
}

void TrackRow::fileDragEnter(const juce::StringArray& /*files*/, int /*x*/, int /*y*/)
{
    dragHighlight = true;
    repaint();
}

void TrackRow::fileDragExit(const juce::StringArray& /*files*/)
{
    dragHighlight = false;
    repaint();
}

void TrackRow::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    dragHighlight = false;
    repaint();
    for (const auto& f : files)
    {
        if (! isAudioFile(f)) continue;
        juce::File file(f);
        if (! file.existsAsFile()) continue;
        track.loadFileAsync(file,
            [this, aliveFlag = alive](bool ok, const juce::String& err)
            {
                if (! *aliveFlag) return;
                if (ok)
                    nameLabel.setText(track.getSource().getLoadedFileName(), juce::dontSendNotification);
                else
                    nameLabel.setText("(failed: " + err + ")", juce::dontSendNotification);
                refreshTimeLabel();
                updateButtons();
            });
        break;
    }
}

void TrackRow::resized()
{
    auto area = getLocalBounds().reduced(6);
    const int nameRowH = 16;
    const int buttonRowH = 26;
    const int pluginRowH = 14;

    nameLabel.setBounds(area.removeFromTop(nameRowH));
    area.removeFromTop(2);

    auto topButtons = area.removeFromTop(buttonRowH);
    layoutTopRow(topButtons);
    area.removeFromTop(2);

    if (abOpen)
    {
        auto abRow = area.removeFromTop(40);
        layoutAbRow(abRow);
        area.removeFromTop(2);
    }

    auto pluginRow = area.removeFromTop(pluginRowH);
    pluginLabel.setBounds(pluginRow);
}

void TrackRow::layoutTopRow(juce::Rectangle<int> area)
{
    const int buttonW = 50;
    const int playW = 50;
    const int stopW = 50;
    const int timeW = 80;
    const int meterW = 50;
    const int removeW = 28;
    const int gainW = 110;
    const int panW = 86;
    const int muteW = 42;
    const int soloW = 42;
    const int loopW = 42;
    const int abW = 36;
    const int pluginW = 44;
    const int bypassW = 50;
    const int editW = 38;

    auto r = area;
    loadButton.setBounds(r.removeFromLeft(buttonW).reduced(2, 3));
    r.removeFromLeft(3);
    playButton.setBounds(r.removeFromLeft(playW).reduced(2, 3));
    r.removeFromLeft(3);
    stopButton.setBounds(r.removeFromLeft(stopW).reduced(2, 3));
    r.removeFromLeft(4);
    timeLabel.setBounds(r.removeFromLeft(timeW).reduced(2, 3));
    r.removeFromLeft(4);
    trackMeter.setBounds(r.removeFromLeft(meterW).reduced(2, 8));
    r.removeFromLeft(5);
    gainSlider.setBounds(r.removeFromLeft(gainW).reduced(0, 5));
    r.removeFromLeft(3);
    panSlider.setBounds(r.removeFromLeft(panW).reduced(0, 5));
    r.removeFromLeft(3);
    muteButton.setBounds(r.removeFromLeft(muteW).reduced(2, 3));
    r.removeFromLeft(2);
    soloButton.setBounds(r.removeFromLeft(soloW).reduced(2, 3));
    r.removeFromLeft(2);
    loopButton.setBounds(r.removeFromLeft(loopW).reduced(2, 3));
    r.removeFromLeft(2);
    abButton.setBounds(r.removeFromLeft(abW).reduced(2, 3));
    r.removeFromLeft(3);
    loadPluginButton.setBounds(r.removeFromLeft(pluginW).reduced(2, 3));
    r.removeFromLeft(2);
    bypassButton.setBounds(r.removeFromLeft(bypassW).reduced(2, 3));
    r.removeFromLeft(2);
    editPluginButton.setBounds(r.removeFromLeft(editW).reduced(2, 3));
    r.removeFromLeft(6);
    removeButton.setBounds(r.removeFromRight(removeW).reduced(2, 3));
}

void TrackRow::layoutAbRow(juce::Rectangle<int> area)
{
    const int setW = 50;
    const int valueW = 80;
    const int sliderH = 16;
    const int clearW = 50;
    const int statusW = 160;

    auto r = area;
    setAButton.setBounds(r.removeFromLeft(setW).reduced(2, 4));
    r.removeFromLeft(3);
    auto aBlock = r.removeFromLeft(valueW);
    startValueLabel.setBounds(aBlock.removeFromTop(14));
    startSlider.setBounds(aBlock.removeFromTop(sliderH).reduced(0, 0));
    r.removeFromLeft(8);
    setBButton.setBounds(r.removeFromLeft(setW).reduced(2, 4));
    r.removeFromLeft(3);
    auto bBlock = r.removeFromLeft(valueW);
    endValueLabel.setBounds(bBlock.removeFromTop(14));
    endSlider.setBounds(bBlock.removeFromTop(sliderH).reduced(0, 0));
    r.removeFromLeft(8);
    clearAbButton.setBounds(r.removeFromLeft(clearW).reduced(2, 4));
    r.removeFromLeft(8);
    abStatusLabel.setBounds(r.removeFromLeft(statusW).reduced(0, 4));
}

void TrackRow::toggleAbPanel()
{
    abOpen = abButton.getToggleState();
    setAButton.setVisible(abOpen);
    setBButton.setVisible(abOpen);
    clearAbButton.setVisible(abOpen);
    startSlider.setVisible(abOpen);
    endSlider.setVisible(abOpen);
    startValueLabel.setVisible(abOpen);
    endValueLabel.setVisible(abOpen);
    abStatusLabel.setVisible(abOpen);

    if (abOpen)
    {
        const int total = track.getSource().getNumSamples();
        startSlider.setRange(0.0, (double) std::max(1, total), 1.0);
        endSlider.setRange(0.0, (double) std::max(1, total), 1.0);
        startSlider.setValue((double) track.getLoopStart(), juce::dontSendNotification);
        endSlider.setValue((double) track.getLoopEnd(), juce::dontSendNotification);
    }
    refreshAbLabels();

    if (onLayoutChanged) onLayoutChanged();
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
            nameLabel.setText("(loading...) " + file.getFileName(), juce::dontSendNotification);
            playButton.setEnabled(false);
            stopButton.setEnabled(false);
            auto aliveFlag = alive;
            track.loadFileAsync(file,
                [this, aliveFlag](bool ok, const juce::String& err)
                {
                    if (! *aliveFlag) return;
                    if (ok)
                    {
                        nameLabel.setText(track.getSource().getLoadedFileName(),
                                          juce::dontSendNotification);
                    }
                    else
                    {
                        nameLabel.setText("(failed: " + err + ")",
                                          juce::dontSendNotification);
                    }
                    updateButtons();
                    refreshTimeLabel();
                    if (abOpen)
                    {
                        const int total = track.getSource().getNumSamples();
                        startSlider.setRange(0.0, (double) std::max(1, total), 1.0);
                        endSlider.setRange(0.0, (double) std::max(1, total), 1.0);
                        startSlider.setValue((double) track.getLoopStart(), juce::dontSendNotification);
                        endSlider.setValue((double) track.getLoopEnd(), juce::dontSendNotification);
                        refreshAbLabels();
                    }
                });
        }
    }
}

void TrackRow::openPluginChooser()
{
    auto aliveFlag = alive;
    auto* dlg = new PluginChooserDialog(pluginHost,
        [this, aliveFlag](const juce::PluginDescription& desc)
        {
            pluginHost.createInstanceAsync(desc,
                [this, aliveFlag, desc](std::unique_ptr<juce::AudioPluginInstance> inst, const juce::String& err)
                {
                    if (! *aliveFlag) return;
                    if (inst)
                    {
                        if (editorWindow != nullptr) { delete editorWindow; editorWindow = nullptr; }
                        auto descCopy = std::make_unique<juce::PluginDescription>(desc);
                        track.setPlugin(std::move(inst), std::move(descCopy));
                        juce::MessageManager::getInstance()->callAsync(
                            [this, aliveFlag] {
                                if (! *aliveFlag) return;
                                refreshPluginLabel();
                                updateButtons();
                            });
                    }
                    else
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Plugin Load Error",
                            err.isEmpty() ? "Unknown error." : err);
                    }
                });
        });
    (void) dlg;
}

void TrackRow::showPluginEditor()
{
    auto* p = track.getPlugin();
    if (p == nullptr) return;
    if (editorWindow != nullptr)
    {
        editorWindow->toFront(true);
        return;
    }
    auto aliveFlag = alive;
    editorWindow = PluginEditorWindow::open(*p,
        [this, aliveFlag]() { if (*aliveFlag) editorWindow = nullptr; });
}

void TrackRow::updateButtons()
{
    const bool loaded = track.getSource().isLoaded();
    playButton.setEnabled(loaded);
    stopButton.setEnabled(loaded);
    playButton.setButtonText(track.getSource().isPlaying() ? "Pause" : "Play");

    const bool hasPlugin = track.hasPlugin();
    bypassButton.setEnabled(hasPlugin);
    editPluginButton.setEnabled(hasPlugin);
    if (!hasPlugin)
    {
        bypassButton.setToggleState(false, juce::dontSendNotification);
        track.setPluginBypass(false);
    }
}

void TrackRow::refreshPluginLabel()
{
    if (track.hasPlugin())
    {
        juce::String txt = track.getPluginName();
        if (track.isPluginBypassed()) txt += "  [bypassed]";
        pluginLabel.setText(txt, juce::dontSendNotification);
        pluginLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    }
    else
    {
        pluginLabel.setText("(no plugin)", juce::dontSendNotification);
        pluginLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    }
}

void TrackRow::refreshTimeLabel()
{
    const auto& src = track.getSource();
    if (!src.isLoaded())
    {
        timeLabel.setText("0:00 / 0:00", juce::dontSendNotification);
        return;
    }
    const double rate = src.getSampleRate() > 0.0 ? src.getSampleRate() : 44100.0;
    const double total = (double)src.getNumSamples() / rate;
    const double current = (double)src.getPlayPosition() / rate;
    timeLabel.setText(formatTime(current) + " / " + formatTime(total),
                      juce::dontSendNotification);
}

juce::String TrackRow::formatTime(double seconds)
{
    if (seconds < 0.0) seconds = 0.0;
    const int total = (int)seconds;
    const int mins = total / 60;
    const int secs = total % 60;
    return juce::String(mins) + ":" + (secs < 10 ? "0" : "") + juce::String(secs);
}

void TrackRow::refreshAbLabels()
{
    if (!abOpen) return;

    const int total = track.getSource().getNumSamples();
    const double rate = track.getSource().getSampleRate() > 0.0
        ? track.getSource().getSampleRate() : 44100.0;

    const int a = track.getLoopStart();
    const int b = track.getLoopEnd();

    startValueLabel.setText("A: " + formatSamples(a, rate), juce::dontSendNotification);
    endValueLabel.setText("B: " + formatSamples(b, rate), juce::dontSendNotification);

    if (total <= 0)
        abStatusLabel.setText("(no file loaded)", juce::dontSendNotification);
    else if (a == 0 && b >= total)
        abStatusLabel.setText("(full file)", juce::dontSendNotification);
    else
        abStatusLabel.setText(juce::String(b - a) + " samples  (" + formatTime((b - a) / rate) + ")",
                              juce::dontSendNotification);
}

juce::String TrackRow::formatSamples(int samples, double sampleRate)
{
    if (sampleRate <= 0.0) sampleRate = 44100.0;
    const double seconds = (double) samples / sampleRate;
    const int mins = (int) seconds / 60;
    const double secs = seconds - (double) mins * 60;
    return juce::String(mins) + ":" + juce::String(secs, 2);
}
