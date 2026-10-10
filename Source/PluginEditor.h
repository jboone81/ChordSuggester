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
    void updateKeySelection();

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
    RoundedKeySelectorLookAndFeel keySelectorLookAndFeel;
    juce::ComboBox keySelector;
    ChordMidiKeyboard midiKeyboard;

    int keyRoot = 0;   // 0 = key of C major
    chords::Chord currentChord { "C", "C major", { { 0, 4, 7 } } };
    std::vector<chords::Suggestion> suggestions;
    std::optional<chords::Chord> previewedChord;
    int hoveredRow = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordSuggesterAudioProcessorEditor)
};
