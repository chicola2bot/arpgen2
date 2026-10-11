#include "Composer.h"
#include "Theory.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace ag
{
namespace
{
constexpr int kStep = kPPQ / 4; // one 16th note
constexpr int kBar = kPPQ * 4;

struct Rng
{
    std::mt19937 g;
    explicit Rng (uint32_t s) : g (s) {}
    float f() { return std::uniform_real_distribution<float> (0.f, 1.f) (g); }
    int i (int a, int b) { return std::uniform_int_distribution<int> (a, b) (g); }
    bool chance (float p) { return f() < p; }
};

struct Scale
{
    const std::vector<int>* iv;
    int root;
    int size;

    int pitch (int deg) const
    {
        const int oct = (int) std::floor ((double) deg / size);
        const int idx = deg - oct * size;
        return root + (*iv)[(size_t) idx] + 12 * oct;
    }
    int nearestDegree (int midi) const
    {
        int best = 0, bd = 1 << 30;
        for (int d = -3 * size; d <= 12 * size; ++d)
        {
            const int dd = std::abs (pitch (d) - midi);
            if (dd < bd) { bd = dd; best = d; }
        }
        return best;
    }
};

std::vector<int> chordTones (int chordDeg, bool seventh)
{
    std::vector<int> t { chordDeg, chordDeg + 2, chordDeg + 4 };
    if (seventh) t.push_back (chordDeg + 6);
    return t;
}

bool isTone (const std::vector<int>& tones, int deg, int size)
{
    for (int t : tones)
        if ((((t - deg) % size) + size) % size == 0) return true;
    return false;
}

int nearestTone (const std::vector<int>& tones, int target, int size)
{
    int best = target, bd = 1 << 30;
    for (int t : tones)
        for (int o = -4; o <= 4; ++o)
        {
            const int c = t + o * size;
            const int dd = std::abs (c - target);
            if (dd < bd) { bd = dd; best = c; }
        }
    return best;
}

// ---- rhythm ---------------------------------------------------------------------------------
std::vector<int> makeSteps (Rng& r, float density, float complexity, float sync)
{
    const int target = std::clamp ((int) std::round (2.f + density * 9.f + (r.f() - 0.5f) * 2.f), 2, 13);
    float w[16];
    for (int s = 0; s < 16; ++s)
    {
        float base = (s % 4 == 0) ? 1.f : (s % 4 == 2) ? 0.6f : 0.28f;
        if (s % 4 == 0 && s != 0) base *= 1.f - 0.45f * sync;
        else if (s % 2 == 1)      base += sync * (0.35f + 0.4f * complexity);
        else if (s % 4 == 2)      base += sync * 0.25f;
        w[s] = base;
    }
    bool chosen[16] = {};
    int count = 0;
    if (r.chance (0.9f)) { chosen[0] = true; count = 1; }
    while (count < target)
    {
        float sum = 0.f;
        for (int s = 0; s < 16; ++s) if (! chosen[s]) sum += w[s];
        float x = r.f() * sum;
        int pick = -1;
        for (int s = 0; s < 16; ++s)
        {
            if (chosen[s]) continue;
            pick = s;
            x -= w[s];
            if (x <= 0.f) break;
        }
        chosen[pick] = true;
        ++count;
    }
    std::vector<int> steps;
    for (int s = 0; s < 16; ++s) if (chosen[s]) steps.push_back (s);
    return steps;
}

std::vector<int> mutateSteps (Rng& r, std::vector<int> s, float variation)
{
    if (s.size() > 1 && r.chance (variation * 0.7f))
    {
        const size_t k = (size_t) r.i (1, (int) s.size() - 1);
        const int ns = s[k] + (r.chance (0.5f) ? 1 : -1) * r.i (1, 2);
        if (ns >= 1 && ns <= 15 && std::find (s.begin(), s.end(), ns) == s.end()) s[k] = ns;
    }
    if (r.chance (variation * 0.4f))
    {
        if (s.size() > 3 && r.chance (0.5f))
            s.erase (s.begin() + r.i (1, (int) s.size() - 1));
        else
        {
            const int ns = r.i (1, 15);
            if (std::find (s.begin(), s.end(), ns) == s.end()) s.push_back (ns);
        }
    }
    std::sort (s.begin(), s.end());
    return s;
}

// cumulative contour (in scale degrees) with leap recovery and a gentle arch
std::vector<int> makeContour (Rng& r, size_t n, float complexity)
{
    std::vector<int> c (n, 0);
    int prev = 0;
    for (size_t i = 1; i < n; ++i)
    {
        const float leapP = 0.08f + 0.3f * complexity;
        const int mag = r.chance (leapP) ? r.i (3, 4) : (r.chance (0.65f) ? 1 : 2);
        bool up = r.chance (i < n / 2 + 1 ? 0.6f : 0.4f);
        if (std::abs (prev) >= 3) up = prev < 0;          // recover after a leap
        if (std::abs (c[i - 1]) > 5) up = c[i - 1] < 0;   // pull back to centre
        const int d = up ? mag : -mag;
        prev = d;
        c[i] = c[i - 1] + d;
    }
    return c;
}

int velFor (int step, Rng& r)
{
    int v = 78;
    if (step % 4 == 0) v += 14; else if (step % 2 == 0) v += 6;
    return std::clamp (v + r.i (-5, 5), 30, 120);
}

Note makeNote (int bar, int step, int lenSteps, int pitch, int vel, float gate)
{
    Note n;
    n.startTick = bar * kBar + step * kStep;
    n.lengthTick = std::max (60, (int) std::lround (lenSteps * kStep * gate));
    n.pitch = std::clamp (pitch, 0, 127);
    n.velocity = vel;
    return n;
}

// ---- melody ---------------------------------------------------------------------------------
void genMelody (const GenParams& p, const Scale& sc, const std::vector<int>& prog, Rng& r, std::vector<Note>& out)
{
    const int size = sc.size;
    const bool seventh = p.complexity > 0.65f;
    const auto motifSteps = makeSteps (r, p.density, p.complexity, p.syncopation);
    const auto contour = makeContour (r, motifSteps.size() + 4, p.complexity);
    int prevDeg = sc.nearestDegree (p.centerMidi);
    int prevPitch = -1;

    for (int b = 0; b < p.bars; ++b)
    {
        const int chordDeg = prog[(size_t) b % prog.size()];
        const auto tones = chordTones (chordDeg, seventh);
        const int pp = b % 4;
        const bool cadence = p.bars > 1 && (pp == 3 || b == p.bars - 1);

        std::vector<int> steps;
        if (cadence)        steps = p.density > 0.5f ? std::vector<int> { 0, 3, 6, 8 } : std::vector<int> { 0, 4, 8 };
        else if (pp == 1)   steps = mutateSteps (r, motifSteps, p.variation);
        else if (pp == 2)   steps = p.variation > 0.5f ? mutateSteps (r, motifSteps, p.variation * 0.5f) : motifSteps;
        else                steps = motifSteps;

        std::vector<int> degs (steps.size());
        if (cadence)
        {
            // tension -> resolution: stepwise approach to the chord root, long final note
            const int finalDeg = nearestTone ({ chordDeg }, prevDeg, size);
            const int sign = prevDeg > finalDeg ? 1 : -1;
            const int n = (int) steps.size();
            for (int k = 0; k < n; ++k) degs[(size_t) k] = finalDeg + sign * (n - 1 - k);
        }
        else
        {
            const int anchor = nearestTone (tones, prevDeg, size);
            for (size_t i = 0; i < steps.size(); ++i)
            {
                int d = anchor + contour[std::min (i, contour.size() - 1)];
                if (pp == 1 && r.chance (p.variation * 0.35f)) d += r.chance (0.5f) ? 1 : -1;
                const bool strong = steps[i] % 4 == 0;
                if (steps[i] == 0 || (strong && ! isTone (tones, d, size) && r.chance (0.8f)))
                    d = nearestTone (tones, d, size); // chord tones on strong beats
                degs[i] = d;
            }
        }

        for (size_t i = 0; i < steps.size(); ++i)
        {
            const int step = steps[i];
            const bool isFinal = cadence && i + 1 == steps.size();
            if (! cadence && i > 0 && r.chance ((1.f - p.density) * 0.25f)) continue; // rest

            int d = degs[i];
            int pitch = sc.pitch (d);
            if (p.avoidRepeats && ! cadence && pitch == prevPitch)
            {
                d += r.chance (0.5f) ? 1 : -1;
                pitch = sc.pitch (d);
            }
            while (pitch > p.centerMidi + 14) { d -= size; pitch = sc.pitch (d); }
            while (pitch < p.centerMidi - 14) { d += size; pitch = sc.pitch (d); }

            const int next = (i + 1 < steps.size()) ? steps[i + 1] : 16;
            const int lenSteps = isFinal ? 16 - step : std::max (1, next - step);
            out.push_back (makeNote (b, step, lenSteps, pitch, velFor (step, r), isFinal ? 0.97f : p.gate));
            prevDeg = d;
            prevPitch = pitch;
        }
    }

    // controlled chromaticism: a weak-beat note may approach the next strong-beat note by a semitone
    if (p.complexity > 0.6f)
        for (size_t i = 0; i + 1 < out.size(); ++i)
        {
            auto& a = out[i];
            const auto& nb = out[i + 1];
            if (a.startTick % kPPQ != 0 && nb.startTick % kPPQ == 0 && nb.startTick - a.startTick <= kPPQ
                && r.chance ((p.complexity - 0.6f) * 0.6f))
            {
                const int np = nb.pitch + (r.chance (0.5f) ? -1 : 1);
                if (np != a.pitch) a.pitch = np;
            }
        }
}

// ---- arpeggio -------------------------------------------------------------------------------
std::vector<int> makeArpSeq (int pat, int N, Rng& r)
{
    std::vector<int> s;
    switch (pat)
    {
        case 0: for (int i = 0; i < N; ++i) s.push_back (i); break;                       // up
        case 1: for (int i = N - 1; i >= 0; --i) s.push_back (i); break;                  // down
        case 2:                                                                           // up-down
            for (int i = 0; i < N; ++i) s.push_back (i);
            for (int i = N - 2; i >= 1; --i) s.push_back (i);
            break;
        case 3: for (int lo = 0, hi = N - 1; lo <= hi;)                                   // outside-in
                { s.push_back (lo++); if (lo <= hi) s.push_back (hi--); } break;
        case 4: for (int i = 0; i < N; ++i) { s.push_back (i); s.push_back ((i + 2) % N); } break; // skips
        default: { int w = N / 2;                                                         // random walk
                   for (int k = 0; k < 16; ++k)
                   { s.push_back (w); w = std::clamp (w + r.i (1, 3) * (r.chance (0.5f) ? 1 : -1), 0, N - 1); } }
    }
    if (s.empty()) s.push_back (0);
    return s;
}

std::vector<int> arpSteps (float density, float sync)
{
    if (sync > 0.5f && density < 0.75f) return { 0, 3, 6, 8, 11, 14 }; // 3+3+2 feel
    const int stride = density < 0.3f ? 4 : density < 0.65f ? 2 : 1;
    std::vector<int> s;
    for (int i = 0; i < 16; i += stride) s.push_back (i);
    return s;
}

void genArp (const GenParams& p, const Scale& sc, const std::vector<int>& prog, Rng& r, std::vector<Note>& out)
{
    const int size = sc.size;
    const int octs = 1 + (p.complexity > 0.35f ? 1 : 0) + (p.complexity > 0.75f ? 1 : 0);
    const bool seventh = p.complexity > 0.5f;
    const int anchorDeg = sc.nearestDegree (p.centerMidi - 12);
    const auto steps = arpSteps (p.density, p.syncopation);
    int seqPos = 0;

    for (int b = 0; b < p.bars; ++b)
    {
        const int chordDeg = prog[(size_t) b % prog.size()];
        const int base = nearestTone ({ chordDeg }, anchorDeg, size);
        std::vector<int> pool;
        for (int o = 0; o < octs; ++o)
            for (int off : { 0, 2, 4, 6 })
            {
                if (off == 6 && ! seventh) continue;
                pool.push_back (base + off + o * size);
            }
        const auto seq = makeArpSeq (p.arpPattern, (int) pool.size(), r);

        for (size_t i = 0; i < steps.size(); ++i)
        {
            const int step = steps[i];
            if (i > 0 && r.chance (0.3f * p.complexity * (step % 4 == 0 ? 0.3f : 1.f))) continue; // rest

            int d = pool[(size_t) seq[(size_t) seqPos % seq.size()] % pool.size()];
            ++seqPos;
            if (r.chance (p.variation * 0.12f) && sc.pitch (d) + 12 <= p.centerMidi + 16) d += size; // octave jump

            const int next = (i + 1 < steps.size()) ? steps[i + 1] : 16;
            int vel = 80 + r.i (-5, 5);
            if (step % 4 == 0)      vel += (int) std::lround (10 * (1.f - p.syncopation));
            else if (step % 4 == 2) vel += (int) std::lround (8 * p.syncopation);
            out.push_back (makeNote (b, step, std::max (1, next - step), sc.pitch (d), std::clamp (vel, 30, 120), p.gate));
        }
    }
}
} // namespace

PatternPtr Composer::generate (const GenParams& p, const Pattern* locked)
{
    const auto& sd = scales()[(size_t) std::clamp (p.scaleIdx, 0, (int) scales().size() - 1)];
    const Scale sc { &sd.iv, ((p.root % 12) + 12) % 12, (int) sd.iv.size() };
    const auto& prog = progressions()[(size_t) std::clamp (p.progIdx, 0, (int) progressions().size() - 1)].degrees;
    Rng r (p.seed);

    std::vector<Note> notes;
    if (p.arpMode) genArp (p, sc, prog, r, notes);
    else           genMelody (p, sc, prog, r, notes);

    const int lengthTicks = p.bars * kBar;
    for (auto& n : notes)
    {
        if (((n.startTick % kPPQ) / kStep) % 2 == 1)
            n.startTick += (int) std::lround (p.swing * 110.f);
        if (p.humanize)
        {
            n.startTick = std::max (0, n.startTick + r.i (-12, 12));
            n.velocity = std::clamp (n.velocity + r.i (-8, 8), 1, 127);
        }
        n.startTick = std::clamp (n.startTick, 0, lengthTicks - 1);
    }

    auto out = std::make_shared<Pattern>();
    out->lengthTicks = lengthTicks;
    std::vector<Note> keep;
    if (locked)
        for (const auto& n : locked->notes)
            if (n.locked && n.startTick < lengthTicks) keep.push_back (n);

    out->notes = keep;
    for (const auto& n : notes)
    {
        bool clash = false;
        for (const auto& l : keep)
            if (n.startTick < l.startTick + l.lengthTick && l.startTick < n.startTick + n.lengthTick) { clash = true; break; }
        if (! clash) out->notes.push_back (n);
    }
    std::stable_sort (out->notes.begin(), out->notes.end(),
                      [] (const Note& a, const Note& b) { return a.startTick < b.startTick; });
    return out;
}
} // namespace ag
