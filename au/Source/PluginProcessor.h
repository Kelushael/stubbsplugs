#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Profile.h"

class NeonForgeProcessor : public juce::AudioProcessor
{
public:
    NeonForgeProcessor();
    ~NeonForgeProcessor() override = default;

    void prepareToPlay (double sr, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "NeonForge"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- Profile / State ----
    enum class State { Idle, Profiling, Cloned };
    std::atomic<State> state { State::Idle };
    std::atomic<bool>  busy  { false };
    juce::String statusText { "Scan, pick a target, then hit Capture." };

    Profile profile;
    juce::CriticalSection profileLock;

    // ---- Target hosting & capture ----
    void scanForAUs();
    juce::KnownPluginList& getKnownPlugins() { return knownList; }
    juce::String chooseTarget (const juce::PluginDescription&);
    void startLearn();                    // begins background profiling thread
    void loadProfileFile (const juce::File&);
    void saveProfileFile (const juce::File&) const;

    // ---- Parametric knob access (for editor) ----
    struct DerivedParam {
        juce::String name;
        float* value;
        float minVal, maxVal;
    };
    std::vector<DerivedParam> getDerivedParams() const;

private:
    void rebuildDSP();
    void processParametric (juce::AudioBuffer<float>&);

    // Target hosting
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownList;
    std::unique_ptr<juce::AudioPluginInstance> target;
    juce::CriticalSection targetLock;

    // Convolution (linear tier)
    juce::dsp::Convolution conv;
    bool useConv = false;

    // Parametric EQ
    static constexpr int maxBands = 8;
    std::array<std::unique_ptr<juce::dsp::IIR::Filter<float>>, maxBands> eqLeft, eqRight;

    // Dynamics
    juce::dsp::Compressor<float> compressor;
    bool useComp = false;

    // Saturator
    juce::dsp::WaveShaper<float> waveShaper;
    float satDrive = 0.0f, satMix = 1.0f;
    bool useSat = false;

    double sampleRate = 48000.0;
    int blockSize = 512;

    // ---- Capture helpers ----
    static juce::AudioBuffer<float> makeSweep (double sr, double f1, double f2, double dur);
    juce::AudioBuffer<float> renderThroughTarget (const juce::AudioBuffer<float>& dryMono);
    static juce::AudioBuffer<float> deconvolve (const juce::AudioBuffer<float>& x,
                                                const juce::AudioBuffer<float>& y, int irLen);
    static juce::var deriveParamsFromAudio (const juce::AudioBuffer<float>& ir,
                                            double sr);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonForgeProcessor)
};
