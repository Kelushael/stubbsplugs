#include "PluginEditor.h"

NeonForgeEditor::NeonForgeEditor (NeonForgeProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (480, 360);

    titleLabel.setFont (juce::Font (24.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00ffa0));
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    scanButton.onClick = [this]
    {
        audioProcessor.scanForAUs();
        refreshTargetList();
    };
    addAndMakeVisible (scanButton);

    targetBox.setTextWhenNothingSelected ("Target plugin to emulate");
    targetBox.onChange = [this]
    {
        const int idx = targetBox.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (idx, audioProcessor.getKnownPlugins().getNumTypes()))
        {
            auto err = audioProcessor.chooseTarget (audioProcessor.getKnownPlugins().getTypes()[idx]);
            if (err.isNotEmpty())
                statusLabel.setText (err, juce::dontSendNotification);
        }
    };
    addAndMakeVisible (targetBox);

    learnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1a1a1a));
    learnButton.setColour (juce::TextButton::textColourOffId, juce::Colours::cyan);
    learnButton.onClick = [this]
    {
        audioProcessor.startLearn();
        currentState = AppState::Profiling;
        learnButton.setEnabled (false);
        learnButton.setButtonText ("EXTRACTING DSP...");
    };
    addAndMakeVisible (learnButton);

    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusLabel);

    saveButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Save NeonForge Profile",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("MyClone.neon"),
            "*.neon");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                if (fc.getResult() == juce::File{}) return;
                audioProcessor.saveProfileFile (fc.getResult());
            });
    };
    addChildComponent (saveButton);

    loadButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Load NeonForge Profile",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
            "*.neon");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser& fc)
            {
                if (fc.getResult() == juce::File{}) return;
                audioProcessor.loadProfileFile (fc.getResult());
                enterClonedMode();
            });
    };
    addChildComponent (loadButton);

    refreshTargetList();
    startTimerHz (10);
}

NeonForgeEditor::~NeonForgeEditor() {}

void NeonForgeEditor::timerCallback()
{
    auto procState = audioProcessor.state.load();
    statusLabel.setText (audioProcessor.statusText, juce::dontSendNotification);

    if (currentState == AppState::Profiling && procState == NeonForgeProcessor::State::Cloned)
    {
        enterClonedMode();
    }
}

void NeonForgeEditor::refreshTargetList()
{
    targetBox.clear (juce::dontSendNotification);
    const auto& types = audioProcessor.getKnownPlugins().getTypes();
    for (int i = 0; i < types.size(); ++i)
        targetBox.addItem (types[i].name, i + 1);
}

void NeonForgeEditor::enterClonedMode()
{
    currentState = AppState::Cloned;

    scanButton.setVisible (false);
    targetBox.setVisible (false);
    learnButton.setVisible (false);
    statusLabel.setVisible (false);

    saveButton.setVisible (true);
    loadButton.setVisible (true);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);

    rebuildKnobs();
    setSize (juce::jmax (480, 100 + dynamicKnobs.size() * 90), 320);
    resized();
    repaint();
}

void NeonForgeEditor::rebuildKnobs()
{
    dynamicKnobs.clear();
    knobLabels.clear();

    auto params = audioProcessor.getDerivedParams();
    for (auto& dp : params)
    {
        auto* slider = dynamicKnobs.add (new juce::Slider());
        slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
        slider->setRange (dp.minVal, dp.maxVal, 0.001);
        slider->setValue (*dp.value, juce::dontSendNotification);
        slider->setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffffaa00));
        float* ptr = dp.value;
        slider->onValueChange = [ptr, slider]() { *ptr = (float) slider->getValue(); };
        addAndMakeVisible (slider);

        auto* label = knobLabels.add (new juce::Label());
        label->setText (dp.name, juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centred);
        label->setColour (juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible (label);
    }
}

void NeonForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d0d));

    if (currentState == AppState::Cloned)
    {
        g.setColour (juce::Colour (0xff00ffa0));
        g.setFont (18.0f);
        g.drawFittedText ("CLONED PROFILE", getLocalBounds().withHeight (32).withY (8),
                          juce::Justification::centred, 1);
    }
}

void NeonForgeEditor::resized()
{
    auto bounds = getLocalBounds().reduced (16);

    if (currentState != AppState::Cloned)
    {
        titleLabel.setBounds (bounds.removeFromTop (36));
        bounds.removeFromTop (8);
        scanButton.setBounds (bounds.removeFromTop (32));
        bounds.removeFromTop (6);
        targetBox.setBounds (bounds.removeFromTop (32));
        bounds.removeFromTop (12);

        auto row = bounds.removeFromTop (48);
        learnButton.setBounds (row.withSizeKeepingCentre (260, 44));
        bounds.removeFromTop (8);
        statusLabel.setBounds (bounds.removeFromTop (28));
    }
    else
    {
        auto top = bounds.removeFromTop (28);
        saveButton.setBounds (top.removeFromLeft (110));
        top.removeFromLeft (8);
        loadButton.setBounds (top.removeFromLeft (110));
        bounds.removeFromTop (12);

        int knobW = 80;
        int spacing = 16;
        int x = bounds.getX();
        int y = bounds.getY();
        int rowH = 0;

        for (int i = 0; i < dynamicKnobs.size(); ++i)
        {
            if (x + knobW > bounds.getRight())
            {
                x = bounds.getX();
                y += rowH + spacing;
                rowH = 0;
            }
            dynamicKnobs[i]->setBounds (x, y, knobW, knobW);
            knobLabels[i]->setBounds (x, y + knobW, knobW, 20);
            x += knobW + spacing;
            rowH = juce::jmax (rowH, knobW + 20);
        }
    }
}
