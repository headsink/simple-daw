#pragma once

#include <JuceHeader.h>
#include "PluginHost.h"

class PluginChooserDialog
    : public juce::DocumentWindow,
      public juce::ListBoxModel,
      public juce::ChangeListener
{
public:
    using OnChosen = std::function<void(const juce::PluginDescription&)>;

    PluginChooserDialog(PluginHost& host, OnChosen cb)
        : juce::DocumentWindow("Choose VST3 Plugin",
                               juce::Desktop::getInstance().getDefaultLookAndFeel()
                                   .findColour(juce::ResizableWindow::backgroundColourId),
                               juce::DocumentWindow::closeButton),
          pluginHost(host),
          onChosen(std::move(cb))
    {
        pluginHost.getKnownList().addChangeListener(this);
        setUsingNativeTitleBar(true);
        setResizable(true, false);
        setSize(460, 360);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);

        addAndMakeVisible(listBox);
        listBox.setRowHeight(24);
        listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1f1f1f));
        listBox.updateContent();

        addAndMakeVisible(rescanButton);
        rescanButton.setButtonText("Rescan");
        rescanButton.onClick = [this] { triggerRescan(); };

        addAndMakeVisible(statusLabel);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        statusLabel.setJustificationType(juce::Justification::centredLeft);
        updateRowCountText();

        enterModalState(false);
    }

    ~PluginChooserDialog() override
    {
        pluginHost.getKnownList().removeChangeListener(this);
    }

    void closeButtonPressed() override
    {
        if (isCurrentlyModal()) exitModalState(0);
        delete this;
    }

    int getNumRows() override { return (int) pluginHost.getKnownList().getTypes().size(); }

    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool sel) override
    {
        const auto types = pluginHost.getKnownList().getTypes();
        if (row < 0 || row >= types.size()) return;
        if (sel)
        {
            g.setColour(juce::Colour(0xff3050a0));
            g.fillRect(0, 0, w, h);
        }
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(14.0f));
        g.drawText(types[row].name, 8, 0, w - 16, h, juce::Justification::centredLeft);
    }

    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        const auto types = pluginHost.getKnownList().getTypes();
        if (row >= 0 && row < types.size() && onChosen)
            onChosen(types[row]);
        closeButtonPressed();
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        listBox.updateContent();
        updateRowCountText();
    }

    void resized() override
    {
        juce::DocumentWindow::resized();
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(28);
        auto bottom = area.removeFromBottom(24);
        rescanButton.setBounds(bottom.removeFromLeft(80));
        bottom.removeFromLeft(8);
        statusLabel.setBounds(bottom);
        listBox.setBounds(area);
    }

private:
    using juce::Component::addAndMakeVisible;

    void triggerRescan()
    {
        statusLabel.setText("Scanning... (this may take a while)", juce::dontSendNotification);
        rescanButton.setEnabled(false);
        juce::Timer::callAfterDelay(10, [this]
        {
            pluginHost.scanForPlugins();
            rescanButton.setEnabled(true);
            updateRowCountText();
        });
    }

    void updateRowCountText()
    {
        const int n = getNumRows();
        statusLabel.setText(n == 0 ? "No plugins found. Click Rescan."
                                   : juce::String(n) + " plugin" + (n == 1 ? "" : "s") + " available. Double-click to load.",
                            juce::dontSendNotification);
    }

    PluginHost& pluginHost;
    OnChosen onChosen;
    juce::ListBox listBox {"Plugins", this};
    juce::TextButton rescanButton;
    juce::Label statusLabel;
};

class PluginEditorWindow : public juce::DocumentWindow
{
public:
    explicit PluginEditorWindow(juce::AudioPluginInstance& p)
        : juce::DocumentWindow(p.getName(),
                               juce::Desktop::getInstance().getDefaultLookAndFeel()
                                   .findColour(juce::ResizableWindow::backgroundColourId),
                               juce::DocumentWindow::allButtons),
          plugin(p)
    {
        setUsingNativeTitleBar(true);
        auto* editor = plugin.createEditorAndMakeActive();
        if (editor == nullptr)
        {
            delete this;
            return;
        }
        setContentOwned(editor, true);
        setResizable(true, false);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
        toFront(true);
    }

    void closeButtonPressed() override { delete this; }

private:
    juce::AudioPluginInstance& plugin;
};
