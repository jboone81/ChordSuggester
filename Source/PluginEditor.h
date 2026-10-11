/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ChordEngine.h"
#include "ChordExplain.h"
#include "MidiChordReader.h"
#include "MidiDragOut.h"
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

class ChordMidiKeyboard final : public juce::MidiKeyboardComponent
{
public:
    explicit ChordMidiKeyboard (juce::MidiKeyboardState& state);
    void showChordPlayback (const std::array<int, 3>& notes);
    void showTransitionPlayback (const std::array<int, 3>& first, const std::array<int, 3>& second);
    juce::String getWhiteNoteText (int midiNoteNumber) override;
    void drawWhiteNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour lineColour,
                        juce::Colour textColour) override;
    void drawBlackNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour noteFillColour) override;

private:
    class PlaybackTimer final : private juce::Timer
    {
    public:
        explicit PlaybackTimer (ChordMidiKeyboard& keyboard) : owner (keyboard) {}
        void start() { startTimerHz (30); }
        void stop() { stopTimer(); }

    private:
        void timerCallback() override { owner.updatePlaybackVisual(); }
        ChordMidiKeyboard& owner;
    };

    void updatePlaybackVisual();
    bool isPlaybackNote (int midiNoteNumber) const;
    void startPlaybackVisual (const std::array<int, 3>& first,
                              const std::optional<std::array<int, 3>>& second);

    std::array<int, 3> playbackNotes {};
    std::optional<std::array<int, 3>> nextPlaybackNotes;
    juce::uint32 playbackStartedAt = 0;
    bool showingSecondChord = false;
    PlaybackTimer playbackTimer { *this };
};

class RoundedKeySelectorLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;
};

//==============================================================================
/**
*/
class ChordSuggesterAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                            public juce::FileDragAndDropTarget,
                                            private juce::Timer
{
public:
    ChordSuggesterAudioProcessorEditor (ChordSuggesterAudioProcessor&);
    ~ChordSuggesterAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    // Dropping a MIDI file (.mid) or a Studio One Musicloop (.musicloop) anywhere on
    // the window sets "Your chord" to the first chord in that file.
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void updateKeySelection();

    // Makes newChord the chord you have, then works out fresh suggestions for it.
    void setCurrentChord (const chords::Chord& newChord);

    // The "More chords" drop-down: every chord that isn't already a suggestion.
    void updateMoreChords();
    void chooseMoreChord();

    // Where suggestion number `index` is drawn, which is also where it can be clicked.
    juce::Rectangle<int> rowBounds (int index) const;

    // Which suggestion is under this point, or -1 if none.
    int rowAt (juce::Point<int> position) const;

    // The "Explain why" button on suggestion number `index`, and which one is under a point (-1 if none).
    juce::Rectangle<int> explainButtonBounds (int index) const;
    int explainButtonAt (juce::Point<int> position) const;
    // Opens a pop-up explaining why suggestion number `index` works after the current chord.
    void showExplanation (int index);
    // The small "Explain why" tag beside "Your progression", which explains the Roman numerals.
    juce::Rectangle<int> numeralsHelpBounds() const;
    void showNumeralsHelp();
    juce::Rectangle<int> currentChordBounds() const;
    juce::Rectangle<int> transitionBounds() const;
    juce::Rectangle<int> progressionBounds() const;
    void paintProgression (juce::Graphics&);
    void undoLastChord();
    // Makes this chord the one you have and adds it to the progression (double-click or drop).
    void confirmChord (const chords::Chord& chord);

    // The chords in "Your progression" as laid out on screen. index is the chord's position in
    // progression.all(), or -1 for the "..." chip that stands in for chords that don't fit.
    struct ProgressionChip { juce::String text, numeral; juce::Rectangle<int> bounds; bool current; int index; };
    std::vector<ProgressionChip> progressionChips() const;
    int progressionChipAt (juce::Point<int> position) const;          // progression index, or -1
    juce::Rectangle<int> chipDeleteBounds (const ProgressionChip&) const;   // the small x on a selected chip
    bool selectedChipDeleteContains (juce::Point<int> position) const;
    void deleteProgressionChord (int index);
    void replaceProgressionChord (int index, const chords::Chord& chord);
    // True if a dragged suggestion let go here would be selected: "Your chord" or "Your progression".
    bool isChordDropTarget (juce::Point<int> position) const;
    void toggleProgressionPlayback();
    // Clears the "now playing" highlight; also silences playback when stopAudio is true.
    void endProgressionPlayback (bool stopAudio);
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
    RoundedKeySelectorLookAndFeel keySelectorLookAndFeel;
    juce::ComboBox keySelector;
    juce::ComboBox moreChords;
    std::vector<chords::Chord> moreChordOptions;   // item ID n is moreChordOptions[n - 1]
    ChordMidiKeyboard midiKeyboard;

    int keyRoot = 0;   // 0 = key of C major
    chords::Chord currentChord { "C", "C major", { { 0, 4, 7 } } };
    std::vector<chords::Suggestion> suggestions;
    std::optional<chords::Chord> previewedChord;
    int hoveredRow = -1;
    int selectedChip = -1;             // the progression chord clicked for editing, or -1
    int dragRow = -1;                  // the suggestion the mouse went down on, while the button is held
    bool draggingChord = false;        // true once that press has moved far enough to be a drag
    juce::Point<int> dragPosition;
    int hoveredExplain = -1;   // the "Explain why" button the mouse is over, or -1
    bool hoveredNumeralsHelp = false;   // true while the mouse is over the tag beside "Your progression"
    juce::Component::SafePointer<juce::CallOutBox> explanationBox;   // the explanation pop-up while it is open
    melody::Progression progression { currentChord };
    melody::Estimate scaleEstimate;
    melody::Phrase phrase;
    PianoRoll pianoRoll { phrase };
    uint32_t generationSeed = std::random_device {}();
    bool showingMelody = false;
    NavigationButton chordsTab { "Chords" }, melodyTab { "Melody" };
    juce::TextButton generateButton { "New suggestion" }, playButton { "Play" }, clearNotesButton { "Clear notes" };
    juce::TextButton backingButton { "With chords" }, loopButton { "Loop" };
    juce::TextButton resetButton { "Reset progression" };
    juce::TextButton undoChordButton { "Undo last" }, resetChordsButton { "Reset" };
    juce::TextButton playProgressionButton { "Play" };
    bool progressionPlaying = false;
    juce::uint32 progressionStartedAt = 0;
    size_t progressionFirst = 0, progressionCount = 0;   // which chords are playing
    int progressionSounding = -1;                         // index into progression.all(), or -1
    juce::ComboBox scaleChoice, barsChoice, timingChoice;
    juce::ComboBox soundChoice;   // under the title: which built-in sound plays the chords and melodies
    juce::TextEditor progressionDisplay;
    bool melodyPlaying = false, seenPlayhead = false;
    MidiDragHandle melodyDragHandle;   // on the Melody tab: drag the suggested melody out to a DAW track
    bool midiDragHovering = false;   // true while a MIDI file is being dragged over the window
    juce::String dropMessage;        // shown under the chord after a drop, e.g. "From clip.mid"

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordSuggesterAudioProcessorEditor)
};
