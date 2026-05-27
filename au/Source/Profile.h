#pragma once
#include <juce_core/juce_core.h>

// JSON profile produced by the Python profiler (or hand-written).
// Loaded by the processor to configure its parametric DSP.
struct Profile
{
    struct EQBand {
        float freq = 1000.0f;
        float gain = 0.0f;
        float q    = 1.0f;
        int   type = 0; // 0=peak, 1=lowShelf, 2=highShelf, 3=lowPass, 4=highPass
    };
    struct Compressor {
        float threshold_db = 0.0f;
        float ratio        = 1.0f;
        float knee_db      = 0.0f;
        float attack_ms    = 5.0f;
        float release_ms   = 50.0f;
        float makeup_db    = 0.0f;
        bool  enabled      = false;
    };
    struct Saturator {
        float drive_db = 0.0f;
        float mix      = 1.0f;
        bool  enabled  = false;
    };

    juce::String name { "Unnamed Clone" };
    juce::Array<EQBand> eqBands;
    Compressor comp;
    Saturator sat;
    bool hasConvolution = false;
    juce::MemoryBlock irData;     // optional raw IR for linear tier
    double irSampleRate = 48000.0;

    static Profile fromJSON (const juce::var& json);
    juce::var toJSON() const;
};
