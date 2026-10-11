#pragma once

// MidiChordReader.h
// Reads a MIDI file (for example a clip dragged out of a DAW) and works out the
// first chord in it. This is the only part of the chord logic that needs JUCE,
// because JUCE does the MIDI file reading.
//
// It accepts:
//   - plain MIDI files (.mid, .midi)
//   - Studio One Musicloops (.musicloop), which are packages with a MIDI file inside

#include <JuceHeader.h>
#include <algorithm>
#include <memory>
#include <vector>

#include "ChordEngine.h"

namespace midi_reader
{

// The file types the plugin will take when something is dropped on it.
inline bool canRead (const juce::File& file)
{
    return file.hasFileExtension ("mid;midi;musicloop");
}

// The notes of the first chord in some MIDI data, as MIDI note numbers (60 = middle C).
// Returns an empty list if the data isn't MIDI or has no notes in it.
inline std::vector<int> firstChordNotes (juce::InputStream& stream)
{
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

// The notes of the first chord in a file, as MIDI note numbers (60 = middle C).
// Returns an empty list if the file can't be read or has no notes in it.
inline std::vector<int> firstChordNotes (const juce::File& file)
{
    const char* midiFilePattern = "*.mid;*.midi";

    // A package that is really a folder: use the first MIDI file inside it.
    if (file.isDirectory())
    {
        for (const auto& child : file.findChildFiles (juce::File::findFiles, true, midiFilePattern))
        {
            const auto notes = firstChordNotes (child);
            if (! notes.empty())
                return notes;
        }

        return {};
    }

    // A plain MIDI file.
    if (file.hasFileExtension ("mid;midi"))
    {
        juce::FileInputStream stream (file);
        if (! stream.openedOk())
            return {};

        return firstChordNotes (stream);
    }

    // Anything else, such as a Studio One .musicloop: a zip package with a MIDI
    // file somewhere inside it. Use the first MIDI file found.
    juce::ZipFile package (file);

    for (int i = 0; i < package.getNumEntries(); ++i)
    {
        const auto* entry = package.getEntry (i);
        if (entry == nullptr)
            continue;

        const auto name = entry->filename.toLowerCase();
        if (! (name.endsWith (".mid") || name.endsWith (".midi")))
            continue;

        const std::unique_ptr<juce::InputStream> packed (package.createStreamForEntry (i));
        if (packed == nullptr)
            continue;

        // Unpack it into memory first, then read it like any other MIDI file.
        juce::MemoryBlock unpacked;
        packed->readIntoMemoryBlock (unpacked);
        juce::MemoryInputStream stream (unpacked, false);

        const auto notes = firstChordNotes (stream);
        if (! notes.empty())
            return notes;
    }

    return {};
}

// Reads a file and works out its first chord. Returns true and fills in
// `result` if it found a major, minor or diminished chord; false otherwise.
inline bool chordFromMidiFile (const juce::File& file, int keyRoot, chords::Chord& result)
{
    return chords::chordFromNotes (firstChordNotes (file), keyRoot, result);
}

} // namespace midi_reader
