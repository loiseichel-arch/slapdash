// Parameters.h
// Every automatable parameter of Slapdash, in one place.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

namespace params
{

inline constexpr int maxBands = 4;

// ---- Global IDs ----
namespace id
{
    inline constexpr const char* input      = "input";
    inline constexpr const char* lowCut     = "lowCut";
    inline constexpr const char* highCut    = "highCut";
    inline constexpr const char* monoMaker  = "monoMaker";
    inline constexpr const char* mix        = "mix";
    inline constexpr const char* output     = "output";
    inline constexpr const char* autoGain   = "autoGain";
    inline constexpr const char* speakerSim = "speakerSim";
    inline constexpr const char* volt       = "slapdash";   // the "Slapdash" fader (battery voltage)
    inline constexpr const char* bands      = "bands";
    inline constexpr const char* layer      = "layer";
    inline constexpr const char* active     = "active";     // On / bypass of the whole plugin
    inline constexpr const char* link       = "link";       // bands move together
    inline constexpr std::array<const char*, 3> xover { "xover1", "xover2", "xover3" };
}

// ---- Per band IDs: "b1_drive", "b2_drive", ... ----
namespace band
{
    inline constexpr const char* type   = "type";
    inline constexpr const char* drive  = "drive";
    inline constexpr const char* more   = "more";
    inline constexpr const char* sag    = "sag";
    inline constexpr const char* grit   = "grit";
    inline constexpr const char* mix    = "mix";
    inline constexpr const char* gain   = "gain";
    inline constexpr const char* pan    = "pan";
    inline constexpr const char* solo   = "solo";
    inline constexpr const char* mute   = "mute";
    inline constexpr const char* bypass = "bypass";
    inline constexpr const char* lo     = "lo";     // Layer mode: low edge
    inline constexpr const char* hi     = "hi";     // Layer mode: high edge

    inline constexpr std::array<const char*, 13> all { type, drive, more, sag, grit, mix, gain, pan,
                                                       solo, mute, bypass, lo, hi };
}

inline juce::String bandId (int bandIndex, const char* name)   // bandIndex 0..3
{
    return "b" + juce::String (bandIndex + 1) + "_" + name;
}

// Stored as an index: renaming a type keeps old sessions working.
inline juce::StringArray typeNames() { return { "Tube", "Tape", "Fuzz", "Screamer" }; }

inline constexpr float minHz = 20.0f, maxHz = 20000.0f;
inline constexpr float gainMinDb = -24.0f, gainMaxDb = 12.0f;   // band gain

// ---- Value formatting (English only) ----
inline juce::String fmtHzValue (float v)
{
    if (v >= 1000.0f)
    {
        const float k = v / 1000.0f;
        return juce::String (k, k < 10.0f ? 2 : 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " kHz";
    }
    return juce::String (juce::roundToInt (v)) + " Hz";
}
inline juce::String fmtHz (float v, int)        { return fmtHzValue (v); }
inline juce::String fmtLowCut (float v, int)    { return v <= 20.5f ? "Off" : fmtHzValue (v); }
inline juce::String fmtHighCut (float v, int)   { return v >= 19900.0f ? "Off" : fmtHzValue (v); }
inline juce::String fmtMono (float v, int)      { return v < 1.0f ? "Off" : fmtHzValue (v); }
inline juce::String fmtPct (float v, int)       { return juce::String (juce::roundToInt (v)) + "%"; }
inline juce::String fmtVolt (float v, int)      { return juce::String (v, 1) + " V"; }
inline juce::String fmtDb (float v, int)
{
    const float r = std::round (v * 10.0f) / 10.0f;
    return (r > 0.0f ? "+" : "") + juce::String (r, 1) + " dB";
}
inline juce::String fmtPan (float v, int)
{
    const int p = juce::roundToInt (v);
    if (p == 0) return "C";
    return (p < 0 ? "L" : "R") + juce::String (std::abs (p));
}

// Frequency range that moves evenly on a log scale.
inline juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float s, float e, float n) { return s * std::pow (e / s, n); },
             [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
             [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
}

// Defaults of the split points (Split mode).
inline constexpr std::array<float, 3> xoverDefaults { 632.0f, 3550.0f, 8400.0f };

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&] (const String& pid, const String& name, NormalisableRange<float> range, float def,
                         String (*fmt) (float, int), const char* unit)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { pid, 1 }, name, range, def,
            AudioParameterFloatAttributes().withStringFromValueFunction (fmt).withLabel (unit)));
    };
    auto addBool = [&] (const String& pid, const String& name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid, 1 }, name, def));
    };

    const NormalisableRange<float> pct (0.0f, 100.0f, 0.1f);

    // ---- Master ----
    addFloat (id::input,     "Input",      NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, fmtDb, "dB");
    addFloat (id::lowCut,    "Low Cut",    logRange (20.0f, 1000.0f), 20.0f, fmtLowCut, "Hz");
    addFloat (id::highCut,   "High Cut",   logRange (1000.0f, 20000.0f), 20000.0f, fmtHighCut, "Hz");
    {
        // Mono maker: 0 = off, then 20 Hz .. 1000 Hz.
        NormalisableRange<float> r (0.0f, 1000.0f, 1.0f);
        r.setSkewForCentre (200.0f);
        addFloat (id::monoMaker, "Mono Maker", r, 0.0f, fmtMono, "Hz");
    }
    addFloat (id::mix,       "Mix",        pct, 100.0f, fmtPct, "%");
    addFloat (id::output,    "Output",     NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, fmtDb, "dB");
    addBool  (id::active,     "On",          true);
    addBool  (id::autoGain,   "Auto Gain",   true);
    addBool  (id::speakerSim, "Speaker Sim", false);
    addFloat (id::volt,      "Slapdash",   NormalisableRange<float> (0.0f, 9.0f, 0.01f), 9.0f, fmtVolt, "V");

    // ---- Bands ----
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { id::bands, 1 }, "Bands", 1, maxBands, 1));
    addBool (id::layer, "Layer Mode", false);
    addBool (id::link,  "Link Bands", false);
    for (int i = 0; i < 3; ++i)
        addFloat (id::xover[(size_t) i], "Split " + String (i + 1), logRange (30.0f, 16000.0f),
                  xoverDefaults[(size_t) i], fmtHz, "Hz");

    for (int b = 0; b < maxBands; ++b)
    {
        const String n = "Band " + String (b + 1) + " ";
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { bandId (b, band::type), 1 },
                                                            n + "Type", typeNames(), 0));
        addFloat (bandId (b, band::drive), n + "Drive", pct, 30.0f, fmtPct, "%");
        addFloat (bandId (b, band::more),  n + "More",  pct, 0.0f,  fmtPct, "%");
        addFloat (bandId (b, band::sag),   n + "Sag",   pct, 0.0f,  fmtPct, "%");
        addFloat (bandId (b, band::grit),  n + "Grit",  pct, 0.0f,  fmtPct, "%");
        addFloat (bandId (b, band::mix),   n + "Mix",   pct, 100.0f, fmtPct, "%");
        addFloat (bandId (b, band::gain),  n + "Gain",  NormalisableRange<float> (gainMinDb, gainMaxDb, 0.1f),
                  0.0f, fmtDb, "dB");
        addFloat (bandId (b, band::pan),   n + "Pan",   NormalisableRange<float> (-100.0f, 100.0f, 1.0f),
                  0.0f, fmtPan, "");
        addBool  (bandId (b, band::solo),   n + "Solo",   false);
        addBool  (bandId (b, band::mute),   n + "Mute",   false);
        addBool  (bandId (b, band::bypass), n + "Bypass", false);
        addFloat (bandId (b, band::lo), n + "Low Edge",  logRange (minHz, maxHz), minHz, fmtHz, "Hz");
        addFloat (bandId (b, band::hi), n + "High Edge", logRange (minHz, maxHz), maxHz, fmtHz, "Hz");
    }

    return layout;
}

} // namespace params
