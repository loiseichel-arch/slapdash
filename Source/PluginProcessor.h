#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include "dsp/Saturation.h"
#include "Parameters.h"

class SlapdashProcessor : public juce::AudioProcessor
{
public:
    SlapdashProcessor();
    ~SlapdashProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isMono() const { return getTotalNumOutputChannels() < 2; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr int B = params::maxBands;
    static constexpr int osOrder = 2;            // 2^2 = 4x oversampling

    // Direct pointers to the parameter values (no lookups on the audio thread).
    struct BandRaw
    {
        std::atomic<float> *type, *drive, *more, *sag, *grit, *mix, *gain, *pan, *solo, *mute, *bypass, *lo, *hi;
    };
    struct Raw
    {
        std::atomic<float> *input, *lowCut, *highCut, *monoMaker, *mix, *output, *autoGain, *speakerSim, *volt,
                           *bands, *layer;
        std::array<std::atomic<float>*, 3> xover {};
        std::array<BandRaw, B> band {};
    } raw {};

    struct Band
    {
        slap::BandSaturator sat;
        juce::SmoothedValue<float> mix, gain, panL, panR, audible;
        float drive = 0.3f, more = 0.0f, sag = 0.0f, grit = 0.0f;   // smoothed per block
        juce::dsp::LinkwitzRileyFilter<float> layerHp, layerLp;
        std::array<juce::dsp::LinkwitzRileyFilter<float>, 3> allpass;  // phase match for the splits above it
    };

    void updateSpeakerSim();

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    std::array<juce::dsp::LinkwitzRileyFilter<float>, 3> splits;
    std::array<Band, B> bands;

    juce::dsp::StateVariableTPTFilter<float> lowCutFilter, highCutFilter;
    juce::dsp::LinkwitzRileyFilter<float> monoSplit;
    static constexpr int spkStages = 6;
    std::array<std::array<juce::dsp::IIR::Filter<float>, spkStages>, 2> speaker;
    juce::dsp::DryWetMixer<float> dryWet { 4096 };

    juce::SmoothedValue<float> inGain, outGain, autoGainSm;
    double sr = 44100.0, osRate = 176400.0;
    int numCh = 2;
    double inMs = 0.0, wetMs = 0.0;
    float autoTarget = 1.0f;
    std::array<float, 3> lastSplitHz { -1.0f, -1.0f, -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlapdashProcessor)
};
