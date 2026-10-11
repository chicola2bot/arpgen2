#pragma once
#include "Types.h"
#include <cstdint>

namespace ag
{
struct GenParams
{
    int root = 9;          // pitch class 0..11
    int scaleIdx = 1;
    int progIdx = 2;
    int bars = 4;          // 1, 2, 4, 8
    int centerMidi = 72;   // register centre
    float complexity = 0.5f, density = 0.5f, variation = 0.5f;
    float syncopation = 0.3f, swing = 0.f, gate = 0.85f;
    bool humanize = false, avoidRepeats = true;
    bool arpMode = false;  // false = melody, true = arpeggio
    int arpPattern = 0;
    uint32_t seed = 1;
};

class Composer
{
public:
    // lockedSource: notes flagged `locked` there are kept; new notes overlapping them are dropped.
    static PatternPtr generate (const GenParams&, const Pattern* lockedSource);
};
} // namespace ag
