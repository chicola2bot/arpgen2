#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace ag
{
// Raw parameter pointers (owned by the AudioProcessorValueTreeState); read lock-free on the audio thread.
struct SynthParams
{
    std::atomic<float>* sound = nullptr;   // choice index 0..5
    std::atomic<float>* cutoff = nullptr;  // Hz
    std::atomic<float>* attack = nullptr;  // s
    std::atomic<float>* release = nullptr; // s
};

class ArpSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class ArpVoice : public juce::SynthesiserVoice
{
public:
    explicit ArpVoice (const SynthParams& p) : params (p) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<ArpSound*> (s) != nullptr; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote (float, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    struct Patch
    {
        int wave1, wave2;     // 0 sine, 1 saw, 2 triangle
        float ratio2, detuneCents, mix2;
        float decay, sustain, filterEnv, octave, minAttack, gain;
    };

    SynthParams params;
    Patch cur {};
    juce::ADSR adsr;
    float phase1 = 0.f, phase2 = 0.f, inc1 = 0.f, inc2 = 0.f, level = 0.f, z1 = 0.f, coeff = 0.f;
};
}
