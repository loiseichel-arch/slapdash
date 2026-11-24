#pragma once

#include "PluginProcessor.h"
#include "ui/SketchLookAndFeel.h"
#include "ui/Components.h"
#include "ui/BandStrip.h"
#include "ui/BandPanel.h"
#include "ui/PresetBar.h"

class SlapdashEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit SlapdashEditor (SlapdashProcessor&);
    ~SlapdashEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateLayout (bool force);
    int  windowWidth (int bands) const;

    void showAddMenu();
    void addBand (bool duplicate);
    void removeBand (int index);
    void copyBand (int from, int to);
    void resetBand (int index);
    void enterLayer();

    static constexpr int H = 716, pad = 28, panelGap = 16, addW = 64, voltW = 104;
    // header: logo, on / Link / Split-Layer, then the presets row under them (since 0.4)
    static constexpr int headerY = 18, presetY = 76, stripY = 122, stripH = 118, modY = 254, modH = 330, masterY = 598, masterH = 100;

    SlapdashProcessor& proc;
    SketchLookAndFeel laf;
    juce::SharedResourcePointer<sketch::Fonts> fonts;
    juce::TooltipWindow tooltips { this, 600 };

    ResetToggle onSwitch { "on", true };
    PresetBar presetBar;
    PillToggle linkButton { "Link", false };
    ChoiceSelector modeSelector;
    BandStrip strip;
    juce::OwnedArray<BandPanel> panels;
    SketchActionButton addButton { SketchActionButton::Kind::addBand };

    SketchFader volt { "slapdash", true };
    ChoiceSelector slapMode;

    SketchKnob input { "Input", 42 }, lowCut { "Low cut", 42 }, highCut { "High cut", 42 },
               monoMaker { "Mono maker", 42 }, mix { "Mix", 42 }, output { "Output", 42 };
    PillToggle autoGain { "Auto gain", true }, speakerSim { "Speaker sim", false };

    int shownBands = -1;
    bool shownLayer = false, shownMono = false, shownOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlapdashEditor)
};
