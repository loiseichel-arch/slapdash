#include "PluginEditor.h"

using namespace sketch;
namespace pid = params::id;

SlapdashEditor::SlapdashEditor (SlapdashProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      modeSelector (*p.apvts.getParameter (pid::layer), { "Split", "Layer" }, true),
      strip (p.apvts)
{
    setLookAndFeel (&laf);
    auto& st = proc.apvts;

    // ---- Header and band strip ----
    modeSelector.beforeChange = [this] (int target)
    {
        if (target == 1 && ! strip.isLayer()) enterLayer();
    };
    addAndMakeVisible (modeSelector);
    addAndMakeVisible (strip);

    // ---- Modules ----
    for (int b = 0; b < params::maxBands; ++b)
    {
        auto* panel = panels.add (new BandPanel (st, b));
        panel->onRemove = [this] (int index) { removeBand (index); };
        addChildComponent (panel);
    }
    addButton.onClick = [this] { addBand(); };
    addChildComponent (addButton);

    volt.attach (st, pid::volt);
    addAndMakeVisible (volt);

    // ---- Master ----
    input.attach     (st, pid::input);
    lowCut.attach    (st, pid::lowCut);
    highCut.attach   (st, pid::highCut);
    monoMaker.attach (st, pid::monoMaker);
    mix.attach       (st, pid::mix);
    output.attach    (st, pid::output);
    for (auto* k : { &input, &lowCut, &highCut, &monoMaker, &mix, &output })
        addAndMakeVisible (k);

    autoGain.attach (st, pid::autoGain);
    speakerSim.attach (st, pid::speakerSim);
    autoGain.setTooltip ("matches the output level to the input level (alt-click to reset)");
    speakerSim.setTooltip ("speaker on its own, no amp (alt-click to reset)");
    addAndMakeVisible (autoGain);
    addAndMakeVisible (speakerSim);

    updateLayout (true);
    startTimerHz (30);
}

SlapdashEditor::~SlapdashEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int SlapdashEditor::windowWidth (int n) const
{
    const int w = 2 * pad + n * BandPanel::preferredWidth + (n - 1) * panelGap
                + (n < params::maxBands ? panelGap + addW : 0) + panelGap + voltW;
    return juce::jmax (740, w);
}

void SlapdashEditor::updateLayout (bool force)
{
    const int n = strip.numBands();
    const bool layer = strip.isLayer();
    const bool mono = proc.isMono();
    if (! force && n == shownBands && layer == shownLayer && mono == shownMono)
        return;

    shownBands = n; shownLayer = layer; shownMono = mono;
    for (int b = 0; b < panels.size(); ++b)
    {
        panels[b]->setVisible (b < n);
        panels[b]->setMode (n > 1 || layer, mono);
    }
    addButton.setVisible (n < params::maxBands);
    monoMaker.setInactive (mono);

    const int w = windowWidth (n);
    if (getWidth() != w || getHeight() != H) setSize (w, H);
    else resized();
    repaint();
}

void SlapdashEditor::timerCallback()
{
    updateLayout (false);
    strip.repaint();
    for (auto* p : panels)
        if (p->isVisible()) p->refreshValues();
}

// ---- Adding and removing bands ----
void SlapdashEditor::addBand()
{
    const int n = strip.numBands();
    if (n >= params::maxBands) return;

    resetBand (n);

    // New split point: halfway (on a log scale) through the top band.
    const auto edges = strip.splitEdges();
    const float lower = edges[(size_t) n - 1];
    const float f = juce::jlimit (lower * 1.2f, 16000.0f, std::sqrt (lower * params::maxHz));
    setParam (proc.apvts.getParameter (pid::xover[(size_t) n - 1]), f);

    setParam (proc.apvts.getParameter (pid::bands), (float) (n + 1));
    updateLayout (true);
}

void SlapdashEditor::removeBand (int index)
{
    const int n = strip.numBands();
    if (n <= 1 || index < 0 || index >= n) return;

    // Split points: the removed band merges with its neighbour.
    std::vector<float> splits;
    for (int k = 0; k < n - 1; ++k) splits.push_back (paramValue (proc.apvts.getParameter (pid::xover[(size_t) k])));
    splits.erase (splits.begin() + (index < n - 1 ? index : index - 1));
    for (size_t k = 0; k < splits.size(); ++k)
        setParam (proc.apvts.getParameter (pid::xover[k]), splits[k]);

    for (int j = index; j < n - 1; ++j) copyBand (j + 1, j);
    resetBand (n - 1);

    setParam (proc.apvts.getParameter (pid::bands), (float) (n - 1));
    updateLayout (true);
}

void SlapdashEditor::copyBand (int from, int to)
{
    for (auto* name : params::band::all)
        setParam (proc.apvts.getParameter (params::bandId (to, name)),
                  paramValue (proc.apvts.getParameter (params::bandId (from, name))));
}

void SlapdashEditor::resetBand (int index)
{
    for (auto* name : params::band::all)
    {
        auto* p = proc.apvts.getParameter (params::bandId (index, name));
        setParam (p, paramDefault (p));
    }
}

// Going to Layer mode: each band starts on the range it had in Split mode.
void SlapdashEditor::enterLayer()
{
    const auto edges = strip.splitEdges();
    for (int b = 0; b < strip.numBands(); ++b)
    {
        setParam (proc.apvts.getParameter (params::bandId (b, params::band::lo)), edges[(size_t) b]);
        setParam (proc.apvts.getParameter (params::bandId (b, params::band::hi)), edges[(size_t) b + 1]);
    }
}

// ---- Drawing ----
void SlapdashEditor::paint (juce::Graphics& g)
{
    const float W = (float) getWidth();
    g.fillAll (colour::paper);
    stroke (g, rectStroke ({ 7.0f, 7.0f, W - 14.0f, (float) H - 14.0f }, 11, 1.0f, 3.0f), 2.4f);

    // Logo: SLAP in ink, DASH in red, slightly tilted.
    {
        const auto font = fonts->title (56.0f);
        const float wSlap = juce::GlyphArrangement::getStringWidth (font, "SLAP");
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (-0.03f, (float) pad, (float) headerY + 30.0f));
        g.setFont (font);
        g.setColour (colour::ink);
        g.drawText ("SLAP", juce::Rectangle<float> ((float) pad, (float) headerY, wSlap + 4.0f, 60.0f), juce::Justification::centredLeft, false);
        g.setColour (colour::accent);
        g.drawText ("DASH", juce::Rectangle<float> ((float) pad + wSlap, (float) headerY, 200.0f, 60.0f), juce::Justification::centredLeft, false);
    }
    g.setColour (colour::soft);
    g.setFont (fonts->hand (16.0f));
    g.drawText ("Planet Gloria", juce::Rectangle<float> (W - (float) pad - 120.0f, (float) headerY + 18.0f, 120.0f, 24.0f),
                juce::Justification::centredRight, false);

    // Slapdash column (dark, like the battery compartment).
    const juce::Rectangle<float> vc (W - (float) pad - (float) voltW, (float) modY, (float) voltW, (float) modH);
    g.setColour (colour::ink);
    g.fillRoundedRectangle (vc.reduced (1.0f), 6.0f);
    stroke (g, rectStroke (vc, 321, 0.8f, 2.0f), 2.2f, colour::accent);
    g.setColour (colour::knob);
    g.setFont (fonts->title (21.0f));
    g.drawText ("Slapdash", vc.withHeight (40.0f).withTrimmedTop (6.0f), juce::Justification::centred, false);
    g.setFont (fonts->hand (13.0f));
    for (float v : { 9.0f, 6.0f, 3.0f, 0.0f })
    {
        const float y = (float) volt.getY() + (float) volt.slider.getPositionOfValue (v);
        g.drawText (juce::String ((int) v) + " V", juce::Rectangle<float> (vc.getX() + 6.0f, y - 8.0f, 34.0f, 16.0f),
                    juce::Justification::centredRight, false);
    }

    // Master strip.
    const juce::Rectangle<float> ms ((float) pad, (float) masterY, W - 2.0f * (float) pad, (float) masterH);
    box (g, ms, 555, colour::tint.withAlpha (0.45f), 2.0f);
    g.setColour (colour::ink);
    g.setFont (fonts->title (26.0f));
    g.drawText ("Master", juce::Rectangle<float> ((float) input.getX() - 92.0f, ms.getY(), 86.0f, ms.getHeight()),
                juce::Justification::centredRight, false);
}

void SlapdashEditor::resized()
{
    const int W = getWidth();
    const int n = strip.numBands();

    // Header.
    const int selW = modeSelector.idealWidth();
    modeSelector.setBounds (W - pad - 120 - 16 - selW, headerY + 16, selW, 30);

    strip.setBounds (pad, stripY, W - 2 * pad, stripH);

    // Modules row: panels share the room left of "+ band" and the Slapdash column.
    const int right = W - pad - voltW - panelGap - (n < params::maxBands ? addW + panelGap : 0);
    const int panelW = (right - pad - (n - 1) * panelGap) / n;
    for (int b = 0; b < panels.size(); ++b)
        panels[b]->setBounds (pad + b * (panelW + panelGap), modY, panelW, modH);
    addButton.setBounds (right + panelGap, modY, addW, modH);

    const int vx = W - pad - voltW;
    volt.setBounds (vx + 46, modY + 54, 44, modH - 72);

    // Master strip: label, six knobs, the two switches.
    const int slot = 72, kh = SketchKnob::heightFor (42);
    const int total = 92 + 6 * slot + 14 + 150;
    int x = pad + juce::jmax (0, (W - 2 * pad - total) / 2) + 92;
    const int ky = masterY + (masterH - kh) / 2 + 2;
    for (auto* k : { &input, &lowCut, &highCut, &monoMaker, &mix, &output })
    {
        k->setBounds (x, ky, slot, kh);
        x += slot;
    }
    x += 14;
    autoGain.setBounds   (x, masterY + 14, 150, 34);
    speakerSim.setBounds (x, masterY + 52, 150, 34);
}
