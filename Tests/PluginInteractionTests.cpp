#include "PluginEditor.h"
#include <iostream>
#include <cstdlib>

static void check (bool condition, const char* message)
{
    if (! condition) { std::cerr << message << '\n'; std::exit (1); }
}
static juce::TextButton& button (juce::Component& editor, const juce::String& name)
{
    for (auto* child : editor.getChildren())
        if (auto* result = dynamic_cast<juce::TextButton*> (child))
            if (result->getButtonText() == name) return *result;
    std::abort();
}
static juce::TextEditor& history (juce::Component& editor)
{
    for (auto* child : editor.getChildren())
        if (auto* result = dynamic_cast<juce::TextEditor*> (child)) return *result;
    std::abort();
}
static juce::MouseEvent event (juce::Component& editor, int count)
{
    const auto now = juce::Time::getCurrentTime();
    const juce::Point<float> point { 430.0f, 140.0f };
    return { juce::Desktop::getInstance().getMainMouseSource(), point,
             juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
             1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &editor, &editor, now, point, now, count, false };
}
static void snapshot (juce::Component& editor, const juce::String& filename)
{
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
    juce::FileOutputStream stream (juce::File::getCurrentWorkingDirectory().getChildFile (filename));
    check (stream.openedOk(), "Snapshot file opens");
    stream.setPosition (0);
    stream.truncate();
    juce::PNGImageFormat png;
    check (png.writeImageToStream (image, stream), "Snapshot renders");
}

static juce::MouseEvent pointerEvent (juce::Component& component, juce::Point<float> point, int count = 1,
                                      int button = juce::ModifierKeys::leftButtonModifier)
{
    const auto now = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(), point,
             juce::ModifierKeys (button),
             1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, now, point, now, count, false };
}

// A mouse event at `point` for a press that started at `downPoint` (for drags).
static juce::MouseEvent dragEvent (juce::Component& component, juce::Point<float> point, juce::Point<float> downPoint)
{
    const auto now = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(), point,
             juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
             1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, now, downPoint, now, 1, false };
}

static void testEditing()
{
    melody::Phrase phrase;
    phrase.lengthBeats = 4;
    phrase.notes = { {72, 1, 0}, {74, 1, 2} };
    PianoRoll roll (phrase);
    roll.setSize (1000, 440);
    roll.setScale ({0, false});
    roll.setGrid (0.5);
    int auditions = 0, edits = 0;
    roll.onAudition = [&] (int) { ++auditions; };
    roll.onEdit = [&] { ++edits; };
    const double beatWidth = roll.noteBounds ({72, 1, 1}).getX() - roll.noteBounds ({72, 1, 0}).getX();
    auto point = roll.noteBounds (phrase.notes[0]).getCentre();
    roll.mouseDown (pointerEvent (roll, point));
    check (auditions == 1, "Selecting note auditions it");
    roll.mouseDrag (pointerEvent (roll, point + juce::Point<float> (float (beatWidth * 0.5), -16)));
    roll.mouseUp (pointerEvent (roll, point));
    check (phrase.notes[0].startBeat == 0.5 && phrase.notes[0].midi == 73 && edits > 0, "Drag snaps time and permits out-of-scale pitch");
    auto bounds = roll.noteBounds (phrase.notes[0]);
    point = { bounds.getRight() - 1, bounds.getCentreY() };
    roll.mouseDown (pointerEvent (roll, point));
    roll.mouseDrag (pointerEvent (roll, point + juce::Point<float> (float (beatWidth * 0.5), 0)));
    roll.mouseUp (pointerEvent (roll, point));
    check (phrase.notes[0].beats == 1.5, "Right-edge resize changes duration");
    bounds = roll.noteBounds (phrase.notes[0]);
    point = { bounds.getRight() - 1, bounds.getCentreY() };
    roll.mouseDown (pointerEvent (roll, point));
    roll.mouseDrag (pointerEvent (roll, point + juce::Point<float> (float (beatWidth), 0)));
    roll.mouseUp (pointerEvent (roll, point));
    check (roll.lastEditRejected() && phrase.notes[0].beats == 1.5, "Overlap gesture rejected");
    roll.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey));
    check (phrase.notes.size() == 1, "Delete removes selected note");
    bounds = roll.noteBounds ({72, 0.5, 0});
    point = { bounds.getX() + 2, bounds.getCentreY() };
    roll.mouseDoubleClick (pointerEvent (roll, point, 2));
    check (phrase.notes.size() == 2 && phrase.notes.back().startBeat == 0 && phrase.notes.back().beats == 0.5, "Double-click adds grid note");
    roll.keyPressed (juce::KeyPress (juce::KeyPress::backspaceKey));
    check (phrase.notes.size() == 1, "Backspace removes selected note");

    // Drawing: click an empty cell for a one-step note, drag right to lengthen it.
    phrase.notes = { {74, 1, 2} };
    bounds = roll.noteBounds ({67, 0.5, 0});
    point = { bounds.getX() + 2, bounds.getCentreY() };
    roll.mouseDown (pointerEvent (roll, point));
    check (phrase.notes.size() == 2 && phrase.notes.back().midi == 67 && phrase.notes.back().beats == 0.5
           && phrase.notes.back().startBeat == 0, "Click on empty grid draws a note");
    roll.mouseDrag (pointerEvent (roll, point + juce::Point<float> (float (beatWidth * 1.2), 0)));
    check (phrase.notes.back().beats == 1.5, "Dragging lengthens the drawn note to the grid");
    roll.mouseDrag (pointerEvent (roll, point + juce::Point<float> (float (beatWidth * 3.5), 0)));
    roll.mouseUp (pointerEvent (roll, point));
    check (phrase.notes.back().beats == 2.0, "Drawn note stretches up to the next note");
    roll.mouseDown (pointerEvent (roll, roll.noteBounds (phrase.notes.back()).getCentre(), 1,
                                  juce::ModifierKeys::rightButtonModifier));
    check (phrase.notes.size() == 1 && phrase.notes[0].midi == 74, "Right-click deletes a note");
}
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    testEditing();
    ChordSuggesterAudioProcessor processor;
    processor.prepareToPlay (48000, 128);
    ChordSuggesterAudioProcessorEditor editor (processor);
    check (history (editor).getText() == "C", "Starting chord recorded once");
    editor.mouseDown (event (editor, 1));
    check (history (editor).getText() == "C", "UI preview excluded from history");
    editor.mouseDown (event (editor, 2));
    editor.mouseDoubleClick (event (editor, 2));
    check (history (editor).getText().contains (" -> "), "UI confirmation records chord");
    const auto confirmed = history (editor).getText();
    for (auto* child : editor.getChildren())
        if (auto* choice = dynamic_cast<juce::ComboBox*> (child); choice != nullptr && choice->getComponentID() == "scale")
            check (choice->getSelectedId() == 1 && choice->getText() != "Estimated scale", "Estimated key display updates without losing automatic selection");
    snapshot (editor, "chords-tab.png");
    for (auto* child : editor.getChildren())
        if (auto* choice = dynamic_cast<juce::ComboBox*> (child); choice != nullptr && choice->getComponentID().isEmpty())
        {
            choice->setSelectedId (3, juce::dontSendNotification);
            choice->onChange();
            check (history (editor).getText() == confirmed, "Chord key selector preserves confirmed history");
            choice->setSelectedId (1, juce::dontSendNotification);
            choice->onChange();
        }
    button (editor, "Melody").onClick();
    check (history (editor).getText() == confirmed, "Tab switch retains history");
    check (! button (editor, "Play").isEnabled(), "Play disabled before generation");
    button (editor, "New suggestion").onClick();
    check (button (editor, "Play").isEnabled(), "Generate enables Play");
    button (editor, "Chords").onClick();
    for (int i = 0; i < 8; ++i)
    {
        editor.mouseDown (event (editor, 1));
        editor.mouseDoubleClick (event (editor, 2));
    }
    check (! button (editor, "Play").isEnabled(), "New confirmation invalidates phrase");
    button (editor, "Melody").onClick();
    button (editor, "New suggestion").onClick();
    snapshot (editor, "melody-four-bars.png");
    for (auto* child : editor.getChildren())
        if (auto* choice = dynamic_cast<juce::ComboBox*> (child); choice != nullptr && choice->getComponentID() == "bars")
        {
            choice->setSelectedId (8, juce::dontSendNotification);
            choice->onChange();
        }
    check (! button (editor, "Play").isEnabled(), "Bar count invalidates phrase");
    snapshot (editor, "melody-empty.png");
    button (editor, "New suggestion").onClick();
    snapshot (editor, "melody-eight-bars.png");
    for (auto* child : editor.getChildren())
        if (auto* choice = dynamic_cast<juce::ComboBox*> (child); choice != nullptr && choice->getComponentID() == "timing")
        {
            choice->setSelectedId (3, juce::dontSendNotification);
            choice->onChange();
        }
    check (button (editor, "Play").isEnabled(), "Timing change retains existing phrase");
    editor.setSize (960, 600);
    for (auto* child : editor.getChildren())
        if (auto* roll = dynamic_cast<PianoRoll*> (child))
            for (auto* scroller : roll->getChildren())
                if (auto* scroll = dynamic_cast<juce::ScrollBar*> (scroller))
                    scroll->setCurrentRangeStart (scroll->getMaximumRangeLimit() - scroll->getCurrentRangeSize());
    snapshot (editor, "melody-scrolled-compact.png");
    editor.setSize (1160, 760);
    for (auto* child : editor.getChildren())
        if (auto* choice = dynamic_cast<juce::ComboBox*> (child); choice != nullptr && choice->getComponentID() == "scale")
        {
            choice->setSelectedId (15, juce::dontSendNotification);
            choice->onChange();
        }
    button (editor, "New suggestion").onClick();
    snapshot (editor, "melody-scale-override.png");
    button (editor, "With chords").setToggleState (true, juce::dontSendNotification);
    button (editor, "With chords").onClick();
    button (editor, "Loop").setToggleState (true, juce::dontSendNotification);
    button (editor, "Loop").onClick();
    button (editor, "Play").onClick();
    juce::AudioBuffer<float> audio (2, 128);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, uint8_t (100)), 0);
    const int midiBytes = midi.getNumEvents();
    audio.clear();
    processor.processBlock (audio, midi);
    check (audio.getMagnitude (0, 128) > 0 && midi.getNumEvents() == midiBytes, "Audible melody preserves MIDI");
    check (button (editor, "Stop").isEnabled(), "Play becomes Stop");
    const auto beforeToggle = processor.melodyPlayhead();
    for (const bool enabled : { false, true, false, true })
    {
        button (editor, "With chords").setToggleState (enabled, juce::dontSendNotification);
        button (editor, "With chords").onClick();
        for (int block = 0; block < 8; ++block) { audio.clear(); processor.processBlock (audio, midi); }
        check (button (editor, "Stop").isEnabled() && processor.melodyPlayhead() > beforeToggle,
               "Backing toggle preserves playback and advances playhead");
    }
    for (int i = 0; i < 128; ++i) check (audio.getSample (0, i) == audio.getSample (1, i), "Stereo identical preview");
    button (editor, "Chords").onClick();
    for (int block = 0; block < 8; ++block) { audio.clear(); processor.processBlock (audio, midi); }
    audio.clear();
    processor.processBlock (audio, midi);
    check (audio.getMagnitude (0, 128) == 0, "Tab switch stops playback");
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < 128; ++sample) audio.setSample (channel, sample, 0.25f);
    processor.processBlock (audio, midi);
    check (audio.getSample (0, 64) == 0.25f, "Audio passes through" );
    juce::MidiBuffer keyboardMidi;
    processor.getMidiKeyboardState().noteOn (1, 67, 0.8f);
    audio.clear();
    processor.processBlock (audio, keyboardMidi);
    check (keyboardMidi.getNumEvents() > 0, "Incoming MIDI keyboard feature emits MIDI");
    processor.getMidiKeyboardState().noteOff (1, 67, 0.0f);
    keyboardMidi.clear();
    processor.processBlock (audio, keyboardMidi);
    button (editor, "Melody").onClick();
    button (editor, "Reset progression").onClick();
    check (! history (editor).getText().contains (" -> ") && ! button (editor, "Play").isEnabled(), "Reset invalidates phrase");
    button (editor, "New suggestion").onClick();
    for (auto* child : editor.getChildren())
        if (auto* choice = dynamic_cast<juce::ComboBox*> (child); choice != nullptr && choice->getComponentID() == "scale")
        {
            choice->setSelectedId (14, juce::dontSendNotification);
            choice->onChange();
        }
    check (! button (editor, "Play").isEnabled(), "Scale change invalidates phrase");
    ChordSuggesterAudioProcessorEditor reopened (processor);
    check (history (reopened).getText() == "C", "New editor resets prototype history");
    // Dragging a suggestion onto "Your chord" selects it, like a double-click; letting go elsewhere does nothing.
    const juce::Point<float> rowPoint { 600.0f, 150.0f };
    reopened.mouseDown (dragEvent (reopened, rowPoint, rowPoint));
    reopened.mouseDrag (dragEvent (reopened, { 300.0f, 170.0f }, rowPoint));
    reopened.mouseUp (dragEvent (reopened, { 150.0f, 200.0f }, rowPoint));
    check (history (reopened).getText().startsWith ("C -> "), "Dropping a suggestion on Your chord selects it");
    const auto afterDrop = history (reopened).getText();
    reopened.mouseDown (dragEvent (reopened, rowPoint, rowPoint));
    reopened.mouseDrag (dragEvent (reopened, { 700.0f, 400.0f }, rowPoint));
    reopened.mouseUp (dragEvent (reopened, { 700.0f, 400.0f }, rowPoint));
    check (history (reopened).getText() == afterDrop, "Dropping a suggestion elsewhere changes nothing");
    std::cout << "Plugin interaction and passthrough tests passed\n";
}
