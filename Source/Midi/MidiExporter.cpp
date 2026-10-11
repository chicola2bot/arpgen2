#include "MidiExporter.h"
#include <algorithm>

namespace ag::MidiExporter
{
bool write (const juce::File& file, const Pattern& pattern, double bpm)
{
    juce::MidiMessageSequence seq;
    const int micros = (int) (60000000.0 / std::clamp (bpm, 20.0, 400.0));

    seq.addEvent (juce::MidiMessage::tempoMetaEvent (micros));
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4));

    for (const auto& n : pattern.notes)
    {
        auto on = juce::MidiMessage::noteOn (1, n.pitch, (juce::uint8) std::clamp (n.velocity, 1, 127));
        on.setTimeStamp ((double) n.startTick);
        auto off = juce::MidiMessage::noteOff (1, n.pitch);
        off.setTimeStamp ((double) (n.startTick + n.lengthTick));
        seq.addEvent (on);
        seq.addEvent (off);
    }

    auto eot = juce::MidiMessage::endOfTrack();
    eot.setTimeStamp ((double) pattern.lengthTicks);
    seq.addEvent (eot);
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (kPPQ);
    mf.addTrack (seq);

    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::FileOutputStream os (file);
    if (! os.openedOk()) return false;
    return mf.writeTo (os, 1);
}

juce::File tempFile (const juce::String& name)
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ArpGen").getChildFile (name);
}
}
