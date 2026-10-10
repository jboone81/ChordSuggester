/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int headerHeight = 64;
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

//==============================================================================
ChordSuggesterAudioProcessorEditor::ChordSuggesterAudioProcessorEditor (ChordSuggesterAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      midiKeyboard (p.getMidiKeyboardState())
{
    setCurrentChord (currentChord);

    for (int root = 0; root < 12; ++root)
    {
        const bool useFlats = root == 1 || root == 3 || root == 5 || root == 8 || root == 10;
        const auto name = juce::String::fromUTF8 (chords::noteName (root, useFlats).c_str());
        keySelector.addItem (name + " major", root + 1);
    }

    keySelector.setSelectedId (keyRoot + 1, juce::dontSendNotification);
    keySelector.setJustificationType (juce::Justification::centred);
    keySelector.setColour (juce::ComboBox::backgroundColourId, headerColour);
    keySelector.setColour (juce::ComboBox::textColourId, juce::Colour (0xfff1f3f6));
    keySelector.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff394150));
    keySelector.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xfff1f3f6));
    keySelector.onChange = [this] { updateKeySelection(); };
    addAndMakeVisible (keySelector);

    addAndMakeVisible (midiKeyboard);
    setSize (1120, 640);
}

ChordSuggesterAudioProcessorEditor::~ChordSuggesterAudioProcessorEditor()
{
}

//==============================================================================
void ChordSuggesterAudioProcessorEditor::setCurrentChord (const chords::Chord& newChord)
{
    const chords::Chord chosen = newChord;

    currentChord = chosen;
    previewedChord.reset();
    dropMessage.clear();
    suggestions = chords::suggest (currentChord, keyRoot);
    repaint();
}

void ChordSuggesterAudioProcessorEditor::updateKeySelection()
{
    const int selectedId = keySelector.getSelectedId();
    if (selectedId < 1 || selectedId > 12)
        return;

    keyRoot = selectedId - 1;
    suggestions = chords::suggest (currentChord, keyRoot);
    previewedChord.reset();
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

void ChordSuggesterAudioProcessorEditor::updateCursor (juce::Point<int> position)
{
    const bool clickable = rowAt (position) >= 0 || currentChordBounds().contains (position)
        || (previewedChord.has_value() && transitionBounds().contains (position));
    setMouseCursor (clickable ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

//==============================================================================
void ChordSuggesterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    g.setColour (headerColour);
    g.fillRect (0, 0, getWidth(), headerHeight);
    g.setColour (juce::Colour (0xff303540));
    g.drawHorizontalLine (headerHeight - 1, 0.0f, (float) getWidth());
    g.drawVerticalLine (leftColumnWidth, (float) headerHeight, (float) getHeight());

    g.setColour (juce::Colour (0xfff1f3f6));
    g.setFont (juce::FontOptions (20.0f).withStyle ("Bold"));
    g.drawText (juce::String (JucePlugin_Name).toUpperCase(), 24, 15, 420, 34,
                juce::Justification::centredLeft);
    g.setColour (secondaryText);
    g.setFont (juce::FontOptions (16.0f));
    g.drawText ("Key", getWidth() - 250, 16, 48, 32, juce::Justification::centredRight);

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
                20, 314, 280, 22,
                juce::Justification::centred);

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

        const int playDiameter = juce::jmin (42, row.getHeight() - 12);
        const auto playButton = juce::Rectangle<float> ((float) (row.getRight() - playDiameter - 16),
                                                         row.toFloat().getCentreY() - playDiameter * 0.5f,
                                                         (float) playDiameter, (float) playDiameter);
        g.setColour (juce::Colour (0x332f3b50));
        g.fillEllipse (playButton);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (15.0f).withStyle ("Bold"));
        g.drawText (safetyDescription (suggestion.safety), row.getRight() - 144, row.getY(),
                    92, row.getHeight(), juce::Justification::centredRight);
        juce::Path playIcon;
        playIcon.addTriangle (playButton.getCentreX() - 3.0f, playButton.getCentreY() - 7.0f,
                              playButton.getCentreX() - 3.0f, playButton.getCentreY() + 7.0f,
                              playButton.getCentreX() + 8.0f, playButton.getCentreY());
        g.fillPath (playIcon);
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
    const int selectorWidth = 176;
    keySelector.setBounds (getWidth() - selectorWidth - 24, 13, selectorWidth, 38);

    const auto keyboardArea = juce::Rectangle<int> (24, getHeight() - 112,
                                                     leftColumnWidth - 48, 88);
    midiKeyboard.setBounds (keyboardArea.reduced (1));
    midiKeyboard.setKeyWidth ((float) midiKeyboard.getWidth() / 7.0f);
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
        midiKeyboard.showChordPlayback (previewedChord->notes);
        audioProcessor.previewChord (*previewedChord);
        repaint();
    }
    else if (currentChordBounds().contains (event.getPosition()))
    {
        midiKeyboard.showChordPlayback (currentChord.notes);
        audioProcessor.previewChord (currentChord);
    }
    else if (previewedChord && transitionBounds().contains (event.getPosition()))
    {
        midiKeyboard.showTransitionPlayback (currentChord.notes, previewedChord->notes);
        audioProcessor.previewTransition (currentChord, *previewedChord);
    }

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
