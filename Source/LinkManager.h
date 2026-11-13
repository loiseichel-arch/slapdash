// LinkManager.h
// "Link": the bands move together. Turning a knob on one band moves the same
// knob on the other bands by the same amount, from their own value, so the
// offsets between bands are kept. A band that hits its end stops there and
// the others carry on.
// Linked: Drive, More, Sag, Grit, Mix, Gain (not Type, not Pan).
// Only gestures made in the interface are passed on (host automation is
// left alone, it already holds every band's own curve).

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Parameters.h"

class LinkManager : private juce::AudioProcessorParameter::Listener
{
public:
    static constexpr std::array<const char*, 6> linked {
        params::band::drive, params::band::more, params::band::sag,
        params::band::grit,  params::band::mix,  params::band::gain };

    explicit LinkManager (juce::AudioProcessorValueTreeState& s)
    {
        linkOn = s.getRawParameterValue (params::id::link);
        bands  = s.getRawParameterValue (params::id::bands);
        for (int b = 0; b < B; ++b)
            for (int n = 0; n < N; ++n)
            {
                auto* p = s.getParameter (params::bandId (b, linked[(size_t) n]));
                param[(size_t) b][(size_t) n] = p;
                last[(size_t) b][(size_t) n].store (p->convertFrom0to1 (p->getValue()));
                p->addListener (this);
            }
    }

    ~LinkManager() override
    {
        for (auto& row : param)
            for (auto* p : row) p->removeListener (this);
    }

    // While alive, changes are not passed on (copying, removing bands...).
    struct Suspend
    {
        explicit Suspend (LinkManager& m) : lm (m) { ++lm.suspended; }
        ~Suspend() { lm.resync(); --lm.suspended; }
        LinkManager& lm;
    };

private:
    static constexpr int B = params::maxBands, N = (int) linked.size();

    bool active() const
    {
        return linkOn->load() > 0.5f && suspended.load() == 0
            && juce::MessageManager::existsAndIsCurrentThread();
    }
    int numBands() const { return juce::jlimit (1, B, juce::roundToInt (bands->load())); }

    bool find (int index, int& b, int& n) const
    {
        for (b = 0; b < B; ++b)
            for (n = 0; n < N; ++n)
                if (param[(size_t) b][(size_t) n]->getParameterIndex() == index) return true;
        return false;
    }

    void resync()
    {
        for (int b = 0; b < B; ++b)
            for (int n = 0; n < N; ++n)
            {
                auto* p = param[(size_t) b][(size_t) n];
                last[(size_t) b][(size_t) n].store (p->convertFrom0to1 (p->getValue()));
            }
    }

    void parameterValueChanged (int index, float newValue) override
    {
        int b = 0, n = 0;
        if (! find (index, b, n)) return;
        auto* p = param[(size_t) b][(size_t) n];
        const float value = p->convertFrom0to1 (newValue);
        const float delta = value - last[(size_t) b][(size_t) n].load();
        last[(size_t) b][(size_t) n].store (value);

        if (propagating || ! active() || b >= numBands() || std::abs (delta) < 1.0e-6f) return;

        propagating = true;
        for (int j = 0; j < numBands(); ++j)
        {
            if (j == b) continue;
            auto* q = param[(size_t) j][(size_t) n];
            const auto& r = q->getNormalisableRange();
            const float v = juce::jlimit (r.start, r.end, last[(size_t) j][(size_t) n].load() + delta);
            last[(size_t) j][(size_t) n].store (v);
            q->setValueNotifyingHost (q->convertTo0to1 (v));
        }
        propagating = false;
    }

    // Open / close the gestures of the followers, so hosts record them.
    void parameterGestureChanged (int index, bool starting) override
    {
        int b = 0, n = 0;
        if (propagating || ! find (index, b, n)) return;
        if (starting && ! active()) return;

        for (int j = 0; j < B; ++j)
        {
            if (j == b) continue;
            auto& open = gestureOpen[(size_t) j][(size_t) n];
            auto* q = param[(size_t) j][(size_t) n];
            if (starting && j < numBands() && ! open) { propagating = true; q->beginChangeGesture(); propagating = false; open = true; }
            if (! starting && open)                  { propagating = true; q->endChangeGesture();   propagating = false; open = false; }
        }
    }

    std::atomic<float>* linkOn = nullptr;
    std::atomic<float>* bands = nullptr;
    std::array<std::array<juce::RangedAudioParameter*, N>, B> param {};
    std::array<std::array<std::atomic<float>, N>, B> last {};
    std::array<std::array<bool, N>, B> gestureOpen {};
    std::atomic<int> suspended { 0 };
    std::atomic<bool> propagating { false };   // set on the message thread only
};
