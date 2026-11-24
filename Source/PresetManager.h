// PresetManager.h
// Presets of a Planet Gloria plugin (same file in every plugin).
//
// - One file per preset: Documents/Planet Gloria/<Plugin>/Presets/<name>.preset
//   (sub-folders allowed: "Drums/Snare" is saved in Presets/Drums/Snare.preset).
//   The files can be copied, renamed, sent, sorted in folders by hand.
// - A preset recalls every parameter of the plugin except the ones it is told
//   to leave alone (the "on" switch). A parameter missing from an older preset
//   goes to its default value.
// - "Init" = every parameter at its default.
// - The name of the current preset and its file are kept in the session
//   (properties of the plugin state), and the window shows "*" when a
//   setting has moved since it was loaded or saved.
// All of it runs on the message thread (the window).

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <map>

class PresetManager
{
public:
    static constexpr const char* extension = ".preset";
    static constexpr const char* initName = "Init";

    PresetManager (juce::AudioProcessorValueTreeState& state, juce::String pluginName, juce::StringArray idsLeftAlone)
        : apvts (state), plugin (std::move (pluginName)), excluded (std::move (idsLeftAlone)) {}

    // Wraps the moment all the parameters change (Slapdash: Link suspended).
    std::function<void (const std::function<void()>&)> aroundLoad;

    juce::File folder() const
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Planet Gloria").getChildFile (plugin).getChildFile ("Presets");
    }

    // Every preset file, sorted by name (sub-folders first level by level).
    juce::Array<juce::File> list() const
    {
        juce::Array<juce::File> files;
        const auto dir = folder();
        if (dir.isDirectory())
            for (const auto& entry : juce::RangedDirectoryIterator (dir, true, juce::String ("*") + extension, juce::File::findFiles))
                files.add (entry.getFile());
        struct Sorter
        {
            juce::File base;
            int compareElements (const juce::File& a, const juce::File& b) const
            {
                return a.getRelativePathFrom (base).compareNatural (b.getRelativePathFrom (base));
            }
        } sorter { dir };
        files.sort (sorter);
        return files;
    }

    // "Drums/Snare" for Presets/Drums/Snare.preset
    juce::String nameOf (const juce::File& f) const
    {
        return f.getRelativePathFrom (folder()).replaceCharacter ('\\', '/').upToLastOccurrenceOf (extension, false, true);
    }

    juce::String currentName() const { return apvts.state.getProperty ("presetName", initName).toString(); }
    juce::File currentFile() const
    {
        const auto path = apvts.state.getProperty ("presetFile", juce::String()).toString();
        return path.isEmpty() ? juce::File() : juce::File (path);
    }
    bool isInit() const { return currentFile() == juce::File() && currentName() == initName; }

    // A setting has moved since the preset was loaded or saved.
    bool isModified()
    {
        refreshReference();
        for (auto* p : params())
        {
            const auto it = reference.find (p->getParameterID());
            const float ref = it != reference.end() ? p->convertTo0to1 (it->second) : p->getDefaultValue();
            if (std::abs (p->getValue() - ref) > 1.0e-4f) return true;
        }
        return false;
    }

    void loadInit()
    {
        apply ({});
        setCurrent (initName, {});
        reference.clear();
        referenceKey = "init";
    }

    bool load (const juce::File& f)
    {
        std::map<juce::String, float> values;
        if (! read (f, values)) return false;
        apply (values);
        setCurrent (nameOf (f), f);
        reference = values;
        referenceKey = f.getFullPathName();
        return true;
    }

    // Next (+1) / previous (-1), Init included, wrapping round.
    void step (int direction)
    {
        const auto files = list();
        const int count = files.size() + 1;          // 0 = Init
        int index = isInit() ? 0 : files.indexOf (currentFile()) + 1;
        if (index <= 0 && ! isInit()) index = direction > 0 ? 0 : 1;   // current one gone: start from the ends
        index = ((index + direction) % count + count) % count;
        if (index == 0) loadInit();
        else            load (files[index - 1]);
    }

    // The file a name would be saved to ("" if the name is not usable).
    juce::File fileFor (const juce::String& name) const
    {
        juce::File f = folder();
        bool any = false;
        for (auto part : juce::StringArray::fromTokens (name.replaceCharacter ('\\', '/'), "/", ""))
        {
            part = juce::File::createLegalFileName (part.trim());
            if (part.isEmpty() || part == "." || part == "..") continue;
            f = f.getChildFile (part);
            any = true;
        }
        return any ? f.withFileExtension (extension) : juce::File();
    }

    bool save (const juce::String& name)
    {
        const auto f = fileFor (name);
        if (f == juce::File()) return false;
        juce::XmlElement xml ("PlanetGloriaPreset");
        xml.setAttribute ("plugin", plugin);
        xml.setAttribute ("version", JucePlugin_VersionString);
        std::map<juce::String, float> values;
        for (auto* p : params())
        {
            const float v = p->convertFrom0to1 (p->getValue());
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", p->getParameterID());
            e->setAttribute ("value", (double) v);
            values[p->getParameterID()] = v;
        }
        if (! f.getParentDirectory().createDirectory().wasOk() || ! xml.writeTo (f)) return false;
        setCurrent (nameOf (f), f);
        reference = values;
        referenceKey = f.getFullPathName();
        return true;
    }

    bool deleteCurrent()
    {
        const auto f = currentFile();
        if (f == juce::File() || ! f.existsAsFile() || ! f.deleteFile()) return false;
        // empty sub-folders go too
        for (auto d = f.getParentDirectory(); d != folder() && d.isAChildOf (folder()) && d.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0; d = d.getParentDirectory())
            d.deleteFile();
        setCurrent ("Untitled", {});
        reference.clear();
        referenceKey = "deleted";
        return true;
    }

private:
    juce::Array<juce::RangedAudioParameter*> params() const
    {
        juce::Array<juce::RangedAudioParameter*> out;
        for (auto* ap : apvts.processor.getParameters())
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (ap))
                if (! excluded.contains (p->getParameterID()))
                    out.add (p);
        return out;
    }

    bool read (const juce::File& f, std::map<juce::String, float>& values) const
    {
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName ("PlanetGloriaPreset")) return false;
        for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
            values[e->getStringAttribute ("id")] = (float) e->getDoubleAttribute ("value");
        return true;
    }

    void apply (const std::map<juce::String, float>& values)
    {
        auto doIt = [&]
        {
            for (auto* p : params())
            {
                const auto it = values.find (p->getParameterID());
                const float target = it != values.end() ? p->convertTo0to1 (it->second) : p->getDefaultValue();
                if (std::abs (p->getValue() - target) < 1.0e-7f) continue;
                p->beginChangeGesture();
                p->setValueNotifyingHost (target);
                p->endChangeGesture();
            }
        };
        if (aroundLoad) aroundLoad (doIt);
        else doIt();
    }

    void setCurrent (const juce::String& name, const juce::File& f)
    {
        apvts.state.setProperty ("presetName", name, nullptr);
        apvts.state.setProperty ("presetFile", f == juce::File() ? juce::String() : f.getFullPathName(), nullptr);
    }

    // After a session is opened, read the file of its preset again to know
    // what "modified" means.
    void refreshReference()
    {
        const auto f = currentFile();
        const juce::String key = f != juce::File() ? f.getFullPathName() : (currentName() == initName ? juce::String ("init") : juce::String ("none"));
        if (key == referenceKey) return;
        referenceKey = key;
        reference.clear();
        if (f != juce::File()) read (f, reference);
        // "none" (no file, not Init): defaults as reference
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String plugin;
    juce::StringArray excluded;
    std::map<juce::String, float> reference;   // values of the loaded preset
    juce::String referenceKey;
};
