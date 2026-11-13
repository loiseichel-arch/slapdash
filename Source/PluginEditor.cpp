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

    onSwitch.attach (st, pid::active);
    onSwitch.setTooltip ("plugin on / bypass (alt-click to reset)");
    addAndMakeVisible (onSwitch);
    linkButton.attach (st, pid::link);
    linkButton.setTooltip ("move all bands together, keeping their offsets (alt-click to reset)");
    addAndMakeVisible (linkButton);
    addAndMakeVisible (strip);

    // ---- Modules ----
    for (int b = 0; b < params::maxBands; ++b)
    {
        auto* panel = panels.add (new BandPanel (st, b));
        panel->onRemove = [this] (int index) { removeBand (index); };
        addChildComponent (panel);
    }
    addButton.onClick = [this] { showAddMenu(); };
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

    // Bypassed: everything but the header fades back.
    const bool on = onSwitch.getToggleState();
    if (on != shownOn)
    {
        shownOn = on;
        for (juce::Component* c : std::initializer_list<juce::Component*> { &strip, &addButton, &volt })
            c->setAlpha (on ? 1.0f : 0.45f);
        for (auto* p : panels) p->setAlpha (on ? 1.0f : 0.45f);
    }
    for (auto* p : panels)
        if (p->isVisible()) p->refreshValues();
}

// ---- Adding and removing bands ----
void SlapdashEditor::showAddMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&laf);
    m.addItem (1, "Add");
    m.addItem (2, "Duplicate");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addButton),
                     [safe = juce::Component::SafePointer<SlapdashEditor> (this)] (int choice)
                     {
                         if (safe != nullptr && choice > 0) safe->addBand (choice == 2);
                     });
}

// Add: a new band with default settings. Duplicate: a copy of the band next
// to it (also its frequency range in Layer mode).
void SlapdashEditor::addBand (bool duplicate)
{
    const int n = strip.numBands();
    if (n >= params::maxBands) return;

    LinkManager::Suspend noLink (proc.link);
    resetBand (n);
    if (duplicate) copyBand (n - 1, n);

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

    LinkManager::Suspend noLink (proc.link);
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
    LinkManager::Suspend noLink (proc.link);
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
    g.setFont (fonts->hand (14.0f));
    g.drawText ("Planet Gloria", juce::Rectangle<float> (W - (float) pad - 160.0f, 12.0f, 160.0f, 18.0f),
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
    // Header, right side: on/off, Link, Split/Layer ("Planet Gloria" above).
    const int selW = modeSelector.idealWidth();
    const int hy = headerY + 18;
    modeSelector.setBounds (W - pad - selW, hy, selW, 30);
    linkButton.setBounds (modeSelector.getX() - 14 - 106, hy - 1, 106, 32);
    onSwitch.setBounds (linkButton.getX() - 14 - 80, hy, 80, 30);

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
