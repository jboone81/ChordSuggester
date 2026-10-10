/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int headerHeight = 72;
constexpr int leftColumnWidth = 380;
constexpr int keyboardFirstNote = 60;
constexpr int keyboardLastNote = 71;

juce::Colour backgroundColour   { 0xff1b1e26 };
juce::Colour headerColour       { 0xff151820 };
juce::Colour panelColour        { 0xff20242d };
juce::Colour secondaryText     { 0xffa7afbc };
juce::Colour accentBlue        { 0xff2563eb };
juce::Colour dramaticGray      { 0xff4a4e55 };

juce::String noteNames (const std::array<int, 3>& notes, bool useFlats)
{
    juce::String result;

    for (size_t i = 0; i < notes.size(); ++i)
    {
        if (i != 0)
            result += juce::String::fromUTF8 (" \xc2\xb7 ");

        result += juce::String::fromUTF8 (chords::noteName (notes[i], useFlats).c_str());
    }

    return result;
}

const char* safetyDescription (float safety)
{
    if (safety >= 0.8f) return "Safest";
    if (safety >= 0.6f) return "Safe";
    if (safety >= 0.35f) return "Balanced";
    return "Dramatic";
}
}

ChordMidiKeyboard::ChordMidiKeyboard (juce::MidiKeyboardState& state)
    : MidiKeyboardComponent (state, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setAvailableRange (keyboardFirstNote, keyboardLastNote);
    setScrollButtonsVisible (false);
    setWantsKeyboardFocus (false);
    setVelocity (0.8f, false);
    setColour (whiteNoteColourId, juce::Colour (0xfff1f3f6));
    setColour (blackNoteColourId, juce::Colour (0xff101319));
    setColour (keySeparatorLineColourId, juce::Colour (0xff596171));
    setColour (mouseOverKeyOverlayColourId, juce::Colour (0x552563eb));
    setColour (keyDownOverlayColourId, juce::Colour (0xaa2563eb));
    setColour (textLabelColourId, juce::Colour (0xff1b1e26));
}

void ChordMidiKeyboard::showChordPlayback (const std::array<int, 3>& notes)
{
    startPlaybackVisual (notes, std::nullopt);
}

void ChordMidiKeyboard::showTransitionPlayback (const std::array<int, 3>& first,
                                                const std::array<int, 3>& second)
{
    startPlaybackVisual (first, second);
}

void ChordMidiKeyboard::startPlaybackVisual (const std::array<int, 3>& first,
                                            const std::optional<std::array<int, 3>>& second)
{
    playbackNotes = first;
    nextPlaybackNotes = second;
    playbackStartedAt = juce::Time::getMillisecondCounter();
    showingSecondChord = false;
    playbackTimer.start();
    repaint();
}

bool ChordMidiKeyboard::isPlaybackNote (int midiNoteNumber) const
{
    const auto& activeNotes = showingSecondChord && nextPlaybackNotes.has_value()
                            ? *nextPlaybackNotes : playbackNotes;
    const int pitchClass = midiNoteNumber % 12;
    return std::find (activeNotes.begin(), activeNotes.end(), pitchClass) != activeNotes.end();
}

void ChordMidiKeyboard::updatePlaybackVisual()
{
    const auto elapsed = juce::Time::getMillisecondCounter() - playbackStartedAt;

    if (nextPlaybackNotes.has_value() && elapsed >= 1000 && ! showingSecondChord)
    {
        showingSecondChord = true;
        repaint();
    }

    if (elapsed >= (nextPlaybackNotes.has_value() ? 2000u : 1000u))
    {
        playbackTimer.stop();
        nextPlaybackNotes.reset();
        showingSecondChord = false;
    }

    repaint();
}

juce::String ChordMidiKeyboard::getWhiteNoteText (int midiNoteNumber)
{
    if (! isPlaybackNote (midiNoteNumber))
        return {};

    return juce::String::fromUTF8 (chords::noteName (midiNoteNumber % 12).c_str());
}

void ChordMidiKeyboard::drawWhiteNote (int midiNoteNumber, juce::Graphics& g,
                                       juce::Rectangle<float> area, bool isDown, bool isOver,
                                       juce::Colour lineColour, juce::Colour textColour)
{
    const auto key = area.reduced (1.5f, 0.0f);
    const float cornerRadius = juce::jmin (7.0f, key.getWidth() * 0.18f);
    juce::Path keyShape;
    keyShape.addRoundedRectangle (key.getX(), key.getY(), key.getWidth(), key.getHeight(),
                                  cornerRadius, cornerRadius, false, false, true, true);

    g.setColour (juce::Colour (0x55000000));
    g.fillPath (keyShape, juce::AffineTransform::translation (0.0f, 1.0f));

    auto fill = juce::Colour (0xfff1f3f6);
    if (isDown || isPlaybackNote (midiNoteNumber))
        fill = fill.interpolatedWith (accentBlue, 0.34f);
    else if (isOver)
        fill = fill.interpolatedWith (accentBlue, 0.12f);

    g.setColour (fill);
    g.fillPath (keyShape);
    g.setColour (juce::Colour (0xffd4d9e2));
    g.strokePath (keyShape, juce::PathStrokeType (1.0f));

    if (isDown || isPlaybackNote (midiNoteNumber))
    {
        const auto label = juce::String::fromUTF8 (chords::noteName (midiNoteNumber % 12).c_str());
        g.setColour (textColour);
        g.setFont (juce::FontOptions (juce::jmin (15.0f, getKeyWidth() * 0.34f)).withStyle ("Bold"));
        g.drawText (label, area.withTrimmedBottom (7.0f), juce::Justification::centredBottom, false);
    }

    juce::ignoreUnused (lineColour);
}

void ChordMidiKeyboard::drawBlackNote (int midiNoteNumber, juce::Graphics& g,
                                       juce::Rectangle<float> area, bool isDown, bool isOver,
                                       juce::Colour noteFillColour)
{
    const auto key = area.reduced (1.0f, 0.0f).withTrimmedBottom (1.0f);
    const float cornerRadius = juce::jmin (5.0f, key.getWidth() * 0.2f);
    juce::Path keyShape;
    keyShape.addRoundedRectangle (key.getX(), key.getY(), key.getWidth(), key.getHeight(),
                                  cornerRadius, cornerRadius, false, false, true, true);

    auto fill = juce::Colour (0xff101319);
    if (isDown || isPlaybackNote (midiNoteNumber))
        fill = fill.interpolatedWith (accentBlue, 0.55f);
    else if (isOver)
        fill = fill.interpolatedWith (accentBlue, 0.24f);

    g.setColour (juce::Colour (0x66000000));
    g.fillPath (keyShape, juce::AffineTransform::translation (0.0f, 1.5f));
    g.setColour (fill);
    g.fillPath (keyShape);
    g.setColour (juce::Colour (0xff333946));
    g.strokePath (keyShape, juce::PathStrokeType (0.8f));

    juce::ignoreUnused (midiNoteNumber, noteFillColour);
}



void RoundedKeySelectorLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                                   bool isButtonDown, int buttonX, int buttonY,
                                                   int buttonW, int buttonH, juce::ComboBox& box)
{
    juce::ignoreUnused (buttonX, buttonY, buttonW, buttonH);

    const auto bounds = juce::Rectangle<float> (0.5f, 0.5f,
                                                 (float) width - 1.0f, (float) height - 1.0f);
    const float cornerRadius = (float) height * 0.42f;
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId)
                     .interpolatedWith (juce::Colour (0xff252a34), isButtonDown ? 0.8f : 0.0f));
    g.fillRoundedRectangle (bounds, cornerRadius);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, cornerRadius, 1.3f);

    const auto centreX = (float) width - (float) height * 0.62f;
    const auto centreY = (float) height * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath (centreX - 4.0f, centreY - 1.5f);
    arrow.lineTo (centreX, centreY + 2.5f);
    arrow.lineTo (centreX + 4.0f, centreY - 1.5f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (arrow, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
}

//==============================================================================
ChordSuggesterAudioProcessorEditor::ChordSuggesterAudioProcessorEditor (ChordSuggesterAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), midiKeyboard (p.getMidiKeyboardState())
{
    // Work out the first set of suggestions for the starting chord.
    setCurrentChord (currentChord);

    for (int root = 0; root < 12; ++root)
    {
        const bool useFlats = root == 1 || root == 3 || root == 5 || root == 8 || root == 10;
        const auto name = juce::String::fromUTF8 (chords::noteName (root, useFlats).c_str());
        keySelector.addItem (name + " major", root + 1);
    }

    keySelector.setSelectedId (keyRoot + 1, juce::dontSendNotification);
    keySelector.setJustificationType (juce::Justification::centred);
    keySelector.setLookAndFeel (&keySelectorLookAndFeel);
    keySelector.setColour (juce::ComboBox::backgroundColourId, headerColour);
    keySelector.setColour (juce::ComboBox::textColourId, juce::Colour (0xfff1f3f6));
    keySelector.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff394150));
    keySelector.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xfff1f3f6));
    keySelector.onChange = [this] { updateKeySelection(); };
    addAndMakeVisible (keySelector);

    moreChords.setComponentID ("moreChords");
    moreChords.setTextWhenNothingSelected (juce::String::fromUTF8 ("More chords\xe2\x80\xa6"));
    moreChords.setColour (juce::ComboBox::backgroundColourId, headerColour);
    moreChords.setColour (juce::ComboBox::textColourId, juce::Colour (0xfff1f3f6));
    moreChords.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff394150));
    moreChords.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xfff1f3f6));
    moreChords.onChange = [this] { chooseMoreChord(); };
    addAndMakeVisible (moreChords);
    updateMoreChords();

    addAndMakeVisible (midiKeyboard);

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
    backingButton.onClick = [this] { audioProcessor.setMelodyBackingEnabled (backingButton.getToggleState()); };
    loopButton.onClick = [this] { stopMelody(); };
    for (auto* box : { &scaleChoice, &barsChoice, &timingChoice }) addAndMakeVisible (*box);
    scaleChoice.setComponentID ("scale");
    barsChoice.setComponentID ("bars");
    timingChoice.setComponentID ("timing");
    addAndMakeVisible (progressionDisplay);
    addAndMakeVisible (pianoRoll);

    // Dragging this handle writes the melody (plus the chords, if "With chords" is on)
    // to a MIDI file and hands it to the DAW.
    addAndMakeVisible (melodyDragHandle);
    melodyDragHandle.createFile = [this]
    {
        return midi_writer::writePhraseToTempFile (phrase, backingButton.getToggleState());
    };
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
    for (int bars : { 1, 2, 3, 4 }) barsChoice.addItem (juce::String (bars), bars);
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
    for (auto* button : { &undoChordButton, &resetChordsButton })
    {
        addAndMakeVisible (*button);
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff20252d));
    }
    undoChordButton.onClick = [this] { undoLastChord(); };
    resetChordsButton.onClick = [this] { progression.reset (currentChord); refreshMelody(); };
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
    keySelector.setLookAndFeel (nullptr);
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
    melodyDragHandle.setEnabled (! phrase.notes.empty());   // nothing to drag until there is a melody
}

void ChordSuggesterAudioProcessorEditor::switchTab (bool melodyView)
{
    showingMelody = melodyView;
    keySelector.setVisible (! showingMelody);
    moreChords.setVisible (! showingMelody);
    midiKeyboard.setVisible (! showingMelody);
    undoChordButton.setVisible (! showingMelody);
    resetChordsButton.setVisible (! showingMelody);
    stopMelody();
    hoveredRow = -1;
    chordsTab.setToggleState (! showingMelody, juce::dontSendNotification);
    melodyTab.setToggleState (showingMelody, juce::dontSendNotification);
    for (auto* button : { &generateButton, &playButton, &backingButton, &loopButton, &resetButton }) button->setVisible (showingMelody);
    for (auto* box : { &scaleChoice, &barsChoice, &timingChoice }) box->setVisible (showingMelody);
    progressionDisplay.setVisible (showingMelody);
    pianoRoll.setVisible (showingMelody);
    melodyDragHandle.setVisible (showingMelody);
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
    undoChordButton.setEnabled (progression.all().size() > 1);
    resetChordsButton.setEnabled (progression.all().size() > 1);
    repaint();
}

void ChordSuggesterAudioProcessorEditor::undoLastChord()
{
    if (! progression.removeLast())
        return;

    // Step back: the chord before the removed one becomes the chord you have again.
    setCurrentChord (progression.all().back());
    refreshMelody();
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
    dropMessage.clear();
    suggestions = chords::suggest (currentChord, keyRoot);
    updateMoreChords();
    repaint();
}

void ChordSuggesterAudioProcessorEditor::updateMoreChords()
{
    moreChords.clear (juce::dontSendNotification);
    moreChordOptions.clear();

    const auto alreadyShown = [this] (const chords::Chord& chord)
    {
        if (chords::sameNotes (chord, currentChord))
            return true;
        for (const auto& option : moreChordOptions)
            if (chords::sameNotes (option, chord))
                return true;
        return std::any_of (suggestions.begin(), suggestions.end(),
                            [&chord] (const chords::Suggestion& s) { return chords::sameNotes (s.chord, chord); });
    };

    const auto add = [this] (const chords::Chord& chord, const juce::String& label)
    {
        moreChordOptions.push_back (chord);
        moreChords.addItem (label, (int) moreChordOptions.size());
    };

    // Chords from outside the key that real songs do play after this one, most common first.
    auto common = chords::connectedChords (currentChord, keyRoot);
    std::stable_sort (common.begin(), common.end(), [] (const chords::Suggestion& a, const chords::Suggestion& b)
                      { return a.chance > b.chance; });

    bool headingAdded = false;
    for (const auto& s : common)
    {
        if (alreadyShown (s.chord))
            continue;
        if (! headingAdded)
        {
            moreChords.addSectionHeading ("Common next in real songs");
            headingAdded = true;
        }
        const int percent = juce::roundToInt (s.chance * 100.0f);
        add (s.chord, juce::String (s.chord.fullName) + "  (" + (percent < 1 ? juce::String ("<1") : juce::String (percent)) + "%)");
    }

    // Everything else: every major, minor and diminished chord, starting from the key note.
    headingAdded = false;
    for (int state = 0; state < chords::data::numStates; ++state)
    {
        const auto chord = chords::chordFromDataState (state, keyRoot);
        if (alreadyShown (chord))
            continue;
        if (! headingAdded)
        {
            moreChords.addSectionHeading ("All other chords");
            headingAdded = true;
        }
        add (chord, chord.fullName);
    }
}

void ChordSuggesterAudioProcessorEditor::chooseMoreChord()
{
    const int id = moreChords.getSelectedId();
    if (id < 1 || id > (int) moreChordOptions.size())
        return;

    // Copy first: setCurrentChord rebuilds moreChordOptions.
    const chords::Chord chosen = moreChordOptions[(size_t) id - 1];
    midiKeyboard.showChordPlayback (chosen.notes);
    audioProcessor.previewChord (chosen);
    setCurrentChord (chosen);
    progression.confirm (currentChord);
    refreshMelody (false);
}

void ChordSuggesterAudioProcessorEditor::updateKeySelection()
{
    const int selectedId = keySelector.getSelectedId();
    if (selectedId < 1 || selectedId > 12)
        return;

    keyRoot = selectedId - 1;
    suggestions = chords::suggest (currentChord, keyRoot);
    previewedChord.reset();
    updateMoreChords();
    repaint();
}

juce::Rectangle<int> ChordSuggesterAudioProcessorEditor::currentChordBounds() const
{
    return { 24, 120, leftColumnWidth - 48, 148 };
}

juce::Rectangle<int> ChordSuggesterAudioProcessorEditor::transitionBounds() const
{
    return { 24, 294, leftColumnWidth - 48, 96 };
}

juce::Rectangle<int> ChordSuggesterAudioProcessorEditor::progressionBounds() const
{
    return { 24, 440, leftColumnWidth - 48, getHeight() - 112 - 30 - 66 - 440 };
}

void ChordSuggesterAudioProcessorEditor::paintProgression (juce::Graphics& g)
{
    const auto area = progressionBounds();
    g.setColour (secondaryText);
    g.setFont (juce::FontOptions (17.0f).withStyle ("Bold"));
    g.drawText ("Your progression", area.getX(), area.getY() - 30, area.getWidth(), 24,
                juce::Justification::centredLeft);

    g.setColour (panelColour);
    g.fillRoundedRectangle (area.toFloat(), 12.0f);
    g.setColour (juce::Colour (0xff353d4b));
    g.drawRoundedRectangle (area.toFloat(), 12.0f, 1.0f);

    // Chords as chips with arrows between, wrapping onto new lines. If they don't all
    // fit, the oldest are left out and a "..." chip stands in for them.
    const juce::Font chipFont (juce::FontOptions (15.0f).withStyle ("Bold"));
    const auto& all = progression.all();
    const auto inner = area.reduced (12, 10);
    const int chipHeight = 28, lineGap = 8, arrowWidth = 20;
    const auto ellipsis = juce::String::fromUTF8 ("\xe2\x80\xa6");

    struct Chip { juce::String text; juce::Rectangle<int> bounds; bool current; };
    const auto layout = [&] (size_t first, std::vector<Chip>& chips)
    {
        chips.clear();
        int x = inner.getX(), y = inner.getY();
        std::vector<std::pair<juce::String, bool>> items;
        if (first > 0) items.push_back ({ ellipsis, false });
        for (size_t i = first; i < all.size(); ++i)
            items.push_back ({ juce::String (all[i].name), i + 1 == all.size() });

        for (size_t i = 0; i < items.size(); ++i)
        {
            const int width = juce::roundToInt (juce::GlyphArrangement::getStringWidth (chipFont, items[i].first)) + 22;
            if (x > inner.getX() && x + width > inner.getRight())
            {
                x = inner.getX();
                y += chipHeight + lineGap;
            }
            chips.push_back ({ items[i].first, { x, y, width, chipHeight }, items[i].second });
            x += width + arrowWidth;
        }
        return chips.empty() || chips.back().bounds.getBottom() <= inner.getBottom();
    };

    std::vector<Chip> chips;
    for (size_t first = 0; first < all.size() && ! layout (first, chips); ++first) {}

    g.setFont (chipFont);
    for (size_t i = 0; i < chips.size(); ++i)
    {
        const auto& chip = chips[i];
        g.setColour (chip.current ? accentBlue : juce::Colour (0xff2b3240));
        g.fillRoundedRectangle (chip.bounds.toFloat(), 8.0f);
        g.setColour (juce::Colour (0xfff1f3f6));
        g.drawText (chip.text, chip.bounds, juce::Justification::centred);

        // Arrow to the next chip when it's on the same line.
        if (i + 1 < chips.size() && chips[i + 1].bounds.getY() == chip.bounds.getY())
        {
            g.setColour (secondaryText);
            g.drawText (juce::String::fromUTF8 ("\xe2\x86\x92"), chip.bounds.getRight(), chip.bounds.getY(),
                        arrowWidth, chipHeight, juce::Justification::centred);
        }
    }
}

void ChordSuggesterAudioProcessorEditor::updateCursor (juce::Point<int> position)
{
    if (showingMelody) return;
    const bool clickable = rowAt (position) >= 0 || currentChordBounds().contains (position)
        || (previewedChord.has_value() && transitionBounds().contains (position));
    setMouseCursor (clickable ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

juce::Rectangle<int> ChordSuggesterAudioProcessorEditor::rowBounds (int index) const
{
    const int left = leftColumnWidth + 28;
    const int top = 114;
    const int bottom = getHeight() - 100;
    const int gap = 8;
    const int count = juce::jmax (1, (int) suggestions.size());
    const int rowHeight = juce::jmax (44, (bottom - top - gap * (count - 1)) / count);
    return { left, top + index * (rowHeight + gap), getWidth() - left - 24, rowHeight };
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


    g.setColour (juce::Colour (0xff303540));
    g.drawVerticalLine (leftColumnWidth, float (headerHeight), float (getHeight()));
    g.setColour (secondaryText);
    g.setFont (juce::FontOptions (16.0f));
    g.drawText ("Key", getWidth() - 250, 22, 48, 32, juce::Justification::centredRight);
    g.setColour (secondaryText);
    g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
    g.drawText ("Your chord", 24, 80, leftColumnWidth - 48, 28,
                juce::Justification::centredLeft);

    const auto current = currentChordBounds();
    g.setColour (midiDragHovering ? juce::Colour (0xff253348) : juce::Colour (0xff222630));
    g.fillRoundedRectangle (current.toFloat(), 13.0f);
    g.setColour (midiDragHovering ? accentBlue : juce::Colour (0xff444c5c));
    g.drawRoundedRectangle (current.toFloat(), 13.0f, midiDragHovering ? 2.5f : 1.5f);
    g.setColour (juce::Colour (0xfff1f3f6));
    g.setFont (juce::FontOptions (44.0f).withStyle ("Bold"));
    g.drawText (currentChord.name, current.withTrimmedTop (28).withHeight (56),
                juce::Justification::centred);
    g.setFont (juce::FontOptions (19.0f).withStyle ("Bold"));
    g.drawText (currentChord.fullName, current.withTrimmedTop (88).withHeight (28),
                juce::Justification::centred);
    g.setColour (secondaryText);
    g.setFont (juce::FontOptions (14.0f));
    const auto currentChordHint = midiDragHovering ? juce::String ("Drop to use this clip's first chord")
                                : dropMessage.isNotEmpty() ? dropMessage
                                : juce::String::fromUTF8 ("Click to replay \xc2\xb7 or drop a MIDI clip here");
    g.drawText (currentChordHint, current.withTrimmedTop (120).withHeight (22).reduced (10, 0),
                juce::Justification::centred);

    const auto transition = transitionBounds();
    g.setColour (previewedChord ? juce::Colour (0xff253348) : panelColour);
    g.fillRoundedRectangle (transition.toFloat(), 12.0f);
    g.setColour (juce::Colour (0xff353d4b));
    g.drawRoundedRectangle (transition.toFloat(), 12.0f, 1.0f);
    g.setColour (secondaryText);
    g.setFont (juce::FontOptions (15.0f).withStyle ("Bold"));
    g.drawText ("Preview next", transition.withTrimmedTop (5).withHeight (22),
                juce::Justification::centred);
    g.setColour (previewedChord ? juce::Colour (0xfff1f3f6) : secondaryText);
    g.setFont (juce::FontOptions (25.0f));
    const auto transitionLabel = juce::String (currentChord.name)
        + juce::String::fromUTF8 (" \xe2\x86\x92 ")
        + (previewedChord ? juce::String (previewedChord->name)
                          : juce::String::fromUTF8 ("\xe2\x80\x94"));
    g.drawText (transitionLabel, transition.withTrimmedTop (26).withHeight (40),
                juce::Justification::centred);
    g.setColour (secondaryText);
    g.setFont (juce::FontOptions (12.5f));
    g.drawText (previewedChord ? "Click to hear the transition" : "Preview a recommendation first",
                transition.getX(), transition.getY() + 69, transition.getWidth(), 22,
                juce::Justification::centred);

    paintProgression (g);

    const auto keyboardArea = juce::Rectangle<int> (24, getHeight() - 112, leftColumnWidth - 48, 88);
    g.setColour (secondaryText);
    g.setFont (juce::FontOptions (17.0f).withStyle ("Bold"));
    g.drawText ("MIDI keyboard", 24, keyboardArea.getY() - 30, leftColumnWidth - 48, 24,
                juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff353d4b));
    g.drawRoundedRectangle (keyboardArea.toFloat(), 8.0f, 1.0f);

    g.setColour (secondaryText);
    g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
    g.drawText ("Chords to try next", leftColumnWidth + 28, 78,
                getWidth() - leftColumnWidth - 52, 30, juce::Justification::centredLeft);

    for (int i = 0; i < (int) suggestions.size(); ++i)
    {
        const auto& suggestion = suggestions[(size_t) i];
        const auto row = rowBounds (i);
        auto colour = dramaticGray.interpolatedWith (accentBlue, suggestion.safety);
        if (i == hoveredRow)
            colour = colour.brighter (0.18f);

        g.setColour (colour);
        g.fillRoundedRectangle (row.toFloat(), 12.0f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (25.0f).withStyle ("Bold"));
        g.drawText (suggestion.chord.name, row.withTrimmedLeft (20).withWidth (92),
                    juce::Justification::centredLeft);

        const int detailsX = row.getX() + 116;
        const int detailsWidth = juce::jmax (70, row.getWidth() - 290);
        g.setFont (juce::FontOptions (16.0f).withStyle ("Bold"));
        g.drawText (suggestion.chord.fullName, detailsX, row.getY() + 8, detailsWidth, 22,
                    juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (14.0f));
        const bool useFlats = keyRoot == 1 || keyRoot == 3 || keyRoot == 5 || keyRoot == 8 || keyRoot == 10;
        const auto details = noteNames (suggestion.chord.notes, useFlats)
            + juce::String::fromUTF8 (" \xc2\xb7 shares ")
            + juce::String (suggestion.sharedNotes)
            + (suggestion.sharedNotes == 1 ? " note" : " notes");
        g.drawText (details, detailsX, row.getY() + 31, detailsWidth, 20,
                    juce::Justification::centredLeft);

        g.setFont (juce::FontOptions (15.0f).withStyle ("Bold"));
        g.drawText (safetyDescription (suggestion.safety), row.getRight() - 112, row.getY(),
                    92, row.getHeight(), juce::Justification::centredRight);
    }

    g.setColour (secondaryText);
    g.setFont (juce::FontOptions (12.5f));
    g.drawText (juce::String::fromUTF8 ("Click to preview \xc2\xb7 Double-click to select"),
                leftColumnWidth + 28, getHeight() - 76, getWidth() - leftColumnWidth - 52, 20,
                juce::Justification::centred);

    const int legendY = getHeight() - 39;
    g.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
    g.drawText ("Safe", leftColumnWidth + 28, legendY - 5, 42, 22, juce::Justification::centredLeft);
    const int legendX = leftColumnWidth + 76;
    const int legendWidth = juce::jmax (100, getWidth() - leftColumnWidth - 180);
    const int segmentGap = 5;
    const int segmentWidth = (legendWidth - segmentGap * 5) / 6;
    for (int i = 0; i < 6; ++i)
    {
        const float safety = 1.0f - (float) i / 5.0f;
        g.setColour (dramaticGray.interpolatedWith (accentBlue, safety));
        g.fillRoundedRectangle ((float) (legendX + i * (segmentWidth + segmentGap)),
                                (float) legendY, (float) segmentWidth, 10.0f, 4.0f);
    }
    g.setColour (secondaryText);
    g.drawText ("Dramatic", legendX + legendWidth + 8, legendY - 5, 72, 22,
                juce::Justification::centredLeft);
}

void ChordSuggesterAudioProcessorEditor::resized()
{
    keySelector.setBounds (getWidth() - 200, 22, 176, 32);
    moreChords.setBounds (getWidth() - 264, 76, 240, 32);
    const auto progressionArea = progressionBounds();
    const int halfWidth = (progressionArea.getWidth() - 10) / 2;
    undoChordButton.setBounds (progressionArea.getX(), progressionArea.getBottom() + 10, halfWidth, 32);
    resetChordsButton.setBounds (progressionArea.getRight() - halfWidth, progressionArea.getBottom() + 10, halfWidth, 32);
    const auto keyboardArea = juce::Rectangle<int> (24, getHeight() - 112, leftColumnWidth - 48, 88);
    midiKeyboard.setBounds (keyboardArea.reduced (1));
    midiKeyboard.setKeyWidth (float (midiKeyboard.getWidth()) / 7.0f);
    chordsTab.setBounds (280, 16, 100, 40);
    melodyTab.setBounds (400, 16, 110, 40);
    scaleChoice.setBounds (getWidth() - 285, 22, 261, 32);
    generateButton.setBounds (200, 86, 180, 36);
    melodyDragHandle.setBounds (392, 86, 172, 36);
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
    const auto position = event.getPosition();
    const int row = rowAt (position);

    if (row >= 0)
    {
        previewedChord = suggestions[(size_t) row].chord;
        midiKeyboard.showChordPlayback (previewedChord->notes);
        audioProcessor.previewChord (*previewedChord);
        repaint();
    }
    else if (currentChordBounds().contains (position))
    {
        midiKeyboard.showChordPlayback (currentChord.notes);
        audioProcessor.previewChord (currentChord);
    }
    else if (previewedChord && transitionBounds().contains (position))
    {
        midiKeyboard.showTransitionPlayback (currentChord.notes, previewedChord->notes);
        audioProcessor.previewTransition (currentChord, *previewedChord);
    }
    updateCursor (event.getPosition());
}

void ChordSuggesterAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (showingMelody || ! event.mods.isLeftButtonDown() || event.getNumberOfClicks() != 2)
        return;
    const int row = rowAt (event.getPosition());
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
    const int row = showingMelody ? -1 : rowAt (event.getPosition());
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

//==============================================================================
// Dropping a MIDI file on the window

namespace
{
// MIDI files (.mid) and Studio One Musicloops (.musicloop).
bool isMidiFile (const juce::String& path)
{
    return midi_reader::canRead (juce::File (path));
}

// The different notes in a group, lowest first, as text with a dot between each one.
// A note played in more than one octave is only listed once.
juce::String playedNoteNames (std::vector<int> notes, bool useFlats)
{
    std::sort (notes.begin(), notes.end());

    juce::String result;
    bool listed[12] = {};

    for (int note : notes)
    {
        const int pitch = ((note % 12) + 12) % 12;
        if (listed[pitch])
            continue;

        listed[pitch] = true;

        if (result.isNotEmpty())
            result += juce::String::fromUTF8 (" \xc2\xb7 ");

        result += juce::String::fromUTF8 (chords::noteName (pitch, useFlats).c_str());
    }

    return result;
}
}

bool ChordSuggesterAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
        if (isMidiFile (path))
            return true;

    return false;
}

void ChordSuggesterAudioProcessorEditor::fileDragEnter (const juce::StringArray&, int, int)
{
    midiDragHovering = true;
    repaint();
}

void ChordSuggesterAudioProcessorEditor::fileDragExit (const juce::StringArray&)
{
    midiDragHovering = false;
    repaint();
}

void ChordSuggesterAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    midiDragHovering = false;

    for (const auto& path : files)
    {
        if (! isMidiFile (path))
            continue;

        const juce::File file (path);
        const std::vector<int> played = midi_reader::firstChordNotes (file);
        const bool keyUsesFlats = keyRoot == 1 || keyRoot == 3 || keyRoot == 5 || keyRoot == 8 || keyRoot == 10;
        chords::Chord dropped;

        if (chords::chordFromNotes (played, keyRoot, dropped))
        {
            setCurrentChord (dropped);

            // Play it and light up the keyboard, the same as clicking "Your chord".
            midiKeyboard.showChordPlayback (currentChord.notes);
            audioProcessor.previewChord (currentChord);

            // Spell the notes to match the chord's name: flats for "Ab minor", sharps for "F# major".
            const juce::String name (dropped.name);
            const bool useFlats = name.substring (1, 2) == "b" || (keyUsesFlats && name.substring (1, 2) != "#");
            dropMessage = "Notes played: " + playedNoteNames (played, useFlats);
        }
        else if (played.empty())
        {
            dropMessage = "No notes found in " + file.getFileName();
        }
        else
        {
            dropMessage = "No chord in these notes: " + playedNoteNames (played, keyUsesFlats);
        }

        break;   // only the first MIDI file is used
    }

    repaint();
}
