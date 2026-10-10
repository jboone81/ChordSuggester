/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ChordEngine.h"

//==============================================================================
ChordSuggesterAudioProcessorEditor::ChordSuggesterAudioProcessorEditor (ChordSuggesterAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (880, 360);
}

ChordSuggesterAudioProcessorEditor::~ChordSuggesterAudioProcessorEditor()
{
}

//==============================================================================
void ChordSuggesterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14171c));

    // The chord you have. Fixed to C major in the key of C for now.
    const chords::Chord current { "C", "C major", { { 0, 4, 7 } } };
    const auto suggestions = chords::suggest (current, 0);   // 0 = key of C major

    // Left: the chord you have
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (48.0f));
    g.drawText (current.name, 0, 0, 300, getHeight(), juce::Justification::centred);

    // Right: chords to try next. Blue = safe, gray = dramatic.
    const juce::Colour safeBlue (0xff2563eb), dramaticGray (0xff4a4e55);
    const int rowHeight = 44, gap = 6;
    int y = 20;

    for (const auto& s : suggestions)
    {
        g.setColour (dramaticGray.interpolatedWith (safeBlue, s.safety));
        g.fillRoundedRectangle (320.0f, (float) y, (float) getWidth() - 340.0f, (float) rowHeight, 9.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (22.0f));
        g.drawText (s.chord.name, 338, y, 200, rowHeight, juce::Justification::centredLeft);
        y += rowHeight + gap;
    }
}

void ChordSuggesterAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}
