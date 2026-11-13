#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace pid = params::id;

SlapdashProcessor::SlapdashProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SlapdashState", params::createLayout())
{
    auto get = [this] (const juce::String& p)
    {
        auto* v = apvts.getRawParameterValue (p);
        jassert (v != nullptr);
        return v;
    };

    raw.input  = get (pid::input);   raw.lowCut   = get (pid::lowCut);   raw.highCut    = get (pid::highCut);
    raw.monoMaker = get (pid::monoMaker); raw.mix = get (pid::mix);      raw.output     = get (pid::output);
    raw.autoGain = get (pid::autoGain); raw.speakerSim = get (pid::speakerSim); raw.volt = get (pid::volt);
    raw.bands  = get (pid::bands);   raw.layer    = get (pid::layer);   raw.active = get (pid::active);
    for (int i = 0; i < 3; ++i) raw.xover[(size_t) i] = get (pid::xover[(size_t) i]);

    using namespace params;
    for (int b = 0; b < B; ++b)
    {
        auto& r = raw.band[(size_t) b];
        r.type  = get (bandId (b, band::type));  r.drive = get (bandId (b, band::drive));
        r.more  = get (bandId (b, band::more));  r.sag   = get (bandId (b, band::sag));
        r.grit  = get (bandId (b, band::grit));  r.mix   = get (bandId (b, band::mix));
        r.gain  = get (bandId (b, band::gain));  r.pan   = get (bandId (b, band::pan));
        r.solo  = get (bandId (b, band::solo));  r.mute  = get (bandId (b, band::mute));
        r.bypass = get (bandId (b, band::bypass));
        r.lo    = get (bandId (b, band::lo));    r.hi    = get (bandId (b, band::hi));
    }
}

bool SlapdashProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void SlapdashProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    numCh = juce::jlimit (1, 2, getTotalNumOutputChannels());
    const auto block = (juce::uint32) juce::jmax (1, samplesPerBlock);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, (size_t) osOrder, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing (block);
    osRate = sr * (double) (1 << osOrder);

    const juce::dsp::ProcessSpec osSpec { osRate, block * (juce::uint32) (1 << osOrder), (juce::uint32) numCh };
    const juce::dsp::ProcessSpec spec   { sr, block, (juce::uint32) numCh };

    using LR = juce::dsp::LinkwitzRileyFilterType;
    for (auto& s : splits) { s.prepare (osSpec); s.setType (LR::lowpass); }
    lastSplitHz = { -1.0f, -1.0f, -1.0f };

    for (auto& b : bands)
    {
        b.sat.prepare (osRate);
        b.layerHp.prepare (osSpec); b.layerHp.setType (LR::highpass);
        b.layerLp.prepare (osSpec); b.layerLp.setType (LR::lowpass);
        for (auto& ap : b.allpass) { ap.prepare (osSpec); ap.setType (LR::allpass); }
        for (auto* v : { &b.mix, &b.gain, &b.panL, &b.panR, &b.audible })
            v->reset (osRate, 0.02);
        b.mix.setCurrentAndTargetValue (1.0f);
        b.gain.setCurrentAndTargetValue (1.0f);
        b.panL.setCurrentAndTargetValue (1.0f);
        b.panR.setCurrentAndTargetValue (1.0f);
        b.audible.setCurrentAndTargetValue (1.0f);
    }

    lowCutFilter.prepare (spec);  lowCutFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    highCutFilter.prepare (spec); highCutFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lowCutFilter.setResonance (0.7071f);
    highCutFilter.setResonance (0.7071f);
    monoSplit.prepare (spec); monoSplit.setType (LR::lowpass);

    const juce::dsp::ProcessSpec monoSpec { sr, block, 1 };
    for (auto& chain : speaker)
        for (auto& f : chain) f.prepare (monoSpec);
    updateSpeakerSim();

    dryWet.prepare (spec);
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    const float latency = oversampling->getLatencyInSamples();
    dryWet.setWetLatency (latency);
    setLatencySamples (juce::roundToInt (latency));

    inGain.reset (sr, 0.03);     inGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (raw.input->load()));
    outGain.reset (sr, 0.03);    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (raw.output->load()));
    autoGainSm.reset (sr, 0.3);  autoGainSm.setCurrentAndTargetValue (1.0f);
    inMs = wetMs = 0.0;
    autoTarget = 1.0f;
}

// A speaker on its own (no amp): low end tightened, a little box resonance,
// the mid scoop and presence peak of a guitar cone, then a steep top roll-off.
void SlapdashProcessor::updateSpeakerSim()
{
    using C = juce::dsp::IIR::Coefficients<float>;
    const std::array<C::Ptr, spkStages> c {
        C::makeHighPass  (sr, 75.0,  0.71f),
        C::makePeakFilter (sr, 115.0, 1.0f, 1.6f),
        C::makePeakFilter (sr, 450.0, 1.1f, 0.6f),
        C::makePeakFilter (sr, 2400.0, 1.4f, 1.8f),
        C::makeLowPass   (sr, 4800.0, 0.71f),
        C::makeLowPass   (sr, 5200.0, 0.9f)
    };
    for (auto& chain : speaker)
        for (int i = 0; i < spkStages; ++i)
            chain[(size_t) i].coefficients = c[(size_t) i];
}

void SlapdashProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    for (auto c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (n == 0 || oversampling == nullptr)
        return;

    const int nch = juce::jmin (numCh, buffer.getNumChannels());
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) nch, (size_t) n);

    // ---- Dry copy for the global Mix, and input level for Auto gain ----
    dryWet.pushDrySamples (block);

    auto blockMs = [&] ()
    {
        double sum = 0.0;
        for (int c = 0; c < nch; ++c)
        {
            const float* d = buffer.getReadPointer (c);
            for (int i = 0; i < n; ++i) sum += (double) d[i] * d[i];
        }
        return sum / (double) (n * nch);
    };
    const double agAlpha = std::exp (-(double) n / (1.5 * sr));   // ~1.5 s average
    const double inBlock = blockMs();
    inMs = agAlpha * inMs + (1.0 - agAlpha) * inBlock;

    // ---- Input gain ----
    inGain.setTargetValue (juce::Decibels::decibelsToGain (raw.input->load()));
    for (int i = 0; i < n; ++i)
    {
        const float g = inGain.getNextValue();
        for (int c = 0; c < nch; ++c) buffer.getWritePointer (c)[i] *= g;
    }

    // ---- Band settings for this block ----
    const int nb = juce::jlimit (1, B, juce::roundToInt (raw.bands->load()));
    const bool layer = raw.layer->load() > 0.5f;
    const float starve = 1.0f - juce::jlimit (0.0f, 9.0f, raw.volt->load()) / 9.0f;
    const bool stereo = nch == 2;

    // Split points: sorted, at least a little apart.
    std::array<float, 3> split {};
    for (int k = 0; k < nb - 1; ++k)
    {
        float f = raw.xover[(size_t) k]->load();
        if (k > 0) f = juce::jmax (f, split[(size_t) k - 1] * 1.1f);
        split[(size_t) k] = juce::jlimit (30.0f, 16000.0f, f);
    }
    if (! layer)
        for (int k = 0; k < nb - 1; ++k)
            if (std::abs (split[(size_t) k] - lastSplitHz[(size_t) k]) > 0.01f)
            {
                splits[(size_t) k].setCutoffFrequency (split[(size_t) k]);
                for (int b = 0; b < k; ++b) bands[(size_t) b].allpass[(size_t) k].setCutoffFrequency (split[(size_t) k]);
                lastSplitHz[(size_t) k] = split[(size_t) k];
            }

    bool anySolo = false;
    for (int b = 0; b < nb; ++b) anySolo = anySolo || raw.band[(size_t) b].solo->load() > 0.5f;

    std::array<bool, B> useHp {}, useLp {}, bypass {};
    for (int b = 0; b < nb; ++b)
    {
        auto& r = raw.band[(size_t) b];
        auto& bd = bands[(size_t) b];

        auto follow = [] (float& v, float target) { v += 0.35f * (target - v); };
        follow (bd.drive, r.drive->load() * 0.01f);
        follow (bd.more,  r.more->load()  * 0.01f);
        follow (bd.sag,   r.sag->load()   * 0.01f);
        follow (bd.grit,  r.grit->load()  * 0.01f);

        slap::BandSettings s;
        s.type   = (slap::Type) juce::jlimit (0, 3, juce::roundToInt (r.type->load()));
        s.drive  = bd.drive; s.more = bd.more; s.sag = bd.sag; s.grit = bd.grit;
        s.starve = starve;
        bd.sat.setSettings (s);

        bd.mix.setTargetValue (r.mix->load() * 0.01f);
        bd.gain.setTargetValue (juce::Decibels::decibelsToGain (r.gain->load()));
        const float pan = stereo ? juce::jlimit (-1.0f, 1.0f, r.pan->load() * 0.01f) : 0.0f;
        bd.panL.setTargetValue (pan > 0.0f ? 1.0f - pan : 1.0f);   // balance law: centre = unchanged
        bd.panR.setTargetValue (pan < 0.0f ? 1.0f + pan : 1.0f);
        const bool audible = r.mute->load() < 0.5f && (! anySolo || r.solo->load() > 0.5f);
        bd.audible.setTargetValue (audible ? 1.0f : 0.0f);
        bypass[(size_t) b] = r.bypass->load() > 0.5f;

        if (layer)
        {
            const float lo = r.lo->load(), hi = r.hi->load();
            useHp[(size_t) b] = lo > 20.5f;
            useLp[(size_t) b] = hi < 19900.0f;
            if (useHp[(size_t) b]) bd.layerHp.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, lo));
            if (useLp[(size_t) b]) bd.layerLp.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, hi));
        }
    }

    // ---- Oversampled band section ----
    auto up = oversampling->processSamplesUp (block);
    const int nOs = (int) up.getNumSamples();
    std::array<float*, 2> io { up.getChannelPointer (0), stereo ? up.getChannelPointer (1) : nullptr };

    for (int i = 0; i < nOs; ++i)
    {
        std::array<float, B> mixV {}, gainL {}, gainR {};
        for (int b = 0; b < nb; ++b)
        {
            auto& bd = bands[(size_t) b];
            bd.sat.tick();
            mixV[(size_t) b] = bd.mix.getNextValue();
            const float g = bd.gain.getNextValue() * bd.audible.getNextValue();
            gainL[(size_t) b] = g * bd.panL.getNextValue();
            gainR[(size_t) b] = g * bd.panR.getNextValue();
        }

        for (int c = 0; c < nch; ++c)
        {
            const float x = io[(size_t) c][i];
            std::array<float, B> in {};

            if (layer)
            {
                for (int b = 0; b < nb; ++b)
                {
                    auto& bd = bands[(size_t) b];
                    float v = x;
                    if (useHp[(size_t) b]) v = bd.layerHp.processSample (c, v);
                    if (useLp[(size_t) b]) v = bd.layerLp.processSample (c, v);
                    in[(size_t) b] = v;
                }
            }
            else
            {
                float rest = x;
                for (int k = 0; k < nb - 1; ++k)
                {
                    float lo = 0.0f, hi = 0.0f;
                    splits[(size_t) k].processSample (c, rest, lo, hi);
                    in[(size_t) k] = lo;
                    rest = hi;
                }
                in[(size_t) nb - 1] = rest;

                // Bands below a split get its allpass, so all bands stay in phase.
                for (int b = 0; b < nb - 1; ++b)
                    for (int k = b + 1; k < nb - 1; ++k)
                        in[(size_t) b] = bands[(size_t) b].allpass[(size_t) k].processSample (c, in[(size_t) b]);
            }

            float sum = 0.0f;
            for (int b = 0; b < nb; ++b)
            {
                auto& bd = bands[(size_t) b];
                const float dry = in[(size_t) b];
                const float wet = bypass[(size_t) b] ? dry : bd.sat.process (c, dry);
                const float y = dry + mixV[(size_t) b] * (wet - dry);
                sum += y * (c == 0 ? gainL[(size_t) b] : gainR[(size_t) b]);
            }
            io[(size_t) c][i] = sum;
        }
    }

    oversampling->processSamplesDown (block);

    // ---- Master: Low cut, High cut ----
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    const float lowCut = raw.lowCut->load(), highCut = raw.highCut->load();
    if (lowCut > 20.5f)
    {
        lowCutFilter.setCutoffFrequency (lowCut);
        lowCutFilter.process (ctx);
    }
    else lowCutFilter.reset();
    if (highCut < 19900.0f)
    {
        highCutFilter.setCutoffFrequency (juce::jmin (highCut, (float) sr * 0.45f));
        highCutFilter.process (ctx);
    }
    else highCutFilter.reset();

    // ---- Speaker sim ----
    if (raw.speakerSim->load() > 0.5f)
    {
        for (int c = 0; c < nch; ++c)
        {
            float* d = buffer.getWritePointer (c);
            for (auto& f : speaker[(size_t) c])
                for (int i = 0; i < n; ++i) d[i] = f.processSample (d[i]);
        }
    }
    else for (auto& chain : speaker) for (auto& f : chain) f.reset();

    // ---- Mono maker: everything below the frequency in mono ----
    const float monoHz = raw.monoMaker->load();
    if (stereo && monoHz >= 20.0f)
    {
        monoSplit.setCutoffFrequency (monoHz);
        float* l = buffer.getWritePointer (0);
        float* r = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            float lLo, lHi, rLo, rHi;
            monoSplit.processSample (0, l[i], lLo, lHi);
            monoSplit.processSample (1, r[i], rLo, rHi);
            const float m = 0.5f * (lLo + rLo);
            l[i] = m + lHi;
            r[i] = m + rHi;
        }
    }
    else monoSplit.reset();

    // ---- Auto gain: slow match of the output level to the input level ----
    const double wetBlock = blockMs();
    wetMs = agAlpha * wetMs + (1.0 - agAlpha) * wetBlock;
    if (raw.autoGain->load() > 0.5f)
    {
        if (inMs > 1.0e-7 && wetMs > 1.0e-9)   // ignore silence: hold the last gain
            autoTarget = (float) juce::jlimit (0.0625, 16.0, std::sqrt (inMs / wetMs));
        autoGainSm.setTargetValue (autoTarget);
    }
    else
    {
        autoTarget = 1.0f;
        autoGainSm.setTargetValue (1.0f);
    }
    for (int i = 0; i < n; ++i)
    {
        const float g = autoGainSm.getNextValue();
        for (int c = 0; c < nch; ++c) buffer.getWritePointer (c)[i] *= g;
    }

    // ---- Global Mix, then Output ----
    // Off = bypass: the dry signal only (delayed like the wet one, no click).
    const bool on = raw.active->load() > 0.5f;
    dryWet.setWetMixProportion (on ? juce::jlimit (0.0f, 1.0f, raw.mix->load() * 0.01f) : 0.0f);
    dryWet.mixWetSamples (block);

    outGain.setTargetValue (on ? juce::Decibels::decibelsToGain (raw.output->load()) : 1.0f);
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        for (int c = 0; c < nch; ++c) buffer.getWritePointer (c)[i] *= g;
    }
}

void SlapdashProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SlapdashProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SlapdashProcessor::createEditor()
{
    return new SlapdashEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SlapdashProcessor();
}
