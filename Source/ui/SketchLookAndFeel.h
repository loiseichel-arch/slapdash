#pragma once

#include "Sketch.h"

// Draws the knobs, faders and on/off switches in the hand-drawn style.
//
// A knob can show a live value instead of its own position (used when the
// envelope moves Rate or Depth): set the slider property "overridePos"
// (0..1) and it is drawn there.
class SketchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SketchLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool highlighted, bool down) override;

    // Vertical faders (band Gain, Slapdash). Slider property "dark" = drawn
    // on the dark Slapdash column.
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    juce::Font getPopupMenuFont() override { return fonts->hand (18.0f); }

    juce::SharedResourcePointer<sketch::Fonts> fonts;
};
