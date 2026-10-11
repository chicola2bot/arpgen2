#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/Theory.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout l;

    auto choice = [&] (const char* id, const char* name, const StringArray& items, int def)
    { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto flt = [&] (const char* id, const char* name, NormalisableRange<float> r, float def)
    { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def)); };
    auto boo = [&] (const char* id, const char* name, bool def)
    { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    StringArray sc, pr;
    for (auto& s : ag::scales()) sc.add (s.name);
    for (auto& p : ag::progressions()) pr.add (p.name);

    choice ("root", "Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9);
    choice ("scale", "Scale / Mode", sc, 1);
    choice ("prog", "Progression", pr, 2);
    choice ("bars", "Bars", { "1", "2", "4", "8" }, 2);
    choice ("register", "Register (octave)", { "2", "3", "4", "5", "6" }, 3);
    choice ("arp", "Arp pattern", { "Up", "Down", "Up-Down", "Outside-In", "Skips (3rds)", "Random Walk" }, 0);

    const NormalisableRange<float> unit (0.f, 1.f);
    flt ("complexity", "Complexity", unit, 0.5f);
    flt ("density", "Density", unit, 0.5f);
    flt ("variation", "Variation", unit, 0.5f);
    flt ("syncopation", "Syncopation", unit, 0.3f);
    flt ("swing", "Swing", unit, 0.f);
    flt ("gate", "Duration", NormalisableRange<float> (0.2f, 1.f), 0.85f);
    boo ("humanize", "Humanize", false);
    boo ("avoidRepeats", "Avoid repeats", true);

    choice ("sound", "Sound", { "Piano", "Pluck", "Bass", "Lead", "Keys", "Pad" }, 1);
    flt ("volume", "Volume", unit, 0.7f);
    flt ("cutoff", "Filter", NormalisableRange<float> (40.f, 18000.f, 0.f, 0.3f), 9000.f);
    flt ("attack", "Attack", NormalisableRange<float> (0.001f, 2.f, 0.f, 0.4f), 0.003f);
    flt ("release", "Release", NormalisableRange<float> (0.01f, 3.f, 0.f, 0.4f), 0.25f);
    boo ("followHost", "Follow host", true);
    flt ("manualBpm", "Tempo (manual)", NormalisableRange<float> (40.f, 240.f, 1.f), 120.f);
    return l;
}

juce::String patternToBase64 (const ag::Pattern& p)
{
    std::vector<int32_t> d;
    d.push_back (p.lengthTicks);
    for (auto& n : p.notes)
    {
        d.push_back (n.startTick); d.push_back (n.lengthTick); d.push_back (n.pitch);
        d.push_back (n.velocity);  d.push_back (n.locked ? 1 : 0);
    }
    return juce::Base64::toBase64 (d.data(), d.size() * sizeof (int32_t));
}

ag::PatternPtr patternFromBase64 (const juce::String& s)
{
    juce::MemoryOutputStream mo;
    if (! juce::Base64::convertFromBase64 (mo, s)) return nullptr;
    const auto size = mo.getDataSize();
    if (size < 4 || size % 4 != 0) return nullptr;
    std::vector<int32_t> d (size / 4);
    std::memcpy (d.data(), mo.getData(), size);

    auto p = std::make_shared<ag::Pattern>();
    p->lengthTicks = std::clamp<int> (d[0], ag::kPPQ, ag::kPPQ * 4 * 8);
    for (size_t i = 1; i + 5 <= d.size(); i += 5)
    {
        ag::Note n;
        n.startTick = std::clamp<int> (d[i], 0, p->lengthTicks - 1);
        n.lengthTick = std::clamp<int> (d[i + 1], 1, ag::kPPQ * 4 * 8);
        n.pitch = std::clamp<int> (d[i + 2], 0, 127);
        n.velocity = std::clamp<int> (d[i + 3], 1, 127);
        n.locked = d[i + 4] != 0;
        p->notes.push_back (n);
    }
    return p;
}
} // namespace

ArpGenProcessor::ArpGenProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ARPGEN", createLayout())
{
    pVolume = apvts.getRawParameterValue ("volume");
    pManualBpm = apvts.getRawParameterValue ("manualBpm");
    pFollowHost = apvts.getRawParameterValue ("followHost");
    synthParams.sound = apvts.getRawParameterValue ("sound");
    synthParams.cutoff = apvts.getRawParameterValue ("cutoff");
    synthParams.attack = apvts.getRawParameterValue ("attack");
    synthParams.release = apvts.getRawParameterValue ("release");

    synth.addSound (new ag::ArpSound());
    for (int i = 0; i < 8; ++i) synth.addVoice (new ag::ArpVoice (synthParams));

    generate (0, false);
    generate (1, false);
}

bool ArpGenProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ArpGenProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    lastGain = pVolume->load();
    expectedPpq = 0.0;
    wasPlaying = false;
}

// ------------------------------------------------------------------------------------------------
ag::GenParams ArpGenProcessor::makeGenParams (int layer) const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    static const int barOptions[] = { 1, 2, 4, 8 };

    ag::GenParams p;
    p.root = (int) get ("root");
    p.scaleIdx = (int) get ("scale");
    p.progIdx = (int) get ("prog");
    p.bars = barOptions[std::clamp ((int) get ("bars"), 0, 3)];
    p.centerMidi = 12 * ((int) get ("register") + 3);
    p.complexity = get ("complexity");
    p.density = get ("density");
    p.variation = get ("variation");
    p.syncopation = get ("syncopation");
    p.swing = get ("swing");
    p.gate = get ("gate");
    p.humanize = get ("humanize") > 0.5f;
    p.avoidRepeats = get ("avoidRepeats") > 0.5f;
    p.arpMode = layer == 1;
    p.arpPattern = (int) get ("arp");
    return p;
}

void ArpGenProcessor::generate (int layer, bool keepLocked)
{
    layer = std::clamp (layer, 0, 1);
    auto gp = makeGenParams (layer);
    juce::Random rnd;
    auto& L = layers[(size_t) layer];

    for (size_t v = 0; v < 3; ++v)
    {
        gp.seed = (uint32_t) rnd.nextInt();
        const ag::Pattern* locked = (keepLocked && L.vars[v]) ? L.vars[v].get() : nullptr;
        L.vars[v] = ag::Composer::generate (gp, locked);
    }
    if (layer == currentLayer.load()) publish();
}

void ArpGenProcessor::selectVariation (int layer, int variation)
{
    layers[(size_t) std::clamp (layer, 0, 1)].active = std::clamp (variation, 0, 2);
    if (layer == currentLayer.load()) publish();
}

void ArpGenProcessor::toggleLock (int layer, int idx)
{
    auto& L = layers[(size_t) std::clamp (layer, 0, 1)];
    auto cur = L.vars[(size_t) L.active];
    if (! cur || idx < 0 || idx >= (int) cur->notes.size()) return;
    auto copy = std::make_shared<ag::Pattern> (*cur);
    copy->notes[(size_t) idx].locked = ! copy->notes[(size_t) idx].locked;
    L.vars[(size_t) L.active] = copy; // lock flags do not affect playback, so nothing is republished
}

ag::PatternPtr ArpGenProcessor::getPattern (int layer) const
{
    const auto& L = layers[(size_t) std::clamp (layer, 0, 1)];
    return L.vars[(size_t) L.active];
}

int ArpGenProcessor::getActiveVariation (int layer) const
{
    return layers[(size_t) std::clamp (layer, 0, 1)].active;
}

void ArpGenProcessor::setCurrentLayer (int layer)
{
    currentLayer.store (std::clamp (layer, 0, 1));
    publish();
}

void ArpGenProcessor::publish()
{
    auto p = getPattern (currentLayer.load());
    keepAlive.push_back (p);
    while (keepAlive.size() > 64) keepAlive.pop_front();
    {
        juce::SpinLock::ScopedLockType sl (patternLock);
        published = p;
        patternVersion.fetch_add (1);
    }
}

void ArpGenProcessor::setInternalPlaying (bool shouldPlay)
{
    if (shouldPlay) restartInternal.store (true);
    internalPlaying.store (shouldPlay);
}

// ------------------------------------------------------------------------------------------------
void ArpGenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    // pick up a newly published pattern (try-lock: never blocks)
    if (audioVersion != patternVersion.load())
    {
        juce::SpinLock::ScopedTryLockType tl (patternLock);
        if (tl.isLocked())
        {
            audioPattern = published;
            audioVersion = patternVersion.load();
            synth.allNotesOff (0, true);
        }
    }

    // tempo / transport
    double bpm = (double) pManualBpm->load();
    bool hostPlaying = false, hostHasPpq = false;
    double hostPpq = 0.0;
    if (pFollowHost->load() > 0.5f)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                if (auto b = pos->getBpm()) bpm = *b;
                hostPlaying = pos->getIsPlaying();
                if (auto pp = pos->getPpqPosition()) { hostPpq = *pp; hostHasPpq = true; }
            }
    bpm = std::clamp (bpm, 20.0, 400.0);
    lastBpm.store (bpm);

    const bool useHost = hostPlaying && hostHasPpq;
    const bool useInternal = ! useHost && internalPlaying.load();
    const bool playing = useHost || useInternal;
    if (restartInternal.exchange (false)) internalPpq = 0.0;

    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    const double ppqPerSample = bpm / 60.0 / sr;
    const double p0 = useHost ? hostPpq : internalPpq;
    const double p1 = p0 + n * ppqPerSample;

    if (playing)
    {
        if (! wasPlaying || std::abs (p0 - expectedPpq) > n * ppqPerSample * 2.0 + 0.01)
            synth.allNotesOff (0, true); // transport started or jumped

        if (audioPattern && ! audioPattern->notes.empty())
        {
            const double loopLen = audioPattern->lengthTicks / (double) ag::kPPQ;
            const int k0 = (int) std::floor (p0 / loopLen);
            const int k1 = (int) std::floor (p1 / loopLen);
            auto sampleOf = [&] (double ppq) { return std::clamp ((int) ((ppq - p0) / ppqPerSample), 0, n - 1); };

            for (int k = k0; k <= k1; ++k)
            {
                const double loopStart = k * loopLen;
                for (const auto& note : audioPattern->notes)
                {
                    const double onP = loopStart + note.startTick / (double) ag::kPPQ;
                    const int lenTicks = std::min (note.lengthTick, audioPattern->lengthTicks - note.startTick);
                    const double offP = std::min (onP + lenTicks / (double) ag::kPPQ, loopStart + loopLen - 0.002);

                    if (onP >= p0 && onP < p1)
                        midi.addEvent (juce::MidiMessage::noteOn (1, note.pitch, (juce::uint8) note.velocity), sampleOf (onP));
                    if (offP >= p0 && offP < p1)
                        midi.addEvent (juce::MidiMessage::noteOff (1, note.pitch), sampleOf (offP));
                }
            }
            uiPpq.store (std::fmod (p1, loopLen));
        }
        expectedPpq = p1;
        if (useInternal) internalPpq = p1;
    }
    else
    {
        if (wasPlaying) synth.allNotesOff (0, true);
        uiPpq.store (-1.0);
    }
    wasPlaying = playing;

    synth.renderNextBlock (buffer, midi, 0, n);

    const float g = pVolume->load();
    buffer.applyGainRamp (0, n, lastGain, g);
    lastGain = g;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i) d[i] = std::tanh (d[i]); // soft clip, avoids harsh digital clipping
    }
}

// ------------------------------------------------------------------------------------------------
void ArpGenProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto old = state.getChildWithName ("Layers"); old.isValid()) state.removeChild (old, nullptr);

    juce::ValueTree lt ("Layers");
    lt.setProperty ("current", currentLayer.load(), nullptr);
    for (size_t l = 0; l < 2; ++l)
    {
        juce::ValueTree layer ("Layer");
        layer.setProperty ("active", layers[l].active, nullptr);
        for (auto& v : layers[l].vars)
        {
            juce::ValueTree vt ("Var");
            if (v) vt.setProperty ("data", patternToBase64 (*v), nullptr);
            layer.appendChild (vt, nullptr);
        }
        lt.appendChild (layer, nullptr);
    }
    state.appendChild (lt, nullptr);

    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ArpGenProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid() || tree.getType() != apvts.state.getType()) return;

    auto lt = tree.getChildWithName ("Layers");
    for (int l = 0; l < 2 && lt.isValid() && l < lt.getNumChildren(); ++l)
    {
        auto layer = lt.getChild (l);
        layers[(size_t) l].active = std::clamp ((int) layer.getProperty ("active", 0), 0, 2);
        for (int v = 0; v < 3 && v < layer.getNumChildren(); ++v)
            layers[(size_t) l].vars[(size_t) v] = patternFromBase64 (layer.getChild (v).getProperty ("data").toString());
    }
    const int cur = lt.isValid() ? std::clamp ((int) lt.getProperty ("current", 0), 0, 1) : 0;

    if (lt.isValid()) tree.removeChild (lt, nullptr);
    apvts.replaceState (tree);

    for (int l = 0; l < 2; ++l)
        for (auto& v : layers[(size_t) l].vars)
            if (! v) { generate (l, false); break; }

    setCurrentLayer (cur);
}

juce::AudioProcessorEditor* ArpGenProcessor::createEditor() { return new ArpGenEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ArpGenProcessor(); }
