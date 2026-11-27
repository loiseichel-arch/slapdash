// Saturation.h
// The distortion of one band, sample by sample. Plain C++ (no JUCE), so it
// can be tested on its own. Runs at the oversampled rate.
//
// Types (v0.3)
//   Transistor : neutral. Symmetric clip, no filter at all.
//   Tube       : a little more mids into the clipper, asymmetry that grows
//                when you dig in (even harmonics), softer top.
//   Tape       : highs pushed before and pulled back after (they saturate
//                first), soft hysteresis, small head bump.
//   Fuzz       : hard asymmetric clip, slightly scooped mids.
//
// The colour filters of Tube, Tape and Fuzz grow with the drive: with
// little drive the band stays open (flat to 15 kHz), the colour comes as
// you push it. Transistor never filters.
//
// Drive: one stage, 0..+40 dB. The More switch extends the course by
// another +40 dB: Drive at 100 % without More = Drive at 0 % with More.
//
// Sag     : a power supply that empties on hard attacks and refills
//           slowly: the clipping point drops, the note squashes then
//           "blooms" back.
// Battery : the Slapdash fader (9 V fresh -> 0 V dead). Since v0.4 it is not
//           in the bands any more: one BatteryStage on the master, after the
//           sum of the modules (see the end of this file), so the band Mix
//           knobs do not change it; only the master Mix doses it.
// Level compensation: measured per type (CompTable.h), so at -18 dBFS the
// drive changes the colour, not the volume.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "CompTable.h"

namespace slap
{

constexpr float kPi = 3.14159265f;

enum class Type : int { Transistor = 0, Tube, Tape, Fuzz };

// ---- Biquad: coefficients shared, state per channel ----
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    struct State { float z1 = 0, z2 = 0; };

    float process (float x, State& s) const
    {
        const float y = b0 * x + s.z1;
        s.z1 = b1 * x - a1 * y + s.z2;
        s.z2 = b2 * x - a2 * y;
        return y;
    }

    void set (float B0, float B1, float B2, float A0, float A1, float A2)
    { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }

    void bypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    void peak (float fs, float f, float q, float db)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * f / fs, al = std::sin (w) / (2 * q), c = std::cos (w);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void highShelf (float fs, float f, float db)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * f / fs, c = std::cos (w), s = std::sin (w);
        const float al = s / 2 * std::sqrt (2.0f), sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
    void lowPass (float fs, float f, float q)
    {
        const float w = 2 * kPi * f / fs, al = std::sin (w) / (2 * q), c = std::cos (w);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
};

struct BandSettings
{
    Type  type   = Type::Transistor;
    float drive  = 0.3f;    // 0..1
    bool  more   = false;   // extends the drive by another full course
    float sag    = 0.0f;    // 0..1
};

// Total drive course, 0..2 (2 = Drive 100 % with More).
inline float driveCourse (float drive, bool more) { return std::clamp (drive, 0.0f, 1.0f) + (more ? 1.0f : 0.0f); }
inline float driveGainDb (float course) { return 40.0f * course; }   // 0..+80 dB

// Gain offset of each type (dB), so the same Drive gives about the same amount
// of saturation on every type (v0.5: measured by the compression of a drum-like
// signal and checked by ear against Transistor and Tube).
inline float typeGainDb (Type t)
{
    switch (t)
    {
        case Type::Tape: return -3.0f;
        case Type::Fuzz: return -4.0f;
        default:         return 0.0f;
    }
}

// The clipper of each type.
inline float clipTransistor (float w)
{
    const float a = std::abs (w);
    return w / std::cbrt (1.0f + a * a * a);                              // symmetric, firm knee
}
inline float clipTube (float w) { return w > 0 ? std::tanh (w) : std::tanh (1.6f * w) / 1.6f; }
inline float clipTape (float w) { return std::tanh (w); }
inline float clipFuzz (float w)
{
    const float w2 = w * w, w4 = w2 * w2, w8 = w4 * w4;
    const float c = w / std::sqrt (std::sqrt (std::sqrt (1.0f + w8)));   // hard knee
    return c > 0 ? c * 0.6f : c;                                          // asymmetric
}

// One band, up to two channels.
class BandSaturator
{
public:
    static constexpr int maxCh = 2;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        auto coef = [this] (float ms) { return std::exp (-1.0f / (0.001f * ms * fs)); };
        envFast = coef (3.0f);   envRel  = coef (60.0f);
        sagDown = coef (4.0f);   sagUp   = coef (280.0f);
        dcR     = 1.0f - 2.0f * kPi * 8.0f / fs;
        currentType = -1; currentAmt = -1.0f;
        reset();
    }

    void reset()
    {
        for (auto& c : ch) c = {};
    }

    // Call once per block.
    void setSettings (const BandSettings& s)
    {
        set = s;
        const float course = driveCourse (s.drive, s.more);
        g = std::pow (10.0f, (driveGainDb (course) + typeGainDb (s.type)) / 20.0f);
        comp = comp::lookup ((int) s.type, course);

        // How much colour: grows with the drive, full from about +36 dB.
        const float amt = std::clamp (driveGainDb (course) / 36.0f, 0.0f, 1.0f);
        if ((int) s.type != currentType || std::abs (amt - currentAmt) > 0.005f)
        {
            const bool typeChanged = (int) s.type != currentType;
            currentType = (int) s.type; currentAmt = amt;
            setFilters (s.type, amt);
            if (typeChanged) for (auto& c : ch) c.filters = {};   // no blast from old filter states
        }
    }

    // Without compensation (used to measure CompTable.h).
    void setCompensation (float c) { comp = c; }

    // x = band-filtered input. Returns the saturated band (before Mix).
    float process (int c, float x)
    {
        auto& st = ch[c];
        auto& fl = st.filters;

        // ---- Sag: the supply empties on hard attacks and refills slowly ----
        const float demand = std::min (4.0f, std::abs (x) * g);
        st.env = demand > st.env ? envFast * st.env + (1 - envFast) * demand : envRel * st.env + (1 - envRel) * demand;
        const float vTarget = 1.0f / (1.0f + set.sag * 6.0f * st.env);
        st.v = vTarget < st.v ? sagDown * st.v + (1 - sagDown) * vTarget : sagUp * st.v + (1 - sagUp) * vTarget;
        const float v = st.v;
        const float sagBias = 0.25f * set.sag * (1 - v);

        const float gb = g;

        float y = 0.0f;
        switch (set.type)
        {
            case Type::Transistor:
            default:
            {
                const float b = sagBias;
                y = (clipTransistor (x * gb / v + b) - clipTransistor (b)) * v;
                break;
            }
            case Type::Tube:
            {
                const float u = pre1.process (x, fl[0]) * gb;
                const float b = 0.12f + 0.4f * std::min (1.0f, st.env * 0.5f) + sagBias;
                const float c1 = (clipTube (u / v + b) - std::tanh (b)) * v;
                y = post2.process (post1.process (c1, fl[2]), fl[3]);
                break;
            }
            case Type::Tape:
            {
                const float u = pre1.process (x, fl[0]) * gb;
                float h = clipTape (u / v + sagBias + 0.35f * st.hyst);
                st.hyst = h;
                h *= v;
                y = post3.process (post2.process (post1.process (h, fl[2]), fl[3]), fl[4]);
                break;
            }
            case Type::Fuzz:
            {
                const float u = pre1.process (x, fl[0]) * gb;
                const float c1 = clipFuzz (u / v + 0.3f + sagBias) * v;
                y = post2.process (post1.process (c1, fl[2]), fl[3]);
                break;
            }
        }

        y *= comp;

        // ---- DC blocker (the biases add DC) ----
        const float dc = y - st.dcX + dcR * st.dcY;
        st.dcX = y; st.dcY = dc;
        return dc;
    }

private:
    // Colour filters, scaled by amt (0 = open, 1 = full colour). Gentler than v0.2.
    void setFilters (Type t, float amt)
    {
        for (auto* f : { &pre1, &post1, &post2, &post3 }) f->bypass();
        auto lp = [&] (float full) { return 30000.0f * std::pow (full / 30000.0f, amt); };   // 30 kHz -> full
        switch (t)
        {
            case Type::Tube:
                pre1.peak (fs, 700, 0.7f, 2.0f * amt);
                post1.lowPass (fs, lp (11000.0f), 0.7f);
                post2.peak (fs, 130, 0.8f, 1.0f * amt);
                break;
            case Type::Tape:
                pre1.highShelf (fs, 2500, 8.0f);    // emphasis / de-emphasis cancel when clean
                post1.highShelf (fs, 2500, -8.0f);
                post2.peak (fs, 90, 1.0f, 2.0f * amt);
                post3.lowPass (fs, lp (16000.0f), 0.7f);
                break;
            case Type::Fuzz:
                pre1.peak (fs, 1200, 0.7f, 2.0f * amt);
                post1.peak (fs, 900, 0.6f, -3.0f * amt);
                post2.lowPass (fs, lp (8000.0f), 0.7f);
                break;
            case Type::Transistor:
            default:
                break;   // no filter
        }
    }

    struct Channel
    {
        std::array<Biquad::State, 6> filters {};
        float env = 0, v = 1, hyst = 0, dcX = 0, dcY = 0;
    };

    BandSettings set;
    Channel ch[maxCh];
    Biquad pre1, post1, post2, post3;
    int currentType = -1;
    float currentAmt = -1.0f;
    float fs = 192000.0f;
    float g = 1.0f, comp = 1.0f;
    float envFast = 0, envRel = 0, sagDown = 0, sagUp = 0;
    float dcR = 0.9999f;
};

// ---------------------------------------------------------------------------
// The Slapdash fader: one battery for the whole plug, on the master, after
// the sum of the modules (oversampled). A simple transistor stage fed by a
// dying battery: as the voltage drops, the supply gets lower (the stage
// clips earlier), the bias shifts (asymmetry, even harmonics), there is a
// little more gain into it and less level out. In "gate" mode it also
// sputters and chokes the note tails; in "open" mode, the colour without
// the gate. At 9 V the stage is out of the way (exact pass-through); it
// comes in gradually over the first volt.
class BatteryStage
{
public:
    static constexpr int maxCh = 2;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        auto coef = [this] (float ms) { return std::exp (-1.0f / (0.001f * ms * fs)); };
        batAtt = coef (1.5f);  batRel = coef (45.0f);
        batSm  = coef (0.6f);
        dcR    = 1.0f - 2.0f * kPi * 8.0f / fs;
        reset();
    }

    void reset()
    {
        for (auto& c : ch) c = {};
        chopLeft = 0; chopOn = true;
    }

    // starve: 0 = fresh (9 V) .. 1 = dead (0 V). Call once per block.
    void set (float starveAmount, bool gateMode)
    {
        s = std::clamp (starveAmount, 0.0f, 1.0f);
        gate = gateMode;
    }

    bool active() const { return s > 0.0f; }

    // Random chops of the sputter, shared by both channels. Once per sample.
    void tick()
    {
        if (--chopLeft <= 0)
        {
            chopOn = nextRandom() < 0.55f;
            chopLeft = 1 + (int) ((0.6f + 4.0f * nextRandom()) * 0.001f * fs);
        }
    }

    float process (int c, float x)
    {
        auto& st = ch[c];
        if (s <= 0.0f) { st = {}; return x; }

        // starved transistor stage
        const float head = 1.0f + 3.0f * s;          // the supply drops: clips earlier
        const float gain = 1.0f + 1.0f * s;          // a little more gain into it
        const float b = 0.45f * s;                   // shifted bias
        float y = (clipTransistor (x * gain * head + b) - clipTransistor (b)) / head;
        y *= 1.0f - 0.55f * s;                       // a dying battery loses level (Auto gain can bring it back)

        // DC blocker (the bias adds DC)
        const float dc = y - st.dcX + dcR * st.dcY;
        st.dcX = y; st.dcY = dc;
        y = dc;

        // comes in over the first volt (9 V -> 8 V)
        const float w = std::min (1.0f, s * 9.0f);
        y = x + w * (y - x);

        // gate (gate mode only): chokes and sputters on the tails
        if (gate && s > 0.02f)
        {
            const float a = std::abs (x);
            st.env = a > st.env ? batAtt * st.env + (1 - batAtt) * a : batRel * st.env + (1 - batRel) * a;
            const float thr = 0.001f + 0.08f * s * s;
            float target;
            if (st.env < thr)             target = 0.0f;
            else if (st.env < thr * 2.0f) target = chopOn ? 1.0f : 0.0f;   // sputter zone
            else                          target = (s > 0.6f && ! chopOn && nextRandomCheap (st) < (s - 0.6f) * 0.5f) ? 0.3f : 1.0f;
            st.gate = batSm * st.gate + (1 - batSm) * target;
            y *= st.gate;
        }
        else st.gate = 1.0f;
        return y;
    }

private:
    struct Channel { float dcX = 0, dcY = 0, env = 0, gate = 1; uint32_t rnd = 0x1234567u; };

    float nextRandom()
    {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        return (float) (seed & 0xFFFFFF) / 16777216.0f;
    }
    static float nextRandomCheap (Channel& c)
    {
        c.rnd ^= c.rnd << 13; c.rnd ^= c.rnd >> 17; c.rnd ^= c.rnd << 5;
        return (float) (c.rnd & 0xFFFF) / 65536.0f;
    }

    Channel ch[maxCh];
    float fs = 192000.0f, s = 0.0f;
    bool gate = true;
    float batAtt = 0, batRel = 0, batSm = 0, dcR = 0.9999f;
    uint32_t seed = 0x9E3779B9u;
    int chopLeft = 0;
    bool chopOn = true;
};

} // namespace slap
