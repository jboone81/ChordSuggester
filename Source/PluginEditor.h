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

//==============================================================================
/**
*/
class ChordSuggesterAudioProcessorEditor  : public juce::AudioProcessorEditor
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

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    ChordSuggesterAudioProcessor& audioProcessor;

    int keyRoot = 0;   // 0 = key of C major
    chords::Chord currentChord { "C", "C major", { { 0, 4, 7 } } };
    std::vector<chords::Suggestion> suggestions;
    std::optional<chords::Chord> previewedChord;
    int hoveredRow = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordSuggesterAudioProcessorEditor)
};
