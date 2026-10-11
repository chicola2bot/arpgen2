#include "SynthEngine.h"
#include <algorithm>
#include <cmath>

namespace ag
{
namespace
{
inline float polyBlep (float t, float dt)
{
    if (t < dt)         { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt)   { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

inline float osc (int wave, float t, float dt)
{
    switch (wave)
    {
        case 0:  return std::sin (juce::MathConstants<float>::twoPi * t);
        case 1:  return 2.f * t - 1.f - polyBlep (t, dt);
        default: return 4.f * std::abs (t - 0.5f) - 1.f;
    }
}

//                    w1 w2 ratio2 det   mix   decay sust  fenv  oct   minA   gain
const struct { int w1, w2; float r2, det, mix, dec, sus, fenv, oct, minA, gain; } kPatches[] = {
    { 2, 1, 1.f,  4.f, 0.30f, 1.60f, 0.05f, 0.60f, 1.0f, 0.001f, 0.80f }, // Piano (tri + saw, closing filter)
    { 1, 1, 1.f,  7.f, 0.50f, 0.28f, 0.00f, 0.80f, 1.0f, 0.001f, 0.70f }, // Pluck
    { 0, 1, 1.f,  0.f, 0.45f, 0.35f, 0.65f, 0.30f, 0.5f, 0.001f, 0.90f }, // Bass (octave down)
    { 1, 1, 1.f, 12.f, 0.60f, 0.20f, 0.80f, 0.20f, 1.0f, 0.001f, 0.60f }, // Lead
    { 2, 0, 2.f,  0.f, 0.35f, 0.70f, 0.25f, 0.35f, 1.0f, 0.001f, 0.80f }, // Keys
    { 1, 1, 1.f, 10.f, 0.70f, 0.60f, 0.85f, 0.00f, 1.0f, 0.350f, 0.50f }, // Pad
};
}

void ArpVoice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int)
{
    const auto& s = kPatches[std::clamp ((int) params.sound->load(), 0, 5)];
    cur = { s.w1, s.w2, s.r2, s.det, s.mix, s.dec, s.sus, s.fenv, s.oct, s.minA, s.gain };

    const double sr = getSampleRate();
    const double f = juce::MidiMessage::getMidiNoteInHertz (midiNote) * cur.octave;
    inc1 = (float) (f / sr);
    inc2 = (float) (f * cur.ratio2 * std::pow (2.0, cur.detuneCents / 1200.0) / sr);
    level = velocity * cur.gain;
    phase1 = phase2 = z1 = 0.f;

    adsr.setSampleRate (sr);
    adsr.setParameters (juce::ADSR::Parameters (std::max (params.attack->load(), cur.minAttack),
                                                cur.decay, cur.sustain, params.release->load()));
    adsr.noteOn();
}

void ArpVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
    }
    else
    {
        clearCurrentNote();
        adsr.reset();
    }
}

void ArpVoice::renderNextBlock (juce::AudioBuffer<float>& out, int start, int num)
{
    if (! isVoiceActive()) return;

    const float cutoff = params.cutoff->load();
    const float sr = (float) getSampleRate();
    const int chans = out.getNumChannels();

    for (int i = 0; i < num; ++i)
    {
        const float env = adsr.getNextSample();
        if (! adsr.isActive())
        {
            clearCurrentNote();
            break;
        }

        if ((i & 7) == 0)
        {
            const float fc = std::clamp (cutoff * (1.f - cur.filterEnv + cur.filterEnv * env), 40.f, 0.45f * sr);
            coeff = 1.f - std::exp (-juce::MathConstants<float>::twoPi * fc / sr);
        }

        const float o1 = osc (cur.wave1, phase1, inc1);
        const float o2 = osc (cur.wave2, phase2, inc2);
        const float s = (1.f - cur.mix2) * o1 + cur.mix2 * o2;

        phase1 += inc1; if (phase1 >= 1.f) phase1 -= 1.f;
        phase2 += inc2; if (phase2 >= 1.f) phase2 -= 1.f;

        z1 += coeff * (s - z1);
        const float y = z1 * env * level * 0.5f;
        for (int ch = 0; ch < chans; ++ch)
            out.addSample (ch, start + i, y);
    }
}
}
