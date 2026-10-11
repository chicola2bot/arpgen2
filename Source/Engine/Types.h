#pragma once
#include <memory>
#include <vector>

namespace ag
{
constexpr int kPPQ = 960; // ticks per quarter note (engine + exported MIDI)

struct Note
{
    int startTick = 0;
    int lengthTick = 240;
    int pitch = 60;
    int velocity = 90;
    bool locked = false;
};

struct Pattern
{
    int lengthTicks = kPPQ * 4;
    std::vector<Note> notes; // sorted by startTick
};

using PatternPtr = std::shared_ptr<const Pattern>;
} // namespace ag
