#pragma once
#include "PluginProcessor.h"

class NeonForgeEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit NeonForgeEditor (NeonForgeProcessor&);
    ~NeonForgeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshTargetList();
    void rebuildKnobs();
    void enterIdleMode();
    void enterClonedMode();

    NeonForgeProcessor& audioProcessor;

    juce::TextButton scanButton  { "Scan Audio Units" };
    juce::ComboBox   targetBox;
    juce::TextButton learnButton { "AI ASSIST: CAPTURE TARGET" };
    juce::Label      statusLabel { "status", "Scan, pick a target, then hit Capture." };
    juce::Label      titleLabel  { {}, "NEONFORGE" };

    juce::OwnedArray<juce::Slider> dynamicKnobs;
    juce::OwnedArray<juce::Label>  knobLabels;
    juce::TextButton saveButton  { "Save Profile" };
    juce::TextButton loadButton  { "Load Profile" };

    enum class AppState { Idle, Profiling, Cloned };
    AppState currentState = AppState::Idle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonForgeEditor)
};
