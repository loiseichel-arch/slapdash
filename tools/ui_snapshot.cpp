// Linux test helper: runs Slapdash's processor offline and saves pictures of
// the window.
//   - the battery (Slapdash fader) is on the master: the band Mix does not
//     change it (Mix 0 %, fader at 0 V: the sound is still starved), 9 V =
//     no effect
//   - presets: save, change, load, Init, delete; "on" is left alone; with
//     Link on, loading a preset does not copy values to the other bands
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

static void save (juce::Component& c, const juce::String& path)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    juce::File f (path);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("picture %s\n", path.toRawUTF8());
}

static void set (SlapdashProcessor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static float get (SlapdashProcessor& p, const juce::String& id)
{
    auto* prm = p.apvts.getParameter (id);
    return prm->convertFrom0to1 (prm->getValue());
}

// A plucked 110 Hz note every 0.5 s for `seconds`; returns the output.
static juce::AudioBuffer<float> run (SlapdashProcessor& p, double seconds)
{
    const int bs = 256, total = (int) (seconds * 48000.0);
    juce::AudioBuffer<float> out (2, total), buf (2, bs);
    juce::MidiBuffer midi;
    for (int s = 0; s < total; s += bs)
    {
        for (int i = 0; i < bs; ++i)
        {
            const int t = (s + i) % 24000;
            const float v = 0.3f * std::exp (-(float) t / 6000.0f) * (float) std::sin (2.0 * 3.14159265 * 110.0 * t / 48000.0);
            buf.setSample (0, i, v); buf.setSample (1, i, v);
        }
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, s, buf, c, 0, juce::jmin (bs, total - s));
    }
    return out;
}

static double rmsDb (const juce::AudioBuffer<float>& b, int from, int to)
{
    double e = 0;
    for (int i = from; i < to; ++i) e += (double) b.getSample (0, i) * b.getSample (0, i);
    return 10.0 * std::log10 (e / (to - from) + 1e-30);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String out = argc > 1 ? argv[1] : "/tmp";
    bool ok = true;

    // ---- the battery on the master ----
    auto render = [] (float volts, float bandMix, int gateMode)
    {
        SlapdashProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 256);
        p.prepareToPlay (48000.0, 256);
        set (p, params::id::autoGain, 0);
        set (p, params::bandId (0, params::band::mix), bandMix);
        set (p, params::id::volt, volts);
        set (p, params::id::slapMode, (float) gateMode);
        return run (p, 2.0);
    };
    const auto clean = render (9.0f, 0.0f, 0), starvedMix0 = render (0.0f, 0.0f, 0), starvedMix100 = render (0.0f, 100.0f, 0);
    const auto starvedOpen = render (0.0f, 0.0f, 1);
    // note body (first 0.1 s of each pluck) and note end (0.3 .. 0.5 s)
    const double bodyClean = rmsDb (clean, 48000, 52800), bodyStarved = rmsDb (starvedMix0, 48000, 52800);
    const double tailClean = rmsDb (clean, 62400, 72000), tailGate = rmsDb (starvedMix0, 62400, 72000), tailOpen = rmsDb (starvedOpen, 62400, 72000);
    double diff = 0;
    for (int i = 0; i < clean.getNumSamples(); ++i) diff = std::max (diff, (double) std::abs (clean.getSample (0, i) - starvedMix0.getSample (0, i)));
    std::printf ("battery on the master, band Mix 0 %%: 9 V vs 0 V max difference %.3f; note body %.1f -> %.1f dBFS, note end %.1f -> %.1f dBFS (gate), %.1f (open)\n",
                 diff, bodyClean, bodyStarved, tailClean, tailGate, tailOpen);
    std::printf ("   0 V with band Mix 100 %%: note body %.1f dBFS\n", rmsDb (starvedMix100, 48000, 52800));
    ok = ok && diff > 0.01 && tailGate < tailClean - 20.0 && tailOpen > tailGate + 10.0;

    // ---- presets ----
    {
        SlapdashProcessor p;
        auto& pm = p.presets;
        const auto f = pm.fileFor ("Test/Drum bus");
        f.deleteFile();
        set (p, params::id::bands, 2);
        set (p, params::bandId (0, params::band::drive), 70.0f);
        set (p, params::bandId (1, params::band::drive), 20.0f);
        set (p, params::id::volt, 5.0f);
        set (p, params::id::link, 1);
        const bool saved = pm.save ("Test/Drum bus");
        // change things (Link off, so only band 1 moves), and switch the plug off
        set (p, params::id::link, 0);
        set (p, params::bandId (0, params::band::drive), 10.0f);
        set (p, params::id::volt, 9.0f);
        set (p, params::id::active, 0);
        set (p, params::id::link, 1);
        const bool modified = pm.isModified();
        const bool loaded = pm.load (f);
        const bool recalled = std::abs (get (p, params::bandId (0, params::band::drive)) - 70.0f) < 0.1f
                           && std::abs (get (p, params::bandId (1, params::band::drive)) - 20.0f) < 0.1f
                           && std::abs (get (p, params::id::volt) - 5.0f) < 0.01f && juce::roundToInt (get (p, params::id::bands)) == 2;
        const bool onKept = get (p, params::id::active) < 0.5f;
        pm.loadInit();
        const bool init = std::abs (get (p, params::bandId (0, params::band::drive)) - 30.0f) < 0.1f && ! pm.isModified();
        pm.load (f);
        const bool deleted = pm.deleteCurrent() && ! f.existsAsFile();
        std::printf ("presets: saved %d, modified %d, loaded %d, recalled (Link on, no copy) %d, on left alone %d, init %d, deleted %d\n",
                     saved, modified, loaded, recalled, onKept, init, deleted);
        ok = ok && saved && modified && loaded && recalled && onKept && init && deleted;
    }

    // ---- pictures ----
    {
        SlapdashProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 256);
        p.prepareToPlay (48000.0, 256);
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            save (*ed, out + "/sd_one_band.png");
        }
        set (p, params::id::bands, 3);
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            save (*ed, out + "/sd_three_bands.png");
        }
    }
    std::printf ("%s\n", ok ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    return ok ? 0 : 1;
}
