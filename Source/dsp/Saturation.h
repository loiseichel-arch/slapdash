// Saturation.h
// The distortion of one band, sample by sample. Plain C++ (no JUCE), so it
// can be tested on its own.
//
// Chain of one band (at the oversampled rate):
//   input -> Drive stage -> More stage (cascade) -> + Grit layer
//         -> built-in level compensation -> DC blocker -> tone of the type
//         -> battery gate (sputter) -> Mix with the clean band signal
//
// Sag      : a level follower lowers the headroom when you play louder, so
//            the attack is squashed and the note "blooms".
// Battery  : the Slapdash fader (9 V fresh -> 0 V dead). A starving circuit
//            gets a shifted bias (asymmetric, spitty), less headroom, less
//            level, and a gate that chokes and sputters on the note tails.
// Level compensation: calibrated on Drive and More, so a -12 dBFS input
// comes out at about the same level whatever the drive. The band Mix then
// really doses the amount of saturation, not the volume.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace slap
{

enum class Type : int { Tube = 0, Tape, Transistor, Diode };

inline float shape (Type t, float x)
{
    switch (t)
    {
        case Type::Tube:
        {
            // Asymmetric soft clip: even harmonics, warm.
            constexpr float b = 0.25f;
            static const float tb = std::tanh (b);
            return (std::tanh (x + b) - tb) * 1.08f;
        }
        case Type::Tape:
            // Very soft, compresses before it clips.
            return 0.6366198f * std::atan (1.3f * x);
        case Type::Transistor:
        {
            // Harder, odd harmonics.
            const float a = std::abs (x);
            return std::copysign (1.0f - std::exp (-1.6f * a), x);
        }
        case Type::Diode:
        default:
        {
            // Clean clip with a sharp knee, overdrive pedal style.
            const float a = std::abs (x);
            const float a2 = a * a;
            return x / std::pow (1.0f + a2 * a2 * a, 0.2f);
        }
    }
}

struct BandSettings
{
    Type  type   = Type::Tube;
    float drive  = 0.3f;   // 0..1
    float more   = 0.0f;   // 0..1
    float sag    = 0.0f;   // 0..1
    float grit   = 0.0f;   // 0..1
    float starve = 0.0f;   // 0 = fresh battery (9 V), 1 = dead (0 V)
};

// Gains of the two stages.
inline float driveGain (float drive) { return std::pow (10.0f, (40.0f * drive) / 20.0f); }   // 0..+40 dB
inline float moreGain  (float more)  { return std::pow (10.0f, (30.0f * more)  / 20.0f); }   // 0..+30 dB

// Static chain (no sag, no battery): used to calibrate the compensation.
inline float staticChain (Type t, float x, float g1, float g2, bool useMore)
{
    float y = shape (t, x * g1);
    if (useMore) y = shape (t, y * g2);
    return y;
}

inline float compensation (Type t, float g1, float g2, bool useMore)
{
    constexpr float ref = 0.25f;   // -12 dBFS
    const float out = 0.5f * (staticChain (t, ref, g1, g2, useMore) - staticChain (t, -ref, g1, g2, useMore));
    return ref / std::max (1.0e-4f, std::abs (out));
}

// One band, both channels.
class BandSaturator
{
public:
    static constexpr int maxCh = 2;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        auto coef = [this] (float ms) { return std::exp (-1.0f / (0.001f * ms * fs)); };
        sagAtt  = coef (5.0f);   sagRel  = coef (120.0f);
        gateAtt = coef (1.5f);   gateRel = coef (45.0f);
        gateSm  = coef (0.6f);
        dcR     = 1.0f - 2.0f * 3.14159265f * 10.0f / fs;
        gritA   = std::exp (-2.0f * 3.14159265f * 2500.0f / fs);
        reset();
    }

    void reset()
    {
        for (auto& c : ch) c = {};
        chopLeft = 0; chopOn = true;
    }

    // Call once per block when the settings change.
    void setSettings (const BandSettings& s)
    {
        set = s;
        g1 = driveGain (s.drive);
        useMore = s.more > 0.001f;
        g2 = moreGain (s.more);
        comp = compensation (s.type, g1, g2, useMore);

        switch (s.type)
        {
            case Type::Tape: toneA = std::exp (-2.0f * 3.14159265f * 9000.0f / fs);  break;
            case Type::Tube: toneA = std::exp (-2.0f * 3.14159265f * 14000.0f / fs); break;
            default:         toneA = 0.0f; break;
        }
    }

    // The sputter of a dying battery: random on/off chops, shared by both
    // channels. Call once per sample, before process().
    void tick()
    {
        if (--chopLeft <= 0)
        {
            chopOn = nextRandom() < 0.55f;
            chopLeft = 1 + (int) ((0.6f + 4.0f * nextRandom()) * 0.001f * fs);
        }
    }

    // x = band-filtered input. Returns the saturated band (before Mix).
    float process (int c, float x)
    {
        auto& st = ch[c];
        const float s = set.starve;

        // ---- Sag: headroom drops with level ----
        const float driven = std::min (4.0f, std::abs (x) * g1);
        st.sagEnv = driven > st.sagEnv ? sagAtt * st.sagEnv + (1.0f - sagAtt) * driven
                                       : sagRel * st.sagEnv + (1.0f - sagRel) * driven;
        const float sagF = 1.0f + set.sag * 2.5f * st.sagEnv;

        // ---- Battery: more gain into a starved, biased stage ----
        const float starveGain = 1.0f + 2.0f * s;
        const float bias = 0.45f * s;

        float y = shape (set.type, x * g1 * starveGain * sagF + bias);
        if (useMore) y = shape (set.type, y * g2);
        y /= sagF;
        y *= 1.0f - 0.55f * s;   // a dying battery loses level (Auto gain can bring it back)

        // ---- Grit: raspy layer of upper harmonics, in parallel ----
        if (set.grit > 0.0f)
        {
            const float fz = std::clamp (x * g1 * 6.0f, -1.0f, 1.0f);
            const float hp = gritA * (st.gritY + fz - st.gritX);
            st.gritX = fz; st.gritY = hp;
            y += set.grit * 0.5f * hp;
        }

        y *= comp;

        // ---- DC blocker (the bias adds DC) ----
        const float dc = y - st.dcX + dcR * st.dcY;
        st.dcX = y; st.dcY = dc;
        y = dc;

        // ---- Tone of the type ----
        if (toneA > 0.0f)
        {
            st.tone = toneA * st.tone + (1.0f - toneA) * y;
            y = st.tone;
        }

        // ---- Battery gate: chokes and sputters on the tails ----
        if (s > 0.02f)
        {
            const float a = std::abs (x);
            st.gateEnv = a > st.gateEnv ? gateAtt * st.gateEnv + (1.0f - gateAtt) * a
                                        : gateRel * st.gateEnv + (1.0f - gateRel) * a;
            const float thr = 0.001f + 0.08f * s * s;
            float target;
            if (st.gateEnv < thr)             target = 0.0f;
            else if (st.gateEnv < thr * 2.0f) target = chopOn ? 1.0f : 0.0f;   // sputter zone
            else                              target = (s > 0.6f && ! chopOn && nextRandomCheap (st) < (s - 0.6f) * 0.5f) ? 0.3f : 1.0f;
            st.gate = gateSm * st.gate + (1.0f - gateSm) * target;
            y *= st.gate;
        }
        else st.gate = 1.0f;

        return y;
    }

private:
    struct Channel
    {
        float sagEnv = 0.0f, gateEnv = 0.0f, gate = 1.0f;
        float dcX = 0.0f, dcY = 0.0f, gritX = 0.0f, gritY = 0.0f, tone = 0.0f;
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
    float fs = 192000.0f;
    float g1 = 1.0f, g2 = 1.0f, comp = 1.0f, toneA = 0.0f;
    bool useMore = false;
    float sagAtt = 0, sagRel = 0, gateAtt = 0, gateRel = 0, gateSm = 0, dcR = 0.9999f, gritA = 0;
    uint32_t seed = 0x9E3779B9u;
    int chopLeft = 0;
    bool chopOn = true;
};

} // namespace slap
