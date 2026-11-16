// BandPanel.h
// One distortion module: Type, Drive / Sag / Mix, the More switch, and on the
// right Pan and the band Gain fader (linked to the gain line of the strip).

#pragma once

#include "Components.h"
#include "../Parameters.h"

class BandPanel : public juce::Component
{
public:
    BandPanel (juce::AudioProcessorValueTreeState& state, int bandIndex)
        : index (bandIndex),
          types (*state.getParameter (params::bandId (bandIndex, params::band::type)), params::typeNames())
    {
        using namespace params;
        drive.attach (state, bandId (index, band::drive));
        sag.attach   (state, bandId (index, band::sag));
        more.attach  (state, bandId (index, band::more));
        more.setTooltip ("more: another full course of drive (alt-click to reset)");
        mix.attach   (state, bandId (index, band::mix));
        pan.attach   (state, bandId (index, band::pan));
        gain.attach  (state, bandId (index, band::gain));
        solo.attach  (state, bandId (index, band::solo));
        mute.attach  (state, bandId (index, band::mute));
        bypass.attach (state, bandId (index, band::bypass));

        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 &types, &drive, &sag, &mix, &more, &pan, &gain })
            addAndMakeVisible (c);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &solo, &mute, &bypass, &removeButton })
            addChildComponent (c);

        removeButton.onClick = [this] { if (onRemove) onRemove (index); };
    }

    std::function<void (int)> onRemove;

    // multi = more than one band (or Layer mode): shows S / M / B and remove.
    void setMode (bool multi, bool monoBus)
    {
        if (multi != showMulti)
        {
            showMulti = multi;
            for (juce::Component* c : std::initializer_list<juce::Component*> { &solo, &mute, &bypass, &removeButton })
                c->setVisible (multi);
            repaint();
        }
        pan.setInactive (monoBus);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace sketch;
        const auto r = getLocalBounds().toFloat().reduced (2.0f);
        box (g, r, 900 + (uint32_t) index, colour::knob, 2.2f);

        g.setColour (colour::ink);
        g.setFont (fonts->title (26.0f));
        g.drawText (showMulti ? "Band " + juce::String (index + 1) : juce::String ("Drive"),
                    juce::Rectangle<float> (16.0f, 8.0f, 160.0f, 32.0f), juce::Justification::centredLeft, false);

        // Dotted separator before Pan / Gain.
        const float sx = (float) rightColumn().getX() - 6.0f;
        juce::Path sep, dashed;
        sep.startNewSubPath (sx, 90.0f); sep.lineTo (sx, (float) getHeight() - 14.0f);
        const float dashes[] = { 3.0f, 4.0f };
        juce::PathStrokeType (1.6f).createDashedStroke (dashed, sep, dashes, 2);
        g.setColour (colour::ink.withAlpha (0.4f));
        g.fillPath (dashed);

        // Gain label and value next to the fader.
        const auto f = faderBounds();
        g.setColour (colour::ink);
        g.setFont (fonts->hand (15.0f));
        g.drawText ("Gain", juce::Rectangle<float> ((float) f.getRight() + 2.0f, (float) f.getCentreY() - 18.0f, 44.0f, 18.0f),
                    juce::Justification::centredLeft, false);
        g.setColour (colour::soft);
        g.setFont (fonts->hand (13.0f));
        g.drawText (gain.valueText(), juce::Rectangle<float> ((float) f.getRight() + 2.0f, (float) f.getCentreY(), 48.0f, 16.0f),
                    juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        // Header: S M B and remove.
        int x = getWidth() - 14 - 30;
        for (auto* b : std::initializer_list<juce::Component*> { &removeButton, &bypass, &mute, &solo })
        {
            b->setBounds (x, 10, 30, 30);
            x -= 33;
        }

        types.setBounds (12, 48, getWidth() - 24, 28);

        // Drive, Sag, Mix in a row, the More switch under Drive.
        const int colW = 70, rowH = SketchKnob::heightFor (54) + 4;
        const int gridW = colW * 3;
        const int leftW = rightColumn().getX() - 8 - 12;
        const int gx = 12 + juce::jmax (0, (leftW - gridW) / 2);
        const int gy = 100;
        SketchKnob* knobs[] = { &drive, &sag, &mix };
        for (int i = 0; i < 3; ++i)
            knobs[i]->setBounds (gx + i * colW, gy, colW, rowH);
        more.setBounds (juce::jmax (12, gx + colW / 2 - 48), gy + rowH + 14, 96, 34);   // under Drive

        const auto rc = rightColumn();
        pan.setBounds (rc.getX(), 90, rc.getWidth(), SketchKnob::heightFor (40));
        gain.setBounds (faderBounds());
    }

    void refreshValues() { repaint (rightColumn().withTrimmedTop (150)); }

    static constexpr int preferredWidth = 330;

private:
    juce::Rectangle<int> rightColumn() const { return { getWidth() - 12 - 78, 90, 78, getHeight() - 104 }; }
    juce::Rectangle<int> faderBounds() const
    {
        const auto rc = rightColumn();
        const int top = 90 + SketchKnob::heightFor (40) + 8;
        return { rc.getX() + 4, top, 26, getHeight() - 16 - top };
    }

    int index;
    ChoiceSelector types;
    SketchKnob drive { "Drive", 54 }, sag { "Sag", 54 }, mix { "Mix", 54 };
    PillToggle more { "More", false };
    SketchKnob pan { "Pan", 40 };
    SketchFader gain { "band gain" };
    CircleToggle solo { "S", "solo" }, mute { "M", "mute" }, bypass { "B", "bypass" };
    SketchActionButton removeButton { SketchActionButton::Kind::remove };
    bool showMulti = false;
    juce::SharedResourcePointer<sketch::Fonts> fonts;
};
