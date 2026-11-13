// Saturation.h
// The distortion of one band, sample by sample. Plain C++ (no JUCE), so it
// can be tested on its own. Runs at the oversampled rate.
//
// Each type is a small "circuit": what is filtered before the clipper and
// after it matters as much as the clipper itself.
//   Tube     : mids pushed in, asymmetry that grows when you dig in (even
//              harmonics), soft top. Warm, fat, thickens on the attack.
//   Tape     : highs boosted before and cut after (they saturate first),
//              soft hysteresis, head bump around 90 Hz. Round, compressed.
//   Fuzz     : huge gain, hard asymmetric clip, scooped mids, closes on
//              weak notes. Buzzy and square.
//   Screamer : only the mids reach the diodes, the bass stays clean
//              (Tube Screamer style). Tight and mid-forward.
//
// Drive -> More (2nd stage with the same clipper) -> + Grit layer
//       -> built-in level compensation -> DC blocker -> battery gate
//
// Sag      : a power supply that empties on hard attacks and refills
//            slowly: the clipping point drops, the note squashes then
//            "blooms" back.
// Grit     : up to 50 % a raspy layer of upper harmonics; above, an
//            octave-up fuzz (rectifier, Octavia style) joins in.
// Battery  : the Slapdash fader (9 V fresh -> 0 V dead): shifted bias,
//            more gain into a starved stage, less level, and a gate that
//            sputters and chokes the note tails.
// Level compensation: measured per type (CompTable.h), so at -18 dBFS
// Drive and More change the colour, not the volume.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "CompTable.h"

namespace slap
{

constexpr float kPi = 3.14159265f;

enum class Type : int { Tube = 0, Tape, Fuzz, Screamer };

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
    void highPass (float fs, float f, float q)
    {
        const float w = 2 * kPi * f / fs, al = std::sin (w) / (2 * q), c = std::cos (w);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
};

struct BandSettings
{
    Type  type   = Type::Tube;
    float drive  = 0.3f;   // 0..1
    float more   = 0.0f;   // 0..1
    float sag    = 0.0f;   // 0..1
    float grit   = 0.0f;   // 0..1
    float starve = 0.0f;   // 0 = fresh battery (9 V), 1 = dead (0 V)
};

inline float driveGain (float drive) { return std::pow (10.0f, (40.0f * drive) / 20.0f); }   // 0..+40 dB
inline float moreGain  (float more)  { return std::pow (10.0f, (30.0f * more)  / 20.0f); }   // 0..+30 dB

// The clipper of each type (used by both stages).
inline float clipTube (float w)      { return w > 0 ? std::tanh (w) : std::tanh (1.6f * w) / 1.6f; }
inline float clipTape (float w)      { return std::tanh (w); }
inline float clipFuzz (float w)
{
    const float w2 = w * w, w4 = w2 * w2, w8 = w4 * w4;
    const float c = w / std::sqrt (std::sqrt (std::sqrt (1.0f + w8)));   // hard knee
    return c > 0 ? c * 0.6f : c;                                          // asymmetric
}
inline float clipScreamer (float w)
{
    const float w2 = w * w;
    return w / std::sqrt (std::sqrt (1.0f + w2 * w2));                    // soft diode pair
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
        fuzzAtt = coef (1.0f);   fuzzRel = coef (30.0f);
        batAtt  = coef (1.5f);   batRel  = coef (45.0f);
        batSm   = coef (0.6f);
        dcR     = 1.0f - 2.0f * kPi * 8.0f / fs;
        gritHp1.highPass (fs, 1500, 0.7f); gritHp2.highPass (fs, 2200, 0.7f);
        octHp.highPass (fs, 60, 0.7f);
        currentType = -1;
        reset();
    }

    void reset()
    {
        for (auto& c : ch) c = {};
        chopLeft = 0; chopOn = true;
    }

    // Call once per block.
    void setSettings (const BandSettings& s)
    {
        set = s;
        g1 = driveGain (s.drive);
        useMore = s.more > 0.001f;
        g2 = moreGain (s.more);
        comp = comp::lookup ((int) s.type, s.drive, s.more);

        if ((int) s.type != currentType)
        {
            currentType = (int) s.type;
            for (auto* f : { &pre1, &pre2, &post1, &post2, &post3, &hpTs }) f->bypass();
            switch (s.type)
            {
                case Type::Tube:
                    pre1.peak (fs, 700, 0.7f, 4); pre2.highPass (fs, 40, 0.7f);
                    post1.lowPass (fs, 7500, 0.7f); post2.peak (fs, 130, 0.8f, 1.5f);
                    break;
                case Type::Tape:
                    pre1.highShelf (fs, 2500, 10); pre2.highPass (fs, 25, 0.7f);
                    post1.highShelf (fs, 2500, -10); post2.peak (fs, 90, 1.0f, 3.5f);
                    post3.lowPass (fs, 13000, 0.7f);
                    break;
                case Type::Fuzz:
                    pre1.highPass (fs, 90, 0.7f); pre2.peak (fs, 1200, 0.7f, 3);
                    post1.peak (fs, 900, 0.6f, -6); post2.lowPass (fs, 5200, 0.7f);
                    break;
                case Type::Screamer:
                default:
                    hpTs.highPass (fs, 720, 0.6f);
                    post1.lowPass (fs, 3300, 0.7f); post2.peak (fs, 800, 0.9f, 3);
                    break;
            }
            for (auto& c : ch) c.filters = {};   // no blast from old filter states
        }
    }

    // The sputter of a dying battery: random chops shared by both channels.
    // Call once per sample, before process().
    void tick()
    {
        if (--chopLeft <= 0)
        {
            chopOn = nextRandom() < 0.55f;
            chopLeft = 1 + (int) ((0.6f + 4.0f * nextRandom()) * 0.001f * fs);
        }
    }

    // Without compensation (used to measure CompTable.h).
    void setCompensation (float c) { comp = c; }

    // x = band-filtered input. Returns the saturated band (before Mix).
    float process (int c, float x)
    {
        auto& st = ch[c];
        auto& fl = st.filters;
        const float s = set.starve;

        // ---- Sag: the supply empties on hard attacks and refills slowly ----
        const float demand = std::min (4.0f, std::abs (x) * g1);
        st.env = demand > st.env ? envFast * st.env + (1 - envFast) * demand : envRel * st.env + (1 - envRel) * demand;
        const float vTarget = 1.0f / (1.0f + set.sag * 6.0f * st.env);
        st.v = vTarget < st.v ? sagDown * st.v + (1 - sagDown) * vTarget : sagUp * st.v + (1 - sagUp) * vTarget;
        const float v = st.v;
        const float sagBias = 0.25f * set.sag * (1 - v);

        // ---- Battery: more gain into a starved, biased stage ----
        const float batGain = 1.0f + 2.0f * s;
        const float batBias = 0.45f * s;
        const float g = g1 * batGain;

        float y = 0.0f;
        switch (set.type)
        {
            case Type::Tube:
            {
                const float u = pre2.process (pre1.process (x, fl[0]), fl[1]) * g;
                const float b = 0.12f + 0.4f * std::min (1.0f, st.env * 0.5f) + sagBias + batBias;
                float c1 = (clipTube (u / v + b) - std::tanh (b)) * v;
                if (useMore) c1 = clipTube (c1 * g2);
                y = post2.process (post1.process (c1, fl[2]), fl[3]);
                break;
            }
            case Type::Tape:
            {
                const float u = pre2.process (pre1.process (x, fl[0]), fl[1]) * (0.35f + 0.65f * g * 0.25f);
                float h = clipTape (u / v + sagBias + batBias + 0.35f * st.hyst);
                st.hyst = h;
                h *= v;
                if (useMore) h = clipTape (h * g2);
                y = post3.process (post2.process (post1.process (h, fl[2]), fl[3]), fl[4]);
                break;
            }
            case Type::Fuzz:
            {
                const float u = pre2.process (pre1.process (x, fl[0]), fl[1]) * g * 6.0f;
                float c1 = clipFuzz (u / v + 0.3f + sagBias + batBias) * v;
                if (useMore) c1 = clipFuzz (c1 * g2);
                y = post2.process (post1.process (c1, fl[2]), fl[3]);

                // Closes on weak notes.
                const float a = std::abs (x);
                st.fuzzGate = a > st.fuzzGate ? fuzzAtt * st.fuzzGate + (1 - fuzzAtt) * a
                                              : fuzzRel * st.fuzzGate + (1 - fuzzRel) * a;
                const float t = std::clamp ((st.fuzzGate - 0.004f) / 0.02f, 0.0f, 1.0f);
                y *= t * t * (3 - 2 * t);
                break;
            }
            case Type::Screamer:
            default:
            {
                const float d = hpTs.process (x, fl[5]) * g * 2.0f;
                float c1 = clipScreamer (d / v + sagBias + batBias) * v;
                if (useMore) c1 = clipScreamer (c1 * g2);
                y = post2.process (post1.process (0.6f * x + 0.7f * c1, fl[2]), fl[3]);
                break;
            }
        }

        // ---- Grit ----
        if (set.grit > 0.0f)
        {
            const float rasp = std::clamp (gritHp1.process (x, st.grit[0]) * g1 * 25.0f, -1.0f, 1.0f);
            const float raspOut = gritHp2.process (rasp, st.grit[1]);
            const float oct = std::clamp (octHp.process (std::abs (x) * g1 * 3.0f, st.grit[2]) * 4.0f, -1.0f, 1.0f);
            const float aRasp = std::min (1.0f, set.grit * 2.0f) * 0.55f;
            const float aOct  = std::max (0.0f, set.grit * 2.0f - 1.0f) * 0.7f;
            y += aRasp * raspOut + aOct * oct;
        }

        y *= comp;
        y *= 1.0f - 0.55f * s;   // a dying battery loses level (Auto gain can bring it back)

        // ---- DC blocker (the biases add DC) ----
        const float dc = y - st.dcX + dcR * st.dcY;
        st.dcX = y; st.dcY = dc;
        y = dc;

        // ---- Battery gate: chokes and sputters on the tails ----
        if (s > 0.02f)
        {
            const float a = std::abs (x);
            st.batEnv = a > st.batEnv ? batAtt * st.batEnv + (1 - batAtt) * a : batRel * st.batEnv + (1 - batRel) * a;
            const float thr = 0.001f + 0.08f * s * s;
            float target;
            if (st.batEnv < thr)             target = 0.0f;
            else if (st.batEnv < thr * 2.0f) target = chopOn ? 1.0f : 0.0f;   // sputter zone
            else                             target = (s > 0.6f && ! chopOn && nextRandomCheap (st) < (s - 0.6f) * 0.5f) ? 0.3f : 1.0f;
            st.batGate = batSm * st.batGate + (1 - batSm) * target;
            y *= st.batGate;
        }
        else st.batGate = 1.0f;

        return y;
    }

private:
    struct Channel
    {
        std::array<Biquad::State, 6> filters {};
        std::array<Biquad::State, 3> grit {};
        float env = 0, v = 1, hyst = 0, fuzzGate = 0, dcX = 0, dcY = 0, batEnv = 0, batGate = 1;
        uint32_t rnd = 0x1234567u;
    };

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

    BandSettings set;
    Channel ch[maxCh];
    Biquad pre1, pre2, post1, post2, post3, hpTs, gritHp1, gritHp2, octHp;
    int currentType = -1;
    float fs = 192000.0f;
    float g1 = 1.0f, g2 = 1.0f, comp = 1.0f;
    bool useMore = false;
    float envFast = 0, envRel = 0, sagDown = 0, sagUp = 0, fuzzAtt = 0, fuzzRel = 0;
    float batAtt = 0, batRel = 0, batSm = 0, dcR = 0.9999f;
    uint32_t seed = 0x9E3779B9u;
    int chopLeft = 0;
    bool chopOn = true;
};

} // namespace slap
