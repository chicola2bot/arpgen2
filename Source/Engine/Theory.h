#pragma once
#include <vector>

namespace ag
{
struct ScaleDef { const char* name; std::vector<int> iv; };
struct ProgressionDef { const char* name; std::vector<int> degrees; }; // 0-based scale degrees

inline const std::vector<ScaleDef>& scales()
{
    static const std::vector<ScaleDef> s = {
        { "Major (Ionian)",          { 0, 2, 4, 5, 7, 9, 11 } },
        { "Natural Minor (Aeolian)", { 0, 2, 3, 5, 7, 8, 10 } },
        { "Dorian",                  { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",                { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",                  { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",              { 0, 2, 4, 5, 7, 9, 10 } },
        { "Locrian",                 { 0, 1, 3, 5, 6, 8, 10 } },
        { "Harmonic Minor",          { 0, 2, 3, 5, 7, 8, 11 } },
        { "Melodic Minor",           { 0, 2, 3, 5, 7, 9, 11 } },
        { "Minor Pentatonic",        { 0, 3, 5, 7, 10 } },
        { "Major Pentatonic",        { 0, 2, 4, 7, 9 } } };
    return s;
}

// Progressions are scale degrees, so they are always diatonic to the chosen scale/mode.
inline const std::vector<ProgressionDef>& progressions()
{
    static const std::vector<ProgressionDef> p = {
        { "1 - 5 - 6 - 4", { 0, 4, 5, 3 } },
        { "6 - 4 - 1 - 5", { 5, 3, 0, 4 } },
        { "1 - 6 - 3 - 7", { 0, 5, 2, 6 } },
        { "1 - 4 - 5 - 1", { 0, 3, 4, 0 } },
        { "2 - 5 - 1 - 1", { 1, 4, 0, 0 } },
        { "1 - 6 - 4 - 5", { 0, 5, 3, 4 } },
        { "1 - 7 - 6 - 5", { 0, 6, 5, 4 } } };
    return p;
}
} // namespace ag
