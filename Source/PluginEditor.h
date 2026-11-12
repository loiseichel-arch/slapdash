#pragma once

#include "PluginProcessor.h"
#include "ui/SketchLookAndFeel.h"
#include "ui/Components.h"
#include "ui/BandStrip.h"
#include "ui/BandPanel.h"

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

    void addBand();
    void removeBand (int index);
    void copyBand (int from, int to);
    void resetBand (int index);
    void enterLayer();

    static constexpr int H = 680, pad = 28, panelGap = 16, addW = 64, voltW = 104;
    static constexpr int headerY = 18, stripY = 86, stripH = 118, modY = 218, modH = 330, masterY = 562, masterH = 100;

    SlapdashProcessor& proc;
    SketchLookAndFeel laf;
    juce::SharedResourcePointer<sketch::Fonts> fonts;
    juce::TooltipWindow tooltips { this, 600 };

    ChoiceSelector modeSelector;
    BandStrip strip;
    juce::OwnedArray<BandPanel> panels;
    SketchActionButton addButton { SketchActionButton::Kind::addBand };

    SketchFader volt { "slapdash", true };

    SketchKnob input { "Input", 42 }, lowCut { "Low cut", 42 }, highCut { "High cut", 42 },
               monoMaker { "Mono maker", 42 }, mix { "Mix", 42 }, output { "Output", 42 };
    PillToggle autoGain { "Auto gain", true }, speakerSim { "Speaker sim", false };

    int shownBands = -1;
    bool shownLayer = false, shownMono = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlapdashEditor)
};
