// PresetBar.h
// The presets row of the header (same file in every Planet Gloria plugin):
//   [<] [ name * ] [>]  [Save] [Delete]
// Click on the name: the list (Init, the presets and their folders, and
// "Open presets folder"). "*" = a setting has moved since the preset was
// loaded or saved. Save asks for a name ("Drums/Snare" makes a folder).

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Sketch.h"
#include "../PresetManager.h"

// A small question over the whole window: a title, maybe a text field, two buttons.
class PresetDialog : public juce::Component
{
public:
    PresetDialog (const juce::String& titleText, const juce::String& okText, bool withField, const juce::String& initialText,
                  std::function<void (bool, juce::String)> done)
        : title (titleText), callback (std::move (done))
    {
        if (withField)
        {
            field = std::make_unique<juce::TextEditor>();
            field->setText (initialText, false);
            field->setFont (fonts->hand (17.0f));
            field->setColour (juce::TextEditor::backgroundColourId, sketch::colour::knob);
            field->setColour (juce::TextEditor::textColourId, sketch::colour::ink);
            field->setColour (juce::TextEditor::outlineColourId, sketch::colour::soft);
            field->setColour (juce::TextEditor::focusedOutlineColourId, sketch::colour::accent);
            field->onReturnKey = [this] { finish (true); };
            field->onEscapeKey = [this] { finish (false); };
            addAndMakeVisible (*field);
        }
        ok.setButtonText (okText);
        cancel.setButtonText ("Cancel");
        ok.onClick = [this] { finish (true); };
        cancel.onClick = [this] { finish (false); };
        addAndMakeVisible (ok);
        addAndMakeVisible (cancel);
        setWantsKeyboardFocus (true);
    }

    // Puts it over the whole window (the caller keeps it alive).
    void showOver (juce::Component& window)
    {
        window.addAndMakeVisible (this);
        setBounds (window.getLocalBounds());
        toFront (true);
        if (field != nullptr) { field->grabKeyboardFocus(); field->selectAll(); }
        else grabKeyboardFocus();
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { finish (false); return true; }
        if (k == juce::KeyPress::returnKey) { finish (true); return true; }
        return false;
    }

    void resized() override
    {
        card = juce::Rectangle<int> (360, field != nullptr ? 170 : 130).withCentre (getLocalBounds().getCentre());
        auto r = card.reduced (20, 16);
        r.removeFromTop (34);
        if (field != nullptr) { field->setBounds (r.removeFromTop (30)); r.removeFromTop (14); }
        auto row = r.removeFromBottom (34);
        cancel.setBounds (row.removeFromRight (100));
        row.removeFromRight (10);
        ok.setBounds (row.removeFromRight (100));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (sketch::colour::paper.withAlpha (0.75f));
        sketch::box (g, card.toFloat(), 4401, sketch::colour::knob, 2.0f);
        g.setColour (sketch::colour::ink);
        g.setFont (fonts->hand (18.0f));
        g.drawFittedText (title, card.reduced (20, 14).removeFromTop (30), juce::Justification::centredLeft, 2);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! card.contains (e.getPosition())) finish (false);   // click outside = cancel
    }

private:
    struct Pill : juce::Button
    {
        Pill() : juce::Button ({}) {}
        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            const auto r = getLocalBounds().toFloat().reduced (2.0f);
            sketch::box (g, r, sketch::seedOf (getButtonText()), down ? sketch::colour::accent : (over ? sketch::colour::tint : sketch::colour::knob), 1.8f);
            g.setColour (down ? sketch::colour::knob : sketch::colour::ink);
            g.setFont (fonts->hand (16.0f));
            g.drawText (getButtonText(), r, juce::Justification::centred, false);
        }
        juce::SharedResourcePointer<sketch::Fonts> fonts;
    };

    // The answer is given after this call returns (the owner may delete the dialog then).
    void finish (bool accepted)
    {
        if (answered) return;
        answered = true;
        setVisible (false);
        const auto text = field != nullptr ? field->getText() : juce::String();
        auto cb = callback;
        juce::MessageManager::callAsync ([cb, accepted, text] { if (cb) cb (accepted, text); });
    }

    bool answered = false;

    juce::String title;
    std::function<void (bool, juce::String)> callback;
    std::unique_ptr<juce::TextEditor> field;
    Pill ok, cancel;
    juce::Rectangle<int> card;
    juce::SharedResourcePointer<sketch::Fonts> fonts;
};

class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (PresetManager& m) : presets (m)
    {
        prev.kind = Btn::Kind::prev;   prev.setTooltip ("previous preset");
        next.kind = Btn::Kind::next;   next.setTooltip ("next preset");
        name.kind = Btn::Kind::name;   name.setTooltip ("presets: click for the list");
        save.kind = Btn::Kind::text;   save.setButtonText ("Save");     save.setTooltip ("save the settings as a preset");
        del.kind  = Btn::Kind::text;   del.setButtonText ("Delete");    del.setTooltip ("delete this preset");
        prev.onClick = [this] { presets.step (-1); refresh(); };
        next.onClick = [this] { presets.step (+1); refresh(); };
        name.onClick = [this] { showList(); };
        save.onClick = [this] { askSave(); };
        del.onClick  = [this] { askDelete(); };
        for (auto* b : { &prev, &name, &next, &save, &del }) addAndMakeVisible (b);
        refresh();
        startTimerHz (5);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        del.setBounds (r.removeFromRight (70));
        r.removeFromRight (4);
        save.setBounds (r.removeFromRight (58));
        r.removeFromRight (10);
        prev.setBounds (r.removeFromLeft (28));
        next.setBounds (r.removeFromRight (28));
        name.setBounds (r.reduced (3, 0));
    }

private:
    struct Btn : juce::Button
    {
        enum class Kind { prev, next, name, text };
        Kind kind = Kind::text;
        bool modified = false;
        Btn() : juce::Button ({}) {}
        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            const auto r = getLocalBounds().toFloat().reduced (1.5f);
            const bool on = isEnabled();
            if (kind == Kind::prev || kind == Kind::next)
            {
                const float cx = r.getCentreX(), cy = r.getCentreY(), s = 6.0f, d = kind == Kind::prev ? 1.0f : -1.0f;
                juce::Path p = sketch::line ({ cx + d * s * 0.6f, cy - s }, { cx - d * s * 0.6f, cy }, kind == Kind::prev ? 11u : 12u, 0.2f);
                p.addPath (sketch::line ({ cx - d * s * 0.6f, cy }, { cx + d * s * 0.6f, cy + s }, kind == Kind::prev ? 13u : 14u, 0.2f));
                sketch::stroke (g, p, over ? 2.6f : 2.0f, down ? sketch::colour::accent : sketch::colour::ink);
                return;
            }
            sketch::box (g, r, kind == Kind::name ? 4410u : sketch::seedOf (getButtonText()),
                         down ? sketch::colour::accent : (over && on ? sketch::colour::tint : sketch::colour::knob), 1.8f);
            g.setColour (down ? sketch::colour::knob : (on ? sketch::colour::ink : sketch::colour::soft.withAlpha (0.5f)));
            g.setFont (fonts->hand (kind == Kind::name ? 17.0f : 15.0f));
            auto t = r.reduced (10.0f, 0.0f);
            if (kind == Kind::name)
            {
                g.drawFittedText (getButtonText() + (modified ? " *" : ""), t.withTrimmedRight (14.0f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
                // little triangle: it opens a list
                juce::Path tri;
                const float x = r.getRight() - 14.0f, y = r.getCentreY();
                tri.addTriangle (x - 4.0f, y - 2.0f, x + 4.0f, y - 2.0f, x, y + 3.0f);
                g.setColour (sketch::colour::soft);
                g.fillPath (tri);
            }
            else g.drawText (getButtonText(), t, juce::Justification::centred, false);
        }
        juce::SharedResourcePointer<sketch::Fonts> fonts;
    };

    void timerCallback() override { refresh(); }

    void refresh()
    {
        const auto n = presets.currentName();
        const bool m = presets.isModified();
        if (n != name.getButtonText() || m != name.modified)
        {
            name.setButtonText (n);
            name.modified = m;
            name.repaint();
        }
        const bool canDelete = presets.currentFile().existsAsFile();
        if (del.isEnabled() != canDelete) del.setEnabled (canDelete);
    }

    juce::Component& window() { return *getTopLevelComponent(); }

    void showList()
    {
        juce::PopupMenu menu;
        const auto files = presets.list();
        const auto current = presets.currentFile();
        menu.addItem (1, PresetManager::initName, true, presets.isInit());
        if (! files.isEmpty()) menu.addSeparator();

        // sub-folders become sub-menus
        std::map<juce::String, juce::PopupMenu> subs;
        juce::StringArray order;
        for (int i = 0; i < files.size(); ++i)
        {
            const auto rel = presets.nameOf (files[i]);
            const auto dir = rel.upToLastOccurrenceOf ("/", false, false);
            const auto leaf = rel.fromLastOccurrenceOf ("/", false, false);
            if (dir.isEmpty()) continue;
            if (! subs.count (dir)) order.add (dir);
            subs[dir].addItem (100 + i, leaf, true, files[i] == current);
        }
        for (const auto& d : order) menu.addSubMenu (d, subs[d]);
        for (int i = 0; i < files.size(); ++i)
            if (! presets.nameOf (files[i]).contains ("/"))
                menu.addItem (100 + i, presets.nameOf (files[i]), true, files[i] == current);
        if (files.isEmpty()) menu.addItem (2, "(no presets yet: Save makes one)", false, false);
        menu.addSeparator();
        menu.addItem (3, "Open presets folder");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&name),
                            [this, files] (int r)
                            {
                                if (r == 1) presets.loadInit();
                                else if (r == 3)
                                {
                                    presets.folder().createDirectory();
                                    presets.folder().startAsProcess();
                                }
                                else if (r >= 100 && r - 100 < files.size()) presets.load (files[r - 100]);
                                refresh();
                            });
    }

    // One question at a time, owned here (it goes when the window closes).
    void ask (const juce::String& title, const juce::String& okText, bool withField, const juce::String& initial,
              std::function<void (PresetBar&, bool, juce::String)> then)
    {
        juce::Component::SafePointer<PresetBar> safe (this);
        dialog = std::make_unique<PresetDialog> (title, okText, withField, initial,
            [safe, then] (bool okPressed, juce::String text)
            {
                if (safe == nullptr) return;
                safe->dialog.reset();
                then (*safe, okPressed, text);
                safe->refresh();
            });
        dialog->showOver (window());
    }

    void askSave()
    {
        const bool named = presets.currentFile() != juce::File();
        ask ("Save preset as:", "Save", true, named ? presets.currentName() : juce::String(),
             [] (PresetBar& bar, bool okPressed, juce::String text)
             {
                 if (! okPressed || text.trim().isEmpty()) return;
                 const auto f = bar.presets.fileFor (text);
                 if (f == juce::File()) return;
                 if (f.existsAsFile() && f != bar.presets.currentFile())
                 {
                     bar.ask ("\"" + bar.presets.nameOf (f) + "\" exists. Replace it?", "Replace", false, {},
                              [text] (PresetBar& b, bool yes, juce::String) { if (yes) b.presets.save (text); });
                     return;
                 }
                 bar.presets.save (text);
             });
    }

    void askDelete()
    {
        if (! presets.currentFile().existsAsFile()) return;
        ask ("Delete the preset \"" + presets.currentName() + "\"?", "Delete", false, {},
             [] (PresetBar& bar, bool yes, juce::String) { if (yes) bar.presets.deleteCurrent(); });
    }

    PresetManager& presets;
    Btn prev, next, name, save, del;
    std::unique_ptr<PresetDialog> dialog;
};
