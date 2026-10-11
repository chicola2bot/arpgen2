#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include "PluginProcessor.h"

namespace ui
{
const juce::Colour kBg (0xff0e1015), kPanel (0xff171a22), kAccent (0xff4fe0c0),
                   kAccent2 (0xffffb454), kDim (0xff8b93a5), kButton (0xff222733);

class ArpLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ArpLookAndFeel()
    {
        setColour (juce::Slider::rotarySliderFillColourId, kAccent);
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2a3040));
        setColour (juce::Slider::thumbColourId, juce::Colours::white);
        setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, kDim);
        setColour (juce::ComboBox::backgroundColourId, kButton);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2f3646));
        setColour (juce::ComboBox::textColourId, juce::Colours::white);
        setColour (juce::ComboBox::arrowColourId, kAccent);
        setColour (juce::PopupMenu::backgroundColourId, kPanel);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent.darker (0.4f));
        setColour (juce::TextButton::buttonColourId, kButton);
        setColour (juce::TextButton::buttonOnColourId, kAccent);
        setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xff08201b));
        setColour (juce::ToggleButton::textColourId, juce::Colours::white);
        setColour (juce::ToggleButton::tickColourId, kAccent);
    }
};

// Read-only piano roll of the active pattern of the shown layer. Click a note to lock / unlock it.
class PianoRoll : public juce::Component, private juce::Timer
{
public:
    explicit PianoRoll (ArpGenProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Map { int lo = 48, hi = 84; float rowH = 10.f, pxPerTick = 0.1f; juce::Rectangle<float> area; };
    Map computeMap (const ag::Pattern*) const;
    void timerCallback() override { repaint(); }
    ArpGenProcessor& proc;
};

// Starts an OS-level file drag of a temporary .mid file (works with FL Studio's Piano Roll / Channel Rack).
class MidiDragButton : public juce::Component
{
public:
    explicit MidiDragButton (juce::String t) : text (std::move (t)) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    std::function<juce::File()> makeFile;

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.f);
        g.setColour (kButton);
        g.fillRoundedRectangle (b, 6.f);
        g.setColour (kAccent);
        g.drawRoundedRectangle (b, 6.f, 1.2f);
        g.setColour (juce::Colours::white);
        g.setFont (14.f);
        g.drawText (text, getLocalBounds(), juce::Justification::centred);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 8 || ! makeFile) return;
        const auto f = makeFile();
        if (! f.existsAsFile()) return;
        dragging = true;
        juce::Component::SafePointer<MidiDragButton> sp (this);
        juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (f.getFullPathName()), false, this,
                                                                    [sp] { if (sp != nullptr) sp->dragging = false; });
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

private:
    juce::String text;
    bool dragging = false;
};
} // namespace ui

class ArpGenEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ArpGenEditor (ArpGenProcessor&);
    ~ArpGenEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    struct Knob  { juce::Slider s; juce::Label l; std::unique_ptr<APVTS::SliderAttachment> a; };
    struct Combo { juce::ComboBox c; juce::Label l; std::unique_ptr<APVTS::ComboBoxAttachment> a; };
    struct Toggle { juce::ToggleButton b; std::unique_ptr<APVTS::ButtonAttachment> a; };

    void initKnob (Knob&, const char* id, const juce::String& text, const juce::String& suffix = {});
    void initCombo (Combo&, const char* id, const juce::String& text);
    void initToggle (Toggle&, const char* id, const juce::String& text);
    void timerCallback() override { refreshButtons(); }
    void refreshButtons();
    juce::File makeTempMidi (int layer);
    void saveMidiDialog();

    ArpGenProcessor& proc;
    ui::ArpLookAndFeel lnf;
    ui::PianoRoll roll;

    juce::TextButton melodyBtn { "Melody" }, arpBtn { "Arpeggio" }, genBtn { "Generate" },
                     regenBtn { "Regenerate (keep locks)" }, playBtn { "Play" }, saveBtn { "Save .mid (shown layer)" };
    juce::TextButton varBtn[3];
    ui::MidiDragButton dragMelody { "Drag Melody MIDI" }, dragArp { "Drag Arp MIDI" };

    Combo root, scale, prog, bars, reg, arp, sound;
    Knob complexity, density, variation, syncopation, swing, gate, volume, cutoff, attack, release, bpm;
    Toggle humanize, avoid, follow;

    juce::Rectangle<int> panel1, panel2, panel3;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArpGenEditor)
};
