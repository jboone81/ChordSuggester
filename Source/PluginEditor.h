/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ChordEngine.h"
#include <optional>
#include "PianoRoll.h"

class NavigationButton : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;
    void paintButton (juce::Graphics& g, bool, bool) override
    {
        g.setColour (getToggleState() ? juce::Colours::white : juce::Colour (0xffa7afbc));
        g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
    }
};

//==============================================================================
/**
*/
class ChordSuggesterAudioProcessorEditor  : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    ChordSuggesterAudioProcessorEditor (ChordSuggesterAudioProcessor&);
    ~ChordSuggesterAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    // Makes newChord the chord you have, then works out fresh suggestions for it.
    void setCurrentChord (const chords::Chord& newChord);

    // Where suggestion number `index` is drawn, which is also where it can be clicked.
    juce::Rectangle<int> rowBounds (int index) const;

    // Which suggestion is under this point, or -1 if none.
    int rowAt (juce::Point<int> position) const;
    juce::Rectangle<int> currentChordBounds() const;
    juce::Rectangle<int> transitionBounds() const;
    void updateCursor (juce::Point<int> position);
    void switchTab (bool melodyTab);
    void refreshMelody (bool stopPlayback = true);
    void invalidatePhrase (bool stopPlayback = true);
    melody::Scale selectedScale() const;
    void paintMelody (juce::Graphics&);
    void timerCallback() override;
    void stopMelody();
    int barCount() const;
    double gridSize() const;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    ChordSuggesterAudioProcessor& audioProcessor;

    int keyRoot = 0;   // 0 = key of C major
    chords::Chord currentChord { "C", "C major", { { 0, 4, 7 } } };
    std::vector<chords::Suggestion> suggestions;
    std::optional<chords::Chord> previewedChord;
    int hoveredRow = -1;
    melody::Progression progression { currentChord };
    melody::Estimate scaleEstimate;
    melody::Phrase phrase;
    PianoRoll pianoRoll { phrase };
    uint32_t generationSeed = std::random_device {}();
    bool showingMelody = false;
    NavigationButton chordsTab { "Chords" }, melodyTab { "Melody" };
    juce::TextButton generateButton { "New suggestion" }, playButton { "Play" };
    juce::TextButton backingButton { "With chords" }, loopButton { "Loop" };
    juce::TextButton resetButton { "Reset progression" };
    juce::ComboBox scaleChoice, barsChoice, timingChoice;
    juce::TextEditor progressionDisplay;
    bool melodyPlaying = false, seenPlayhead = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordSuggesterAudioProcessorEditor)
};
