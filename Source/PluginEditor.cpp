#include "PluginEditor.h"
#include "Midi/MidiExporter.h"

using namespace ui;

// ---------------------------------------------------------------------------------------------
PianoRoll::Map PianoRoll::computeMap (const ag::Pattern* pat) const
{
    Map m;
    constexpr float keyW = 34.f;
    m.area = getLocalBounds().toFloat().withTrimmedLeft (keyW).reduced (0.f, 4.f);
    if (pat != nullptr && ! pat->notes.empty())
    {
        int lo = 127, hi = 0;
        for (auto& n : pat->notes) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); }
        lo -= 2; hi += 2;
        if (hi - lo < 23) { const int mid = (hi + lo) / 2; lo = mid - 12; hi = mid + 11; }
        m.lo = std::max (0, lo);
        m.hi = std::min (127, hi);
    }
    m.rowH = m.area.getHeight() / (float) (m.hi - m.lo + 1);
    const int len = pat != nullptr ? pat->lengthTicks : ag::kPPQ * 4;
    m.pxPerTick = m.area.getWidth() / (float) len;
    return m;
}

void PianoRoll::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff0b0d12));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.f);

    const auto pat = proc.getPattern (proc.getCurrentLayer());
    const auto m = computeMap (pat.get());
    constexpr float keyW = 34.f;

    for (int p = m.lo; p <= m.hi; ++p)
    {
        const float y = m.area.getBottom() - (float) (p - m.lo + 1) * m.rowH;
        const int pc = p % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        g.setColour (black ? juce::Colour (0xff10131a) : juce::Colour (0xff151923));
        g.fillRect (m.area.getX(), y, m.area.getWidth(), m.rowH);
        g.setColour (black ? juce::Colour (0xff23262e) : juce::Colour (0xff3a3f4b));
        g.fillRect (4.f, y, keyW - 6.f, m.rowH - 0.5f);
        if (pc == 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.setFont (10.f);
            g.drawText ("C" + juce::String (p / 12 - 1), 4, (int) y, (int) keyW - 8, (int) m.rowH, juce::Justification::centredRight);
        }
    }

    if (! pat) return;

    for (int t = 0; t <= pat->lengthTicks; t += ag::kPPQ)
    {
        const float x = m.area.getX() + (float) t * m.pxPerTick;
        g.setColour (juce::Colours::white.withAlpha (t % (ag::kPPQ * 4) == 0 ? 0.22f : 0.07f));
        g.drawVerticalLine ((int) x, m.area.getY(), m.area.getBottom());
    }

    for (const auto& n : pat->notes)
    {
        const float x = m.area.getX() + (float) n.startTick * m.pxPerTick;
        const float w = std::max (3.f, (float) n.lengthTick * m.pxPerTick - 1.f);
        const float y = m.area.getBottom() - (float) (n.pitch - m.lo + 1) * m.rowH;
        const juce::Rectangle<float> r (x, y + 1.f, w, m.rowH - 2.f);
        g.setColour (n.locked ? kAccent2 : kAccent.withAlpha (0.55f + 0.45f * (float) n.velocity / 127.f));
        g.fillRoundedRectangle (r, 3.f);
        if (n.locked)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (r, 3.f, 1.2f);
        }
    }

    const double ppq = proc.getPlayheadPpq();
    if (ppq >= 0.0)
    {
        const float x = m.area.getX() + (float) (ppq * ag::kPPQ) * m.pxPerTick;
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawVerticalLine ((int) x, m.area.getY(), m.area.getBottom());
    }
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    const int layer = proc.getCurrentLayer();
    const auto pat = proc.getPattern (layer);
    if (! pat) return;
    const auto m = computeMap (pat.get());

    for (int i = (int) pat->notes.size() - 1; i >= 0; --i)
    {
        const auto& n = pat->notes[(size_t) i];
        const float x = m.area.getX() + (float) n.startTick * m.pxPerTick;
        const float w = std::max (3.f, (float) n.lengthTick * m.pxPerTick - 1.f);
        const float y = m.area.getBottom() - (float) (n.pitch - m.lo + 1) * m.rowH;
        if (juce::Rectangle<float> (x, y, w, m.rowH).contains (e.position))
        {
            proc.toggleLock (layer, i);
            repaint();
            return;
        }
    }
}

// ---------------------------------------------------------------------------------------------
ArpGenEditor::ArpGenEditor (ArpGenProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), roll (p)
{
    setLookAndFeel (&lnf);
    setSize (1000, 680);

    addAndMakeVisible (roll);
    for (auto* b : { &melodyBtn, &arpBtn, &genBtn, &regenBtn, &playBtn, &saveBtn, &varBtn[0], &varBtn[1], &varBtn[2] })
        addAndMakeVisible (*b);
    addAndMakeVisible (dragMelody);
    addAndMakeVisible (dragArp);

    melodyBtn.setClickingTogglesState (false);
    arpBtn.setClickingTogglesState (false);
    for (int v = 0; v < 3; ++v)
    {
        varBtn[v].setButtonText (juce::String::charToString ((juce::juce_wchar) ('A' + v)));
        varBtn[v].setClickingTogglesState (false);
        varBtn[v].onClick = [this, v] { proc.selectVariation (proc.getCurrentLayer(), v); refreshButtons(); };
    }
    melodyBtn.onClick = [this] { proc.setCurrentLayer (0); refreshButtons(); };
    arpBtn.onClick    = [this] { proc.setCurrentLayer (1); refreshButtons(); };
    genBtn.onClick    = [this] { proc.generate (proc.getCurrentLayer(), false); };
    regenBtn.onClick  = [this] { proc.generate (proc.getCurrentLayer(), true); };
    playBtn.onClick   = [this] { proc.setInternalPlaying (! proc.isInternalPlaying()); refreshButtons(); };
    saveBtn.onClick   = [this] { saveMidiDialog(); };

    dragMelody.makeFile = [this] { return makeTempMidi (0); };
    dragArp.makeFile    = [this] { return makeTempMidi (1); };

    initCombo (root, "root", "Key");
    initCombo (scale, "scale", "Scale / Mode");
    initCombo (prog, "prog", "Progression");
    initCombo (bars, "bars", "Bars");
    initCombo (reg, "register", "Register");
    initCombo (arp, "arp", "Arp pattern");
    initCombo (sound, "sound", "Sound");

    initKnob (complexity, "complexity", "Complexity");
    initKnob (density, "density", "Density");
    initKnob (variation, "variation", "Variation");
    initKnob (syncopation, "syncopation", "Syncopation");
    initKnob (swing, "swing", "Swing");
    initKnob (gate, "gate", "Duration");
    initKnob (volume, "volume", "Volume");
    initKnob (cutoff, "cutoff", "Filter", " Hz");
    initKnob (attack, "attack", "Attack", " s");
    initKnob (release, "release", "Release", " s");
    initKnob (bpm, "manualBpm", "Manual BPM");

    initToggle (humanize, "humanize", "Humanize");
    initToggle (avoid, "avoidRepeats", "Avoid repeated notes");
    initToggle (follow, "followHost", "Follow host tempo / transport");

    refreshButtons();
    startTimerHz (10);
}

ArpGenEditor::~ArpGenEditor() { setLookAndFeel (nullptr); }

void ArpGenEditor::initKnob (Knob& k, const char* id, const juce::String& text, const juce::String& suffix)
{
    k.s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k.s.setTextValueSuffix (suffix);
    k.l.setText (text, juce::dontSendNotification);
    k.l.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.s);
    addAndMakeVisible (k.l);
    k.a = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, k.s);
}

void ArpGenEditor::initCombo (Combo& c, const char* id, const juce::String& text)
{
    if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c.c.addItemList (cp->choices, 1);
    c.l.setText (text, juce::dontSendNotification);
    addAndMakeVisible (c.c);
    addAndMakeVisible (c.l);
    c.a = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, c.c);
}

void ArpGenEditor::initToggle (Toggle& t, const char* id, const juce::String& text)
{
    t.b.setButtonText (text);
    addAndMakeVisible (t.b);
    t.a = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, t.b);
}

void ArpGenEditor::refreshButtons()
{
    const int l = proc.getCurrentLayer();
    melodyBtn.setToggleState (l == 0, juce::dontSendNotification);
    arpBtn.setToggleState (l == 1, juce::dontSendNotification);
    for (int v = 0; v < 3; ++v)
        varBtn[v].setToggleState (proc.getActiveVariation (l) == v, juce::dontSendNotification);
    playBtn.setButtonText (proc.isInternalPlaying() ? "Stop" : "Play");
    playBtn.setToggleState (proc.isInternalPlaying(), juce::dontSendNotification);
}

juce::File ArpGenEditor::makeTempMidi (int layer)
{
    auto pat = proc.getPattern (layer);
    if (! pat || pat->notes.empty()) return {};
    const juce::String name = juce::String (layer == 0 ? "Melody" : "Arp") + "_"
                              + juce::String::charToString ((juce::juce_wchar) ('A' + proc.getActiveVariation (layer)));
    auto f = ag::MidiExporter::tempFile ("ArpGen_" + name + ".mid");
    return ag::MidiExporter::write (f, *pat, proc.getLastBpm()) ? f : juce::File();
}

void ArpGenEditor::saveMidiDialog()
{
    const int layer = proc.getCurrentLayer();
    auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile (layer == 0 ? "ArpGen_Melody.mid" : "ArpGen_Arp.mid");
    chooser = std::make_unique<juce::FileChooser> ("Save MIDI file", start, "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, layer] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f == juce::File()) return;
                              if (auto pat = proc.getPattern (layer))
                                  ag::MidiExporter::write (f.withFileExtension ("mid"), *pat, proc.getLastBpm());
                          });
}

void ArpGenEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kAccent);
    g.setFont (juce::Font (juce::FontOptions (24.f, juce::Font::bold)));
    g.drawText ("ARPGEN", 16, 12, 150, 34, juce::Justification::centredLeft);

    g.setColour (kDim);
    g.setFont (12.f);
    g.drawText ("Click a note in the piano roll to lock it, then use Regenerate.", 632, 314, 356, 32,
                juce::Justification::centredRight);

    auto drawPanel = [&] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (r.toFloat(), 10.f);
        g.setColour (kAccent.withAlpha (0.8f));
        g.setFont (12.f);
        g.drawText (title, r.getX() + 14, r.getY() + 8, r.getWidth() - 28, 20, juce::Justification::centredLeft);
    };
    drawPanel (panel1, "HARMONY");
    drawPanel (panel2, "FEEL");
    drawPanel (panel3, "SOUND & SYNC");
}

void ArpGenEditor::resized()
{
    const int W = getWidth();
    melodyBtn.setBounds (170, 12, 90, 34);
    arpBtn.setBounds (264, 12, 90, 34);
    for (int v = 0; v < 3; ++v) varBtn[v].setBounds (380 + v * 38, 12, 34, 34);
    genBtn.setBounds (520, 12, 110, 34);
    regenBtn.setBounds (634, 12, 170, 34);
    playBtn.setBounds (W - 12 - 90, 12, 90, 34);

    roll.setBounds (12, 56, W - 24, 250);
    dragMelody.setBounds (12, 314, 200, 32);
    dragArp.setBounds (218, 314, 200, 32);
    saveBtn.setBounds (424, 314, 200, 32);

    panel1 = { 12, 354, 320, 314 };
    panel2 = { 340, 354, 330, 314 };
    panel3 = { 678, 354, 310, 314 };

    auto placeCombo = [] (Combo& c, int x, int y, int w)
    {
        c.l.setBounds (x, y, 100, 28);
        c.c.setBounds (x + 100, y, w - 100, 28);
    };
    auto placeKnob = [] (Knob& k, int x, int y)
    {
        k.l.setBounds (x, y, 100, 16);
        k.s.setBounds (x, y + 16, 100, 84);
    };

    int y = panel1.getY() + 36;
    for (auto* c : { &root, &scale, &prog, &bars, &reg, &arp })
    {
        placeCombo (*c, panel1.getX() + 14, y, 292);
        y += 42;
    }

    Knob* feel[] = { &complexity, &density, &variation, &syncopation, &swing, &gate };
    for (int i = 0; i < 6; ++i)
        placeKnob (*feel[i], panel2.getX() + 10 + (i % 3) * 104, panel2.getY() + 36 + (i / 3) * 104);
    humanize.b.setBounds (panel2.getX() + 12, 602, 150, 24);
    avoid.b.setBounds (panel2.getX() + 166, 602, 160, 24);

    placeCombo (sound, panel3.getX() + 14, panel3.getY() + 36, 282);
    placeKnob (volume, panel3.getX() + 8, 428);
    placeKnob (cutoff, panel3.getX() + 108, 428);
    placeKnob (attack, panel3.getX() + 208, 428);
    placeKnob (release, panel3.getX() + 8, 528);
    placeKnob (bpm, panel3.getX() + 108, 528);
    follow.b.setBounds (panel3.getX() + 14, 634, 282, 24);
}
