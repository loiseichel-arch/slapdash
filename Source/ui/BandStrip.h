// BandStrip.h
// The strip at the top: where the bands sit in the spectrum.
//
// Split mode : bands side by side. Drag a split point left/right; drag up/
//              down inside a band to set its gain (the horizontal line).
// Layer mode : one lane per band, bands can overlap. Drag an edge, or the
//              middle of a bar to move it whole. Edges snap onto the edges of
//              the other bands (and 20 Hz / 20 kHz); hold Cmd (Ctrl on
//              Windows) to drag without snapping. Drag the gain tag up/down.
// Alt-click anything: back to its default.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>
#include "Sketch.h"
#include "Components.h"
#include "../Parameters.h"

class BandStrip : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    explicit BandStrip (juce::AudioProcessorValueTreeState& s)
    {
        using namespace params;
        bandsParam = s.getParameter (id::bands);
        layerParam = s.getParameter (id::layer);
        for (int k = 0; k < 3; ++k) xover[(size_t) k] = s.getParameter (id::xover[(size_t) k]);
        for (int b = 0; b < maxBands; ++b)
        {
            gain[(size_t) b] = s.getParameter (bandId (b, band::gain));
            lo[(size_t) b]   = s.getParameter (bandId (b, band::lo));
            hi[(size_t) b]   = s.getParameter (bandId (b, band::hi));
        }
        setTooltip ("drag to set (alt-click to reset)");
    }

    int numBands() const { return juce::jlimit (1, params::maxBands, juce::roundToInt (paramValue (bandsParam))); }
    bool isLayer() const { return paramValue (layerParam) > 0.5f; }

    // Edges of the bands in Split mode: 20 Hz, splits..., 20 kHz.
    std::vector<float> splitEdges() const
    {
        std::vector<float> e { params::minHz };
        const int n = numBands();
        for (int k = 0; k < n - 1; ++k)
            e.push_back (juce::jmax (paramValue (xover[(size_t) k]), e.back() * 1.1f));
        e.push_back (params::maxHz);
        return e;
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.5f);
        sketch::box (g, r, 4242, sketch::colour::tint.withAlpha (0.45f), 2.0f);
        if (isLayer()) paintLayer (g); else paintSplit (g);
    }

    // ---- Mouse ----
    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = {};
        if (isLayer()) startLayerDrag (e); else startSplitDrag (e);
        if (drag.param != nullptr && e.mods.isAltDown())
        {
            resetTarget();
            drag = {};
            repaint();
            return;
        }
        if (drag.param != nullptr) drag.param->beginChangeGesture();
        if (drag.second != nullptr) drag.second->beginChangeGesture();
        mouseDrag (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag.param == nullptr) return;
        const auto pos = e.position;
        const bool noSnap = e.mods.isCommandDown();
        snapX = -1.0f;

        switch (drag.what)
        {
            case What::splitPoint:
            {
                const auto edges = splitEdges();
                const int k = drag.index;
                const float lower = edges[(size_t) k] * 1.15f, upper = edges[(size_t) k + 2] / 1.15f;
                const float hz = juce::jlimit (juce::jmax (30.0f, lower), juce::jmin (16000.0f, upper), xToHz (pos.x));
                set (drag.param, hz);
                break;
            }
            case What::splitGain:
                set (drag.param, juce::jlimit (-params::gainRangeDb, params::gainRangeDb, yToGain (pos.y)));
                break;
            case What::layerGain:
                set (drag.param, juce::jlimit (-params::gainRangeDb, params::gainRangeDb,
                                               drag.startA + (drag.startY - pos.y) * 0.1f));
                break;
            case What::layerLo:
            case What::layerHi:
            {
                float hz = snap (xToHz (pos.x), pos.x, drag.index, noSnap);
                const float other = paramValue (drag.what == What::layerLo ? hi[(size_t) drag.index] : lo[(size_t) drag.index]);
                hz = drag.what == What::layerLo ? juce::jmin (hz, other / 1.05f) : juce::jmax (hz, other * 1.05f);
                set (drag.param, juce::jlimit (params::minHz, params::maxHz, hz));
                break;
            }
            case What::layerMove:
            {
                // Move the whole bar on the log axis, keeping its width.
                const float dx = pos.x - drag.startX;
                const float ratio = drag.startB / drag.startA;
                float newLo = xToHz (hzToX (drag.startA) + dx);
                float newHi = newLo * ratio;

                // Snap whichever edge is closer to a target.
                if (! noSnap)
                {
                    float sx1 = -1.0f, sx2 = -1.0f;
                    const float sLo = snapTarget (newLo, drag.index, sx1);
                    const float sHi = snapTarget (newHi, drag.index, sx2);
                    if (sx1 >= 0.0f && (sx2 < 0.0f || std::abs (hzToX (sLo) - hzToX (newLo)) <= std::abs (hzToX (sHi) - hzToX (newHi))))
                    { newLo = sLo; newHi = sLo * ratio; snapX = sx1; }
                    else if (sx2 >= 0.0f)
                    { newHi = sHi; newLo = sHi / ratio; snapX = sx2; }
                }
                if (newLo < params::minHz) { newLo = params::minHz; newHi = newLo * ratio; }
                if (newHi > params::maxHz) { newHi = params::maxHz; newLo = newHi / ratio; }
                set (drag.param, newLo);
                set (drag.second, newHi);
                break;
            }
            case What::none: break;
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (drag.param != nullptr) drag.param->endChangeGesture();
        if (drag.second != nullptr) drag.second->endChangeGesture();
        drag = {};
        snapX = -1.0f;
        repaint();
    }

private:
    enum class What { none, splitPoint, splitGain, layerGain, layerLo, layerHi, layerMove };

    struct Drag
    {
        What what = What::none;
        int index = 0;
        juce::RangedAudioParameter* param = nullptr;
        juce::RangedAudioParameter* second = nullptr;
        float startX = 0, startY = 0, startA = 0, startB = 0;
    };

    // ---- Geometry ----
    juce::Rectangle<float> inner() const { return getLocalBounds().toFloat().reduced (12.0f, 6.0f); }

    float hzToX (float hz) const
    {
        const auto in = inner();
        const float t = std::log (juce::jlimit (params::minHz, params::maxHz, hz) / params::minHz) / std::log (params::maxHz / params::minHz);
        return in.getX() + t * in.getWidth();
    }
    float xToHz (float x) const
    {
        const auto in = inner();
        const float t = juce::jlimit (0.0f, 1.0f, (x - in.getX()) / in.getWidth());
        return params::minHz * std::pow (params::maxHz / params::minHz, t);
    }
    float gainToY (float db) const
    {
        const auto in = inner();
        return in.getCentreY() + 8.0f - db / params::gainRangeDb * (in.getHeight() * 0.34f);
    }
    float yToGain (float y) const
    {
        const auto in = inner();
        return (in.getCentreY() + 8.0f - y) / (in.getHeight() * 0.34f) * params::gainRangeDb;
    }

    juce::Rectangle<float> laneRect (int b) const
    {
        auto in = inner().withTrimmedTop (16.0f);
        const int n = numBands();
        const float h = (in.getHeight() - 3.0f * (float) (n - 1)) / (float) n;
        return { in.getX(), in.getY() + (float) b * (h + 3.0f), in.getWidth(), h };
    }
    juce::Rectangle<float> barRect (int b) const
    {
        const auto lane = laneRect (b);
        const float x1 = hzToX (paramValue (lo[(size_t) b])), x2 = hzToX (paramValue (hi[(size_t) b]));
        return { x1, lane.getY() + 1.0f, juce::jmax (6.0f, x2 - x1), lane.getHeight() - 2.0f };
    }
    juce::Rectangle<float> gainTagRect (int b) const
    {
        const auto bar = barRect (b);
        const float w = 74.0f;
        return juce::Rectangle<float> (w, bar.getHeight()).withCentre (bar.getCentre()).getIntersection (bar);
    }

    // ---- Snapping (Layer mode) ----
    float snapTarget (float hz, int band, float& snappedX) const
    {
        snappedX = -1.0f;
        const float x = hzToX (hz);
        float best = 8.0f;   // pixels
        float result = hz;
        auto consider = [&] (float f)
        {
            const float d = std::abs (hzToX (f) - x);
            if (d < best) { best = d; result = f; snappedX = hzToX (f); }
        };
        consider (params::minHz);
        consider (params::maxHz);
        for (int b = 0; b < numBands(); ++b)
            if (b != band)
            {
                consider (paramValue (lo[(size_t) b]));
                consider (paramValue (hi[(size_t) b]));
            }
        return result;
    }

    float snap (float hz, float, int band, bool noSnap)
    {
        if (noSnap) return hz;
        float sx = -1.0f;
        const float s = snapTarget (hz, band, sx);
        snapX = sx;
        return s;
    }

    // ---- Starting a drag ----
    void startSplitDrag (const juce::MouseEvent& e)
    {
        const auto edges = splitEdges();
        const int n = numBands();
        for (int k = 0; k < n - 1; ++k)
            if (std::abs (e.position.x - hzToX (edges[(size_t) k + 1])) < 7.0f)
            {
                drag.what = What::splitPoint; drag.index = k; drag.param = xover[(size_t) k];
                return;
            }
        for (int b = 0; b < n; ++b)
            if (e.position.x >= hzToX (edges[(size_t) b]) && e.position.x <= hzToX (edges[(size_t) b + 1]))
            {
                drag.what = What::splitGain; drag.index = b; drag.param = gain[(size_t) b];
                return;
            }
    }

    void startLayerDrag (const juce::MouseEvent& e)
    {
        for (int b = 0; b < numBands(); ++b)
        {
            const auto bar = barRect (b);
            if (! laneRect (b).contains (e.position)) continue;
            drag.index = b;
            drag.startX = e.position.x; drag.startY = e.position.y;
            if (std::abs (e.position.x - bar.getX()) < 8.0f)
            { drag.what = What::layerLo; drag.param = lo[(size_t) b]; }
            else if (std::abs (e.position.x - bar.getRight()) < 8.0f)
            { drag.what = What::layerHi; drag.param = hi[(size_t) b]; }
            else if (gainTagRect (b).contains (e.position))
            { drag.what = What::layerGain; drag.param = gain[(size_t) b]; drag.startA = paramValue (gain[(size_t) b]); }
            else if (bar.contains (e.position))
            {
                drag.what = What::layerMove; drag.param = lo[(size_t) b]; drag.second = hi[(size_t) b];
                drag.startA = paramValue (lo[(size_t) b]); drag.startB = paramValue (hi[(size_t) b]);
            }
            return;
        }
    }

    void resetTarget()
    {
        if (drag.what == What::layerMove)
        {
            setParam (drag.param, paramDefault (drag.param));
            setParam (drag.second, paramDefault (drag.second));
        }
        else setParam (drag.param, paramDefault (drag.param));
    }

    static void set (juce::RangedAudioParameter* p, float v)
    {
        if (p != nullptr) p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    // ---- Painting ----
    void paintSplit (juce::Graphics& g)
    {
        using namespace sketch;
        const auto in = inner();
        const auto edges = splitEdges();
        const int n = numBands();
        const float dashes[] = { 4.0f, 4.0f };

        for (int b = 0; b < n; ++b)
        {
            const float x1 = hzToX (edges[(size_t) b]), x2 = hzToX (edges[(size_t) b + 1]);
            const juce::Rectangle<float> zone (x1, in.getY(), x2 - x1, in.getHeight());
            if (b % 2 == 1)
            {
                g.setColour (colour::ink.withAlpha (0.05f));
                g.fillRect (zone);
            }

            g.setColour (colour::ink);
            g.setFont (fonts->hand (16.0f));
            g.drawText (n == 1 ? "Full band" : "Band " + juce::String (b + 1),
                        zone.reduced (8.0f, 2.0f).removeFromTop (18.0f), juce::Justification::centredLeft, false);
            g.setColour (colour::soft);
            g.setFont (fonts->hand (13.0f));
            g.drawText (params::fmtHzValue (edges[(size_t) b]) + juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x92 "))
                            + params::fmtHzValue (edges[(size_t) b + 1]),
                        zone.reduced (8.0f, 2.0f).removeFromTop (18.0f), juce::Justification::centredRight, false);

            // 0 dB line and the gain line.
            juce::Path zero, zeroDashed;
            const float y0 = gainToY (0.0f);
            zero.startNewSubPath (x1 + 8.0f, y0); zero.lineTo (x2 - 8.0f, y0);
            juce::PathStrokeType (1.0f).createDashedStroke (zeroDashed, zero, dashes, 2);
            g.setColour (colour::ink.withAlpha (0.35f));
            g.fillPath (zeroDashed);

            const float db = paramValue (gain[(size_t) b]);
            const float yg = gainToY (db);
            stroke (g, line ({ x1 + 8.0f, yg }, { x2 - 64.0f, yg }, 600 + (uint32_t) b, 0.4f), 3.5f);
            const auto tag = juce::Rectangle<float> (52.0f, 18.0f).withCentre ({ x2 - 36.0f, yg });
            box (g, tag, 610 + (uint32_t) b, colour::accent, 1.6f);
            g.setColour (colour::knob);
            g.setFont (fonts->hand (13.0f));
            g.drawText (params::fmtDb (db, 0), tag, juce::Justification::centred, false);
        }

        // Split points.
        for (int k = 0; k < n - 1; ++k)
        {
            const float x = hzToX (edges[(size_t) k + 1]);
            stroke (g, line ({ x, in.getY() - 2.0f }, { x, in.getBottom() + 2.0f }, 700 + (uint32_t) k, 0.4f), 3.0f, colour::accent);
            const auto tag = juce::Rectangle<float> (74.0f, 18.0f).withCentre ({ x, in.getBottom() - 10.0f });
            box (g, tag, 710 + (uint32_t) k, colour::knob, 1.6f);
            g.setColour (colour::ink);
            g.setFont (fonts->hand (13.0f));
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x94 ")) + params::fmtHzValue (edges[(size_t) k + 1]),
                        tag, juce::Justification::centred, false);
        }
    }

    void paintLayer (juce::Graphics& g)
    {
        using namespace sketch;
        const auto in = inner();
        const float dashes[] = { 4.0f, 4.0f };

        g.setColour (colour::soft);
        g.setFont (fonts->hand (12.0f));
        for (float f : { 100.0f, 1000.0f, 10000.0f })
            g.drawText (params::fmtHzValue (f), juce::Rectangle<float> (60.0f, 14.0f).withCentre ({ hzToX (f), in.getY() + 7.0f }),
                        juce::Justification::centred, false);

        for (int b = 0; b < numBands(); ++b)
        {
            const auto lane = laneRect (b);
            juce::Path base, baseDashed;
            base.startNewSubPath (lane.getX(), lane.getBottom()); base.lineTo (lane.getRight(), lane.getBottom());
            juce::PathStrokeType (1.0f).createDashedStroke (baseDashed, base, dashes, 2);
            g.setColour (colour::ink.withAlpha (0.25f));
            g.fillPath (baseDashed);

            const auto bar = barRect (b);
            const juce::Colour fill = b % 2 == 0 ? colour::accent.withAlpha (b == 0 ? 0.30f : 0.16f)
                                                 : colour::ink.withAlpha (b == 1 ? 0.14f : 0.08f);
            box (g, bar, 800 + (uint32_t) b, fill, 1.8f);

            const float loHz = paramValue (lo[(size_t) b]), hiHz = paramValue (hi[(size_t) b]);
            g.setColour (colour::ink);
            g.setFont (fonts->hand (12.0f));
            const auto inside = bar.reduced (6.0f, 0.0f);
            if (bar.getWidth() > 170.0f)
            {
                g.drawText (params::fmtHzValue (loHz), inside, juce::Justification::centredLeft, false);
                g.drawText (params::fmtHzValue (hiHz), inside, juce::Justification::centredRight, false);
            }
            g.setFont (fonts->hand (13.0f));
            g.drawText ("Band " + juce::String (b + 1) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "))
                            + params::fmtDb (paramValue (gain[(size_t) b]), 0) + juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x95")),
                        gainTagRect (b).expanded (30.0f, 0.0f), juce::Justification::centred, false);
        }

        // Snap guide while dragging.
        if (snapX >= 0.0f)
        {
            juce::Path guide, guideDashed;
            guide.startNewSubPath (snapX, in.getY() + 14.0f); guide.lineTo (snapX, in.getBottom());
            juce::PathStrokeType (2.0f).createDashedStroke (guideDashed, guide, dashes, 2);
            g.setColour (colour::accent);
            g.fillPath (guideDashed);
        }
    }

    juce::RangedAudioParameter* bandsParam = nullptr;
    juce::RangedAudioParameter* layerParam = nullptr;
    std::array<juce::RangedAudioParameter*, 3> xover {};
    std::array<juce::RangedAudioParameter*, params::maxBands> gain {}, lo {}, hi {};
    Drag drag;
    float snapX = -1.0f;
    juce::SharedResourcePointer<sketch::Fonts> fonts;
};
