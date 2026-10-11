#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <deque>
#include "Engine/Composer.h"
#include "Audio/SynthEngine.h"

class ArpGenProcessor : public juce::AudioProcessor
{
public:
    ArpGenProcessor();
    ~ArpGenProcessor() override = default;

    // ---- juce::AudioProcessor ----
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "ArpGen"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- message-thread API (layer 0 = melody, 1 = arpeggio) ----
    void generate (int layer, bool keepLocked);
    void selectVariation (int layer, int variation);
    void toggleLock (int layer, int noteIndex);
    ag::PatternPtr getPattern (int layer) const;
    int getActiveVariation (int layer) const;
    void setCurrentLayer (int layer);
    int getCurrentLayer() const { return currentLayer.load(); }

    // ---- thread-safe ----
    void setInternalPlaying (bool shouldPlay);
    bool isInternalPlaying() const { return internalPlaying.load(); }
    double getLastBpm() const { return lastBpm.load(); }
    double getPlayheadPpq() const { return uiPpq.load(); } // -1 when stopped

private:
    struct LayerState
    {
        std::array<ag::PatternPtr, 3> vars;
        int active = 0;
    };

    ag::GenParams makeGenParams (int layer) const;
    void publish();

    std::array<LayerState, 2> layers; // message thread only
    std::atomic<int> currentLayer { 0 };

    // pattern hand-off message thread -> audio thread (no allocation or blocking on the audio thread)
    juce::SpinLock patternLock;
    ag::PatternPtr published;
    std::atomic<uint32_t> patternVersion { 0 };
    std::deque<ag::PatternPtr> keepAlive; // message thread; guarantees the audio thread never frees a pattern
    ag::PatternPtr audioPattern;          // audio thread only
    uint32_t audioVersion = 0xffffffffu;  // audio thread only

    // audio
    juce::Synthesiser synth;
    ag::SynthParams synthParams;
    std::atomic<float> *pVolume = nullptr, *pManualBpm = nullptr, *pFollowHost = nullptr;
    float lastGain = 0.7f;

    // transport
    std::atomic<bool> internalPlaying { false }, restartInternal { false };
    std::atomic<double> lastBpm { 120.0 }, uiPpq { -1.0 };
    double internalPpq = 0.0, expectedPpq = 0.0;
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArpGenProcessor)
};
