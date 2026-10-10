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

    addAndMakeVisible (chordsTab);
    addAndMakeVisible (melodyTab);
    for (auto* button : { &generateButton, &playButton, &backingButton, &loopButton, &resetButton })
    {
        addAndMakeVisible (*button);
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff20252d));
        button->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2563eb));
    }
    playButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2563eb));
    backingButton.setClickingTogglesState (true);
    loopButton.setClickingTogglesState (true);
    backingButton.onClick = [this] { stopMelody(); };
    loopButton.onClick = [this] { stopMelody(); };
    for (auto* box : { &scaleChoice, &barsChoice, &timingChoice }) addAndMakeVisible (*box);
    scaleChoice.setComponentID ("scale");
    barsChoice.setComponentID ("bars");
    timingChoice.setComponentID ("timing");
    addAndMakeVisible (progressionDisplay);
    addAndMakeVisible (pianoRoll);
    progressionDisplay.setMultiLine (false);
    progressionDisplay.setReadOnly (true);
    progressionDisplay.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff20252d));
    progressionDisplay.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    scaleChoice.addItem ("Estimated scale", 1);
    for (int mode = 0; mode < 2; ++mode)
        for (int root = 0; root < 12; ++root)
            scaleChoice.addItem (melody::Scale { root, mode == 1 }.name(), 2 + mode * 12 + root);
    scaleChoice.setSelectedId (1, juce::dontSendNotification);
    scaleChoice.onChange = [this] { invalidatePhrase(); repaint(); };
    for (int bars : { 1, 2, 4, 8 }) barsChoice.addItem (juce::String (bars), bars);
    barsChoice.setSelectedId (4, juce::dontSendNotification);
    barsChoice.onChange = [this] { invalidatePhrase(); repaint(); };
    timingChoice.addItem ("1/4 notes", 1);
    timingChoice.addItem ("1/8 notes", 2);
    timingChoice.addItem ("1/16 notes", 3);
    timingChoice.setSelectedId (2, juce::dontSendNotification);
    timingChoice.onChange = [this] { pianoRoll.setGrid (gridSize()); };
    chordsTab.onClick = [this] { switchTab (false); };
    melodyTab.onClick = [this] { switchTab (true); };
    generateButton.onClick = [this]
    {
        stopMelody();
        auto next = melody::generate (progression.all(), selectedScale(), ++generationSeed, barCount(), gridSize());
        const auto samePitches = [&]
        {
            if (next.notes.size() != phrase.notes.size()) return false;
            for (size_t i = 0; i < next.notes.size(); ++i)
                if (next.notes[i].midi != phrase.notes[i].midi) return false;
            return true;
        };
        for (int attempt = 0; attempt < 16 && samePitches(); ++attempt)
            next = melody::generate (progression.all(), selectedScale(), ++generationSeed, barCount(), gridSize());
        if (! next.notes.empty() && samePitches())
            next.notes.front().midi += next.notes.front().midi <= 72 ? 12 : -12;
        phrase = std::move (next);
        pianoRoll.refresh();
        playButton.setEnabled (! phrase.notes.empty());
        repaint();
    };
    playButton.onClick = [this]
    {
        if (melodyPlaying) { stopMelody(); return; }
        audioProcessor.previewPhrase (phrase, backingButton.getToggleState(), loopButton.getToggleState());
        melodyPlaying = true;
        seenPlayhead = false;
        playButton.setButtonText ("Stop");
    };
    pianoRoll.onEdit = [this]
    {
        stopMelody();
        playButton.setEnabled (! phrase.notes.empty());
    };
    pianoRoll.onAudition = [this] (int midi) { stopMelody(); audioProcessor.previewNote (midi); };
    resetButton.onClick = [this] { progression.reset (currentChord); refreshMelody(); };
    refreshMelody();
    switchTab (false);
    startTimerHz (30);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setResizable (true, false);
    setResizeLimits (960, 600, 1600, 1000);
    setSize (1160, 760);
}

ChordSuggesterAudioProcessorEditor::~ChordSuggesterAudioProcessorEditor()
{
    audioProcessor.stopPreview();
}

int ChordSuggesterAudioProcessorEditor::barCount() const { return std::max (1, barsChoice.getSelectedId()); }
double ChordSuggesterAudioProcessorEditor::gridSize() const
{
    return timingChoice.getSelectedId() == 3 ? 0.25 : timingChoice.getSelectedId() == 2 ? 0.5 : 1.0;
}

void ChordSuggesterAudioProcessorEditor::stopMelody()
{
    audioProcessor.stopPreview();
    melodyPlaying = seenPlayhead = false;
    pianoRoll.setPlayhead (-1);
    playButton.setButtonText ("Play");
}

void ChordSuggesterAudioProcessorEditor::timerCallback()
{
    const double beat = audioProcessor.melodyPlayhead();
    if (melodyPlaying)
    {
        if (beat >= 0) seenPlayhead = true;
        else if (seenPlayhead) { melodyPlaying = false; playButton.setButtonText ("Play"); }
    }
    pianoRoll.setPlayhead (melodyPlaying ? beat : -1);
}

void ChordSuggesterAudioProcessorEditor::switchTab (bool melodyView)
{
    showingMelody = melodyView;
    stopMelody();
    hoveredRow = -1;
    chordsTab.setToggleState (! showingMelody, juce::dontSendNotification);
    melodyTab.setToggleState (showingMelody, juce::dontSendNotification);
    for (auto* button : { &generateButton, &playButton, &backingButton, &loopButton, &resetButton }) button->setVisible (showingMelody);
    for (auto* box : { &scaleChoice, &barsChoice, &timingChoice }) box->setVisible (showingMelody);
    progressionDisplay.setVisible (showingMelody);
    pianoRoll.setVisible (showingMelody);
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void ChordSuggesterAudioProcessorEditor::invalidatePhrase (bool stopPlayback)
{
    if (stopPlayback) stopMelody();
    phrase.notes.clear();
    phrase.chords = melody::chordsForBars (progression.all(), barCount());
    phrase.lengthBeats = barCount() * 4.0;
    pianoRoll.setScale (selectedScale());
    pianoRoll.setGrid (gridSize());
    pianoRoll.refresh();
    playButton.setEnabled (false);
}

void ChordSuggesterAudioProcessorEditor::refreshMelody (bool stopPlayback)
{
    scaleEstimate = melody::estimate (progression.all());
    const bool usingEstimate = scaleChoice.getSelectedId() <= 1;
    scaleChoice.changeItemText (1, juce::String (scaleEstimate.best.name()));
    if (usingEstimate) scaleChoice.setSelectedId (1, juce::dontSendNotification);
    invalidatePhrase (stopPlayback);
    juce::String text;
    for (const auto& chord : progression.all())
    {
        if (text.isNotEmpty()) text += " -> ";
        text += juce::String (chord.name);
    }
    progressionDisplay.setText (text, false);
    repaint();
}

melody::Scale ChordSuggesterAudioProcessorEditor::selectedScale() const
{
    const int id = scaleChoice.getSelectedId();
    return id <= 1 ? scaleEstimate.best : melody::Scale { (id - 2) % 12, id >= 14 };
}

void ChordSuggesterAudioProcessorEditor::paintMelody (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xffa7afbc));
    g.setFont (juce::FontOptions (17.0f));
    g.drawText ("Suggested melody", 24, 89, 175, 30, juce::Justification::centredLeft);
    g.drawText ("Bars", getWidth() - 375, 89, 45, 30, juce::Justification::centredLeft);
    g.drawText ("Timing", getWidth() - 220, 89, 65, 30, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Estimated: " + juce::String (scaleEstimate.best.name())
                + (scaleEstimate.ambiguous ? " | Also possible: " + juce::String (scaleEstimate.alternative.name()) : ""),
                24, 126, getWidth() - 48, 20, juce::Justification::centredLeft);
    juce::String notes = "Scale notes: ";
    for (int note : selectedScale().notes()) notes += juce::String (chords::noteName (note)) + "  ";
    g.drawText (notes, 24, 182, getWidth() - 48, 20, juce::Justification::centredLeft);
    bool outside = false;
    for (const auto& chord : progression.all())
        for (int note : chord.notes) outside = outside || ! selectedScale().contains (note);
    g.setColour (outside ? juce::Colour (0xffffca80) : juce::Colour (0xffa7afbc));
    g.drawText (outside ? "Some chord notes fall outside this scale. Generated notes stay in scale; edits may use any row."
                        : "Shaded rows are outside the scale. Drag notes to move them; drag their right edge to resize.",
                24, getHeight() - 40, getWidth() - 48, 20, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffa7afbc));
    g.drawFittedText ("Double-click to add | Delete to remove | Overlaps rejected | 120 BPM, 4/4 | Shift+wheel scrolls horizontally",
                      410, getHeight() - 84, getWidth() - 435, 38, juce::Justification::centredLeft, 2);
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
    position.y -= 72;
    if (showingMelody) return;
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
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("ChordSuggester", 24, 18, 235, 36, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff343a44));
    g.drawHorizontalLine (71, 0.0f, float (getWidth()));
    g.setColour (juce::Colour (0xff2563eb));
    const auto activeTab = showingMelody ? melodyTab.getBounds() : chordsTab.getBounds();
    g.fillRect (activeTab.getX(), 68, activeTab.getWidth(), 4);
    if (showingMelody)
    {
        g.setColour (juce::Colour (0xffa7afbc));
        g.setFont (juce::FontOptions (16.0f));
        g.drawText (scaleChoice.getSelectedId() == 1 ? "Estimated key" : "Scale override",
                    getWidth() - 415, 22, 115, 32, juce::Justification::centredLeft);
        paintMelody (g);
        return;
    }
    g.addTransform (juce::AffineTransform::translation (0.0f, 72.0f));

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
    chordsTab.setBounds (280, 16, 100, 40);
    melodyTab.setBounds (400, 16, 110, 40);
    scaleChoice.setBounds (getWidth() - 285, 22, 261, 32);
    generateButton.setBounds (200, 86, 180, 36);
    barsChoice.setBounds (getWidth() - 325, 86, 85, 36);
    timingChoice.setBounds (getWidth() - 150, 86, 126, 36);
    progressionDisplay.setBounds (24, 150, getWidth() - 250, 26);
    resetButton.setBounds (getWidth() - 205, 150, 181, 26);
    pianoRoll.setBounds (24, 210, getWidth() - 48, getHeight() - 312);
    playButton.setBounds (24, getHeight() - 84, 120, 38);
    backingButton.setBounds (154, getHeight() - 84, 130, 38);
    loopButton.setBounds (294, getHeight() - 84, 100, 38);
}

//==============================================================================
void ChordSuggesterAudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    if (showingMelody || ! event.mods.isLeftButtonDown() || event.getNumberOfClicks() != 1)
        return;
    const auto position = event.getPosition() - juce::Point<int> (0, 72);
    const int row = rowAt (position);

    if (row >= 0)
    {
        previewedChord = suggestions[(size_t) row].chord;
        audioProcessor.previewChord (*previewedChord);
        repaint();
    }
    else if (currentChordBounds().contains (position))
        audioProcessor.previewChord (currentChord);
    else if (previewedChord && transitionBounds().contains (position))
        audioProcessor.previewTransition (currentChord, *previewedChord);
    updateCursor (event.getPosition());
}

void ChordSuggesterAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (showingMelody || ! event.mods.isLeftButtonDown() || event.getNumberOfClicks() != 2)
        return;
    const int row = rowAt (event.getPosition() - juce::Point<int> (0, 72));
    if (row >= 0)
    {
        setCurrentChord (suggestions[(size_t) row].chord);
        progression.confirm (currentChord);
        // Keep the first click's chord preview sounding during confirmation.
        refreshMelody (false);
    }
    updateCursor (event.getPosition());
}

void ChordSuggesterAudioProcessorEditor::mouseMove (const juce::MouseEvent& event)
{
    const int row = showingMelody ? -1 : rowAt (event.getPosition() - juce::Point<int> (0, 72));
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
