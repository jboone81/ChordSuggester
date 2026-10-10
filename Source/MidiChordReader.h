#pragma once

// MidiChordReader.h
// Reads a MIDI file (for example a clip dragged out of a DAW) and works out the
// first chord in it. This is the only part of the chord logic that needs JUCE,
// because JUCE does the MIDI file reading.

#include <JuceHeader.h>
#include <algorithm>
#include <vector>

#include "ChordEngine.h"

namespace midi_reader
{

// The notes of the first chord in a MIDI file, as MIDI note numbers (60 = middle C).
// Returns an empty list if the file can't be read or has no notes in it.
inline std::vector<int> firstChordNotes (const juce::File& file)
{
    juce::FileInputStream stream (file);
    if (! stream.openedOk())
        return {};

    juce::MidiFile midi;
    if (! midi.readFrom (stream))
        return {};

    struct NoteStart
    {
        double time;
        int note;
    };

    std::vector<NoteStart> starts;

    for (int t = 0; t < midi.getNumTracks(); ++t)
    {
        const auto* track = midi.getTrack (t);
        if (track == nullptr)
            continue;

        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& message = track->getEventPointer (i)->message;
            if (message.isNoteOn())
                starts.push_back ({ message.getTimeStamp(), message.getNoteNumber() });
        }
    }

    if (starts.empty())
        return {};

    std::stable_sort (starts.begin(), starts.end(),
                      [] (const NoteStart& a, const NoteStart& b) { return a.time < b.time; });

    // Times are in "ticks". A positive time format is the number of ticks in one beat.
    const double ticksPerBeat = midi.getTimeFormat() > 0 ? (double) midi.getTimeFormat() : 480.0;
    const double firstStart = starts.front().time;

    // Start with the notes that begin together at the very start of the clip.
    // If those don't give three different notes (the chord may be spread out or
    // played one note at a time), widen the search to one beat, then to one bar.
    std::vector<int> notes;

    for (double window : { ticksPerBeat * 0.25, ticksPerBeat, ticksPerBeat * 4.0 })
    {
        notes.clear();
        bool seen[12] = {};
        int differentNotes = 0;

        for (const auto& start : starts)
        {
            if (start.time - firstStart > window)
                break;

            notes.push_back (start.note);

            const int pitch = ((start.note % 12) + 12) % 12;
            if (! seen[pitch])
            {
                seen[pitch] = true;
                ++differentNotes;
            }
        }

        if (differentNotes >= 3)
            break;
    }

    return notes;
}

// Reads a MIDI file and works out its first chord. Returns true and fills in
// `result` if it found a major, minor or diminished chord; false otherwise.
inline bool chordFromMidiFile (const juce::File& file, int keyRoot, chords::Chord& result)
{
    return chords::chordFromNotes (firstChordNotes (file), keyRoot, result);
}

} // namespace midi_reader
