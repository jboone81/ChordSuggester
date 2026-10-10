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
    previewedChord.reset();
    suggestions = chords::suggest (currentChord, keyRoot);
    repaint();
}

juce::Rectangle<int> ChordSuggesterAudioProcessorEditor::currentChordBounds() const
{
    return { 20, 50, 280, 156 };
}

juce::Rectangle<int> ChordSuggesterAudioProcessorEditor::transitionBounds() const
{
    return { 24, 240, 272, 68 };
}

void ChordSuggesterAudioProcessorEditor::updateCursor (juce::Point<int> position)
{
    const bool clickable = rowAt (position) >= 0 || currentChordBounds().contains (position)
        || (previewedChord.has_value() && transitionBounds().contains (position));
    setMouseCursor (clickable ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
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
    g.drawText (currentChord.name, 20, 70, 280, 100, juce::Justification::centred);

    g.setColour (juce::Colour (0xffa7afbc));
    g.setFont (juce::FontOptions (16.0f));
    g.drawText (currentChord.fullName, 20, 176, 280, 24, juce::Justification::centred);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Click to replay", 20, 204, 280, 20, juce::Justification::centred);

    const auto transition = transitionBounds();
    g.setColour (juce::Colour (previewedChord ? 0xff253348 : 0xff20252d));
    g.fillRoundedRectangle (transition.toFloat(), 9.0f);
    g.setColour (juce::Colour (0xffa7afbc));
    g.drawText ("Preview next", transition.withHeight (22), juce::Justification::centred);
    g.setColour (previewedChord ? juce::Colours::white : juce::Colour (0xffa7afbc));
    g.setFont (juce::FontOptions (24.0f));
    const auto label = juce::String (currentChord.name) + juce::String::fromUTF8 (" \xe2\x86\x92 ")
        + (previewedChord ? juce::String (previewedChord->name) : juce::String::fromUTF8 ("\xe2\x80\x94"));
    g.drawText (label, transition.withTrimmedTop (22).withTrimmedBottom (6), juce::Justification::centred);
    g.setFont (juce::FontOptions (13.0f));
    g.setColour (juce::Colour (0xffa7afbc));
    g.drawText (previewedChord ? "Click above to hear the transition" : "Preview a recommendation first",
                20, 314, 280, 22, juce::Justification::centred);

    g.drawText (juce::String::fromUTF8 ("Click to preview \xc2\xb7 Double-click to select"),
                320, 326, getWidth() - 340, 22, juce::Justification::centred);

    // Right: chords to try next. Click to preview; double-click to select.
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
    if (! event.mods.isLeftButtonDown() || event.getNumberOfClicks() != 1)
        return;
    const int row = rowAt (event.getPosition());

    if (row >= 0)
    {
        previewedChord = suggestions[(size_t) row].chord;
        audioProcessor.previewChord (*previewedChord);
        repaint();
    }
    else if (currentChordBounds().contains (event.getPosition()))
        audioProcessor.previewChord (currentChord);
    else if (previewedChord && transitionBounds().contains (event.getPosition()))
        audioProcessor.previewTransition (currentChord, *previewedChord);
    updateCursor (event.getPosition());
}

void ChordSuggesterAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (! event.mods.isLeftButtonDown() || event.getNumberOfClicks() != 2)
        return;
    const int row = rowAt (event.getPosition());
    if (row >= 0)
        setCurrentChord (suggestions[(size_t) row].chord);
    updateCursor (event.getPosition());
}

void ChordSuggesterAudioProcessorEditor::mouseMove (const juce::MouseEvent& event)
{
    const int row = rowAt (event.getPosition());
    updateCursor (event.getPosition());

    if (row != hoveredRow)
    {
        hoveredRow = row;
        repaint();
    }
}

void ChordSuggesterAudioProcessorEditor::mouseExit (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
    if (hoveredRow != -1)
    {
        hoveredRow = -1;
        repaint();
    }
}
