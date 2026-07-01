#pragma once

#include <JuceHeader.h>
#include "PluginHost.h"

class PluginChooserDialog
    : public juce::DocumentWindow,
      public juce::ListBoxModel,
      public juce::ChangeListener,
      public juce::Timer
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

        startTimer(150);
        enterModalState(false);
    }

    ~PluginChooserDialog() override
    {
        stopTimer();
        pluginHost.getKnownList().removeChangeListener(this);
    }

    void closeButtonPressed() override
    {
        if (isCurrentlyModal()) exitModalState(0);
        juce::MessageManager::getInstance()->callAsync([this] { delete this; });
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

    void timerCallback() override
    {
        if (pluginHost.isScanning())
        {
            const int n = pluginHost.getScanFoundCount();
            statusLabel.setText("Scanning... (" + juce::String(n) + " found)",
                                juce::dontSendNotification);
            rescanButton.setEnabled(false);
        }
        else
        {
            rescanButton.setEnabled(true);
            updateRowCountText();
        }
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
        pluginHost.scanForPluginsAsync();
    }

    void updateRowCountText()
    {
        const int n = getNumRows();
        if (n == 0)
            statusLabel.setText("No plugins found. Click Rescan.",
                                juce::dontSendNotification);
        else
            statusLabel.setText(juce::String(n) + " plugin" + (n == 1 ? "" : "s") + " available. Double-click to load.",
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
    using OnClosed = std::function<void()>;

    static PluginEditorWindow* open(juce::AudioPluginInstance& p, OnClosed cb)
    {
        auto* editor = p.createEditorAndMakeActive();
        if (editor == nullptr) return nullptr;

        auto* w = new PluginEditorWindow(p, std::move(cb));
        w->setContentOwned(editor, true);
        w->setUsingNativeTitleBar(true);
        w->setResizable(true, false);
        w->centreWithSize(w->getWidth(), w->getHeight());
        w->setVisible(true);
        w->toFront(true);
        return w;
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
    PluginEditorWindow(juce::AudioPluginInstance& p, OnClosed cb)
        : juce::DocumentWindow(p.getName(),
                               juce::Desktop::getInstance().getDefaultLookAndFeel()
                                   .findColour(juce::ResizableWindow::backgroundColourId),
                               juce::DocumentWindow::allButtons),
          plugin(p), onClosed(std::move(cb)) {}

    juce::AudioPluginInstance& plugin;
    OnClosed onClosed;
    std::atomic<bool> closing{false};
};
