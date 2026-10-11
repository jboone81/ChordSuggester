#pragma once

// MidiDragOut.h
// Lets the user drag the suggested melody out of the plugin and drop it on a track
// in their DAW. A plugin can't put notes straight into a DAW, so this writes the
// melody to a temporary MIDI file and hands that file to the DAW as a normal
// file drag, the same as dragging a .mid file in from Finder.

#include <JuceHeader.h>
#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include "MelodyEngine.h"

namespace midi_writer
{

// Writes a phrase to a MIDI file. Returns true if the file was written.
// One beat is 480 ticks. No tempo is stored, so the notes follow the DAW's own tempo.
// With includeChords, each bar's chord is added underneath the melody, voiced the
// same way the plugin plays it back.
inline bool writePhrase (const melody::Phrase& phrase, bool includeChords, const juce::File& destination)
{
    if (phrase.notes.empty() && ! (includeChords && ! phrase.chords.empty()))
        return false;

    const double ticksPerBeat = 480.0;
    juce::MidiMessageSequence track;

    auto addNote = [&] (int midiNote, double startBeat, double endBeat, int velocity)
    {
        if (midiNote < 0 || midiNote > 127 || endBeat - startBeat < 0.01)
            return;

        track.addEvent (juce::MidiMessage::noteOn (1, midiNote, (juce::uint8) velocity), startBeat * ticksPerBeat);
        track.addEvent (juce::MidiMessage::noteOff (1, midiNote), endBeat * ticksPerBeat);
    };

    for (const auto& note : phrase.notes)
        addNote (note.midi, note.startBeat, note.startBeat + note.beats, 100);

    if (includeChords)
    {
        // A piano roll can't hold two notes of the same pitch at the same time, so
        // where the melody plays a note the chord is also holding, the chord note
        // steps aside for the melody note and carries on afterwards.
        auto addChordNote = [&] (int midiNote, double startBeat, double endBeat)
        {
            std::vector<std::pair<double, double>> melodyTimes;

            for (const auto& note : phrase.notes)
                if (note.midi == midiNote && note.startBeat < endBeat && note.startBeat + note.beats > startBeat)
                    melodyTimes.push_back ({ note.startBeat, note.startBeat + note.beats });

            std::sort (melodyTimes.begin(), melodyTimes.end());

            double from = startBeat;

            for (const auto& time : melodyTimes)
            {
                addNote (midiNote, from, std::min (time.first, endBeat), 80);
                from = std::max (from, time.second);
            }

            addNote (midiNote, from, endBeat, 80);
        };

        for (size_t bar = 0; bar < phrase.chords.size(); ++bar)
        {
            // Stack the chord's three notes upward from the C below middle C.
            int previous = -1;

            for (int pitch : phrase.chords[bar].notes)
            {
                int midiNote = 48 + pitch;
                while (midiNote <= previous)
                    midiNote += 12;

                previous = midiNote;
                addChordNote (midiNote, (double) bar * 4.0, (double) bar * 4.0 + 4.0);
            }
        }
    }

    track.sort();
    track.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote ((int) ticksPerBeat);
    file.addTrack (track);

    destination.deleteFile();
    juce::FileOutputStream stream (destination);
    return stream.openedOk() && file.writeTo (stream);
}

// Writes a chord progression to a MIDI file, one chord per bar (four beats each).
// Returns true if the file was written. The chords are voiced the way the plugin plays
// them: the three notes stacked upward from middle C. One beat is 480 ticks and no tempo
// is stored, so the chords follow the DAW's own tempo.
inline bool writeChords (const std::vector<chords::Chord>& progression, const juce::File& destination)
{
    if (progression.empty())
        return false;

    const double ticksPerBar = 4.0 * 480.0;
    juce::MidiMessageSequence track;

    for (size_t bar = 0; bar < progression.size(); ++bar)
    {
        int previous = -1;

        for (int pitch : progression[bar].notes)
        {
            int midiNote = 60 + pitch;
            while (midiNote <= previous)
                midiNote += 12;

            previous = midiNote;
            track.addEvent (juce::MidiMessage::noteOn (1, midiNote, (juce::uint8) 90), (double) bar * ticksPerBar);
            track.addEvent (juce::MidiMessage::noteOff (1, midiNote), (double) (bar + 1) * ticksPerBar);
        }
    }

    track.sort();
    track.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (480);
    file.addTrack (track);

    destination.deleteFile();
    juce::FileOutputStream stream (destination);
    return stream.openedOk() && file.writeTo (stream);
}

// Writes the chord progression to a MIDI file in the system's temporary folder and returns it.
// Returns a file that doesn't exist if there was nothing to write.
inline juce::File writeChordsToTempFile (const std::vector<chords::Chord>& progression)
{
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("EZ Chords & Harmony Chords.mid");

    if (! writeChords (progression, file))
        return {};

    return file;
}

// Writes the phrase to a MIDI file in the system's temporary folder and returns it.
// Returns a file that doesn't exist if there was nothing to write.
inline juce::File writePhraseToTempFile (const melody::Phrase& phrase, bool includeChords)
{
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("EZ Chords & Harmony Melody.mid");

    if (! writePhrase (phrase, includeChords, file))
        return {};

    return file;
}

} // namespace midi_writer

// A small handle the user drags from. When the drag starts it asks `createFile` for
// a MIDI file and passes that file to whatever the user drops it on.
class MidiDragHandle final : public juce::Component
{
public:
    MidiDragHandle()
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    // Set this to a function that writes the MIDI file and returns it.
    std::function<juce::File()> createFile;

    // The words shown on the handle.
    juce::String label { "Drag to your DAW" };

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (0.5f);
        const bool active = isEnabled();

        g.setColour (juce::Colour (0xff20252d));
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (active ? juce::Colour (0xff6f7a8c) : juce::Colour (0xff343a44));
        g.drawRoundedRectangle (area, 6.0f, 1.0f);

        // Six dots: the usual "you can drag this" mark.
        g.setColour (active ? juce::Colour (0xffa7afbc) : juce::Colour (0xff4a5362));
        const float gripX = 14.0f, gripY = (float) getHeight() * 0.5f - 6.0f;
        for (int column = 0; column < 2; ++column)
            for (int row = 0; row < 3; ++row)
                g.fillEllipse (gripX + (float) column * 6.0f, gripY + (float) row * 5.0f, 2.5f, 2.5f);

        g.setColour (active ? juce::Colours::white : juce::Colour (0xff6f7785));
        g.setFont (juce::FontOptions (15.0f));
        g.drawText (label, getLocalBounds().withTrimmedLeft (32).withTrimmedRight (8),
                    juce::Justification::centredLeft);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        dragInProgress = false;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        // Start once per press, and only after the mouse has really moved.
        if (dragInProgress || ! isEnabled() || createFile == nullptr || event.getDistanceFromDragStart() < 6)
            return;

        const juce::File file = createFile();
        if (! file.existsAsFile())
            return;

        dragInProgress = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragInProgress = false;
    }

private:
    bool dragInProgress = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiDragHandle)
};
