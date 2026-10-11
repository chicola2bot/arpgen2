#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "Engine/Types.h"

namespace ag::MidiExporter
{
// Writes a standard MIDI file (type 1, 960 PPQ, one track) with editable note events.
bool write (const juce::File& file, const Pattern& pattern, double bpm);

// File inside <temp>/ArpGen/ (used for drag and drop).
juce::File tempFile (const juce::String& name);
}
