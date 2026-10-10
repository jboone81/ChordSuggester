/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChordSuggesterAudioProcessorEditor::ChordSuggesterAudioProcessorEditor (ChordSuggesterAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Work out the first set of suggestions for the starting chord.
    setCurrentChord (currentChord);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (880, 360);
}

ChordSuggesterAudioProcessorEditor::~ChordSuggesterAudioProcessorEditor()
{
}

//==============================================================================
void ChordSuggesterAudioProcessorEditor::setCurrentChord (const chords::Chord& newChord)
{
    // Copy first: newChord may be one of the suggestions we are about to replace.
    const chords::Chord chosen = newChord;

    currentChord = chosen;
    suggestions = chords::suggest (currentChord, keyRoot);
    repaint();
}

juce::Rectangle<int> ChordSuggesterAudioProcessorEditor::rowBounds (int index) const
{
    const int left = 320, top = 20, rowHeight = 44, gap = 6;
    return { left, top + index * (rowHeight + gap), getWidth() - left - 20, rowHeight };
}

int ChordSuggesterAudioProcessorEditor::rowAt (juce::Point<int> position) const
{
    for (int i = 0; i < (int) suggestions.size(); ++i)
        if (rowBounds (i).contains (position))
            return i;

    return -1;
}

//==============================================================================
void ChordSuggesterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14171c));

    // Left: the chord you have
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (48.0f));
    g.drawText (currentChord.name, 0, 0, 300, getHeight() - 30, juce::Justification::centred);

    g.setColour (juce::Colour (0xffa7afbc));
    g.setFont (juce::FontOptions (16.0f));
    g.drawText (currentChord.fullName, 0, getHeight() / 2 + 20, 300, 24, juce::Justification::centred);

    // Right: chords to try next. Blue = safe, gray = dramatic. Click one to move to it.
    const juce::Colour safeBlue (0xff2563eb), dramaticGray (0xff4a4e55);

    for (int i = 0; i < (int) suggestions.size(); ++i)
    {
        const auto& s = suggestions[(size_t) i];
        const auto row = rowBounds (i);

        auto colour = dramaticGray.interpolatedWith (safeBlue, s.safety);
        if (i == hoveredRow)
            colour = colour.brighter (0.25f);

        g.setColour (colour);
        g.fillRoundedRectangle (row.toFloat(), 9.0f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (22.0f));
        g.drawText (s.chord.name, row.withTrimmedLeft (18), juce::Justification::centredLeft);
    }
}

void ChordSuggesterAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}

//==============================================================================
void ChordSuggesterAudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    const int row = rowAt (event.getPosition());

    if (row >= 0)
        setCurrentChord (suggestions[(size_t) row].chord);
}

void ChordSuggesterAudioProcessorEditor::mouseMove (const juce::MouseEvent& event)
{
    const int row = rowAt (event.getPosition());

    if (row != hoveredRow)
    {
        hoveredRow = row;
        setMouseCursor (row >= 0 ? juce::MouseCursor::PointingHandCursor
                                 : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ChordSuggesterAudioProcessorEditor::mouseExit (const juce::MouseEvent&)
{
    if (hoveredRow != -1)
    {
        hoveredRow = -1;
        repaint();
    }
}
