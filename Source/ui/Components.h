// Components.h
// Knobs, faders, switches and small buttons of Slapdash, all hand-drawn.
// Every control goes back to its default value on alt-click.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "Sketch.h"

inline void setParam (juce::RangedAudioParameter* p, float value)
{
    if (p == nullptr) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (value));
    p->endChangeGesture();
}

inline float paramValue (juce::RangedAudioParameter* p)
{
    return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f;
}

inline float paramDefault (juce::RangedAudioParameter* p)
{
    return p != nullptr ? p->convertFrom0to1 (p->getDefaultValue()) : 0.0f;
}

// ============================================================================
// A knob with its label and value underneath, attached to a parameter.
class SketchKnob : public juce::Component
{
public:
    SketchKnob (const juce::String& labelText, int knobDiameter)
        : label (labelText), diameter (knobDiameter)
    {
        slider.setName (labelText);
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (220);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
    }

    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
    {
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
        if (auto* p = state.getParameter (paramId))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()),
                                              juce::ModifierKeys::altModifier);
        slider.setTooltip (label.toLowerCase() + " (alt-click to reset)");
    }

    void setInactive (bool shouldBeInactive)
    {
        if (inactive == shouldBeInactive) return;
        inactive = shouldBeInactive;
        slider.setEnabled (! inactive);
        setAlpha (inactive ? 0.35f : 1.0f);
        slider.setTooltip (inactive ? "not available in mono" : label.toLowerCase() + " (alt-click to reset)");
    }

    void resized() override
    {
        auto b = getLocalBounds();
        slider.setBounds (b.removeFromTop (diameter + 6).withSizeKeepingCentre (diameter + 6, diameter + 6));
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        b.removeFromTop ((float) diameter + 6.0f);

        g.setColour (sketch::colour::ink);
        g.setFont (fonts->hand (diameter >= 50 ? 16.0f : 14.0f));
        g.drawText (label, b.removeFromTop (17.0f), juce::Justification::centred, false);

        g.setColour (sketch::colour::soft);
        g.setFont (fonts->hand (13.0f));
        g.drawText (slider.getTextFromValue (slider.getValue()), b.removeFromTop (15.0f), juce::Justification::centred, false);
    }

    static int heightFor (int d) { return d + 6 + 17 + 15; }

    juce::Slider slider;

private:
    juce::String label;
    int diameter;
    bool inactive = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::SharedResourcePointer<sketch::Fonts> fonts;
};

// ============================================================================
// Vertical fader attached to a parameter (band Gain, Slapdash).
class SketchFader : public juce::Component
{
public:
    explicit SketchFader (const juce::String& name, bool darkColumn = false)
    {
        slider.setName (name);
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.getProperties().set ("dark", darkColumn);
        slider.setMouseDragSensitivity (200);
        addAndMakeVisible (slider);
    }

    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
    {
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
        if (auto* p = state.getParameter (paramId))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()),
                                              juce::ModifierKeys::altModifier);
        slider.setTooltip (slider.getName().toLowerCase() + " (alt-click to reset)");
    }

    void resized() override { slider.setBounds (getLocalBounds()); }

    juce::String valueText() { return slider.getTextFromValue (slider.getValue()); }

    juce::Slider slider;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ============================================================================
// On/off button that goes back to its default state on alt-click.
class ResetToggle : public juce::ToggleButton
{
public:
    explicit ResetToggle (const juce::String& buttonText = {}, bool defaultOn = false)
        : juce::ToggleButton (buttonText), defaultState (defaultOn) {}

    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
    {
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramId, *this);
        if (auto* p = state.getParameter (paramId))
            defaultState = p->getDefaultValue() > 0.5f;
    }

protected:
    void clicked (const juce::ModifierKeys& mods) override
    {
        if (mods.isAltDown())
            setToggleState (defaultState, juce::sendNotificationSync);
        juce::ToggleButton::clicked (mods);
    }

    bool defaultState;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    juce::SharedResourcePointer<sketch::Fonts> fonts;
};

// Pill with its state written in it: "Auto gain · ON".
class PillToggle : public ResetToggle
{
public:
    using ResetToggle::ResetToggle;

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const bool on = getToggleState();
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        sketch::box (g, r, sketch::seedOf (getButtonText()),
                     on ? sketch::colour::accent : (over ? sketch::colour::paper : sketch::colour::knob), 1.8f);
        g.setColour (on ? sketch::colour::knob : sketch::colour::ink);
        g.setFont (fonts->hand (15.0f));
        g.drawText (getButtonText() + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + (on ? "ON" : "OFF"),
                    r, juce::Justification::centred, false);
    }
};

// Small round S / M / B button.
class CircleToggle : public ResetToggle
{
public:
    CircleToggle (const juce::String& letter, const juce::String& tip) : ResetToggle (letter, false)
    {
        setTooltip (tip);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const bool on = getToggleState();
        const auto r = getLocalBounds().toFloat().reduced (2.0f);
        const auto circle = sketch::ellipse (r, sketch::seedOf (getButtonText() + getTooltip()), 0.5f);
        g.setColour (on ? sketch::colour::accent : (over ? sketch::colour::tint : sketch::colour::knob));
        g.fillPath (circle);
        sketch::stroke (g, circle, 1.7f);
        g.setColour (on ? sketch::colour::knob : sketch::colour::ink);
        g.setFont (fonts->hand (14.0f));
        g.drawText (getButtonText(), r, juce::Justification::centred, false);
    }
};

// ============================================================================
// Plain hand-drawn buttons: "+ band" and the round "−".
class SketchActionButton : public juce::Button
{
public:
    enum class Kind { addBand, remove };

    SketchActionButton (Kind k) : juce::Button (k == Kind::addBand ? "add band" : "remove band"), kind (k)
    {
        setTooltip (getName());
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const auto r = getLocalBounds().toFloat().reduced (2.0f);
        const float dashes[] = { 5.0f, 4.0f };
        juce::Path outline, dashed;

        if (kind == Kind::remove) outline = sketch::ellipse (r, 77, 0.4f);
        else                      outline = sketch::rectStroke (r, 78, 0.8f, 1.0f);

        if (over)
        {
            g.setColour (sketch::colour::tint);
            if (kind == Kind::remove) g.fillEllipse (r); else g.fillRoundedRectangle (r, 4.0f);
        }
        juce::PathStrokeType (1.8f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (sketch::colour::ink);
        g.fillPath (dashed);

        if (kind == Kind::remove)
        {
            sketch::stroke (g, sketch::line ({ r.getCentreX() - 5.0f, r.getCentreY() }, { r.getCentreX() + 5.0f, r.getCentreY() }, 5, 0.2f), 2.0f);
        }
        else
        {
            g.setFont (fonts->title (44.0f));
            g.drawText ("+", juce::Rectangle<float> (r.getWidth(), 46.0f).withCentre ({ r.getCentreX(), r.getCentreY() - 14.0f }),
                        juce::Justification::centred, false);
            g.setFont (fonts->hand (15.0f));
            g.drawText ("band", juce::Rectangle<float> (r.getWidth(), 20.0f).withCentre ({ r.getCentreX(), r.getCentreY() + 18.0f }),
                        juce::Justification::centred, false);
        }
    }

private:
    Kind kind;
    juce::SharedResourcePointer<sketch::Fonts> fonts;
};

// ============================================================================
// A row of pills choosing one value of a parameter (band Type, Split/Layer).
// Alt-click on any pill: back to the default value.
class ChoiceSelector : public juce::Component
{
public:
    ChoiceSelector (juce::RangedAudioParameter& p, const juce::StringArray& labels, bool joined = false)
        : param (p), segmented (joined)
    {
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* b = buttons.add (new Pill (labels[i], *this, i));
            b->onClick = [this, i]
            {
                const bool alt = juce::ModifierKeys::currentModifiers.isAltDown();
                const int target = alt ? juce::roundToInt (param.convertFrom0to1 (param.getDefaultValue())) : i;
                if (beforeChange) beforeChange (target);
                attachment->setValueAsCompleteGesture ((float) target);
            };
            addAndMakeVisible (b);
        }
        attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { select (juce::roundToInt (v)); });
        attachment->sendInitialUpdate();
    }

    // Called with the index about to be chosen (used by Split/Layer).
    std::function<void (int)> beforeChange;

    // Natural width of the pills with this font.
    int idealWidth() const
    {
        int w = 0;
        for (auto* b : buttons) w += pillWidth (b->getName()) + (segmented ? 0 : gap);
        return w - (segmented ? 0 : gap);
    }

    void resized() override
    {
        const float scale = juce::jmin (1.0f, (float) getWidth() / (float) juce::jmax (1, idealWidth()));
        int x = juce::jmax (0, (getWidth() - (int) ((float) idealWidth() * scale)) / 2);
        for (auto* b : buttons)
        {
            const int w = (int) ((float) pillWidth (b->getName()) * scale);
            b->setBounds (x, 0, w, getHeight());
            x += w + (segmented ? 0 : gap);
        }
    }

private:
    static constexpr int gap = 5;

    int pillWidth (const juce::String& t) const
    {
        return (int) juce::GlyphArrangement::getStringWidth (fonts->hand (15.0f), t) + 22;
    }

    struct Pill : juce::Button
    {
        Pill (const juce::String& n, ChoiceSelector& o, int i) : juce::Button (n), owner (o), index (i)
        {
            setTooltip (n.toLowerCase() + " (alt-click to reset)");
        }

        void paintButton (juce::Graphics& g, bool over, bool) override
        {
            const auto r = getLocalBounds().toFloat().reduced (owner.segmented ? 1.0f : 1.5f, 1.5f);
            sketch::box (g, r, sketch::seedOf (getName()) + (uint32_t) index,
                         selected ? sketch::colour::accent : (over ? sketch::colour::paper : sketch::colour::knob), 1.8f);
            g.setColour (selected ? sketch::colour::knob : sketch::colour::ink);
            g.setFont (owner.fonts->hand (15.0f));
            g.drawText (getName(), r, juce::Justification::centred, false);
        }

        ChoiceSelector& owner;
        int index;
        bool selected = false;
    };

    void select (int index)
    {
        for (int i = 0; i < buttons.size(); ++i)
        {
            auto* b = static_cast<Pill*> (buttons[i]);
            b->selected = (i == index);
            b->repaint();
        }
    }

    juce::RangedAudioParameter& param;
    bool segmented;
    juce::OwnedArray<juce::Button> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::SharedResourcePointer<sketch::Fonts> fonts;
};
