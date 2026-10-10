#pragma once

// ChordEngine.h
// The "brain" of the plugin: given the chord you have and the key you're in,
// it returns the other chords in that key, ranked from safest to most dramatic.
// Plain C++ with no JUCE in it, so it can be tested on its own.

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace chords
{

// Notes are numbers 0-11: C=0, C#=1, D=2 ... B=11.
struct Chord
{
    std::string name;            // e.g. "Em"
    std::string fullName;        // e.g. "E minor"
    std::array<int, 3> notes;    // e.g. { 4, 7, 11 }
};

struct Suggestion
{
    Chord chord;
    int sharedNotes = 0;   // notes in common with the current chord (0-3)
    int movement = 0;      // fewest half-steps the three notes travel in total
    float safety = 0.0f;   // 1 = safest (most blue), 0 = most dramatic (most gray)
};

inline std::string noteName (int note, bool useFlats = false)
{
    static const char* sharps[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flats[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    const int n = ((note % 12) + 12) % 12;
    return useFlats ? flats[n] : sharps[n];
}

// The seven chords that belong to a major key. keyRoot: 0 = C major, 7 = G major, etc.
inline std::vector<Chord> chordsInMajorKey (int keyRoot)
{
    static const int steps[7] = { 0, 2, 4, 5, 7, 9, 11 };
    enum Quality { major, minor, diminished };
    static const Quality qualities[7] = { major, minor, minor, major, major, minor, diminished };

    // Keys that are normally written with flats: F, Bb, Eb, Ab, Db.
    const int k = ((keyRoot % 12) + 12) % 12;
    const bool useFlats = (k == 5 || k == 10 || k == 3 || k == 8 || k == 1);

    std::vector<Chord> result;

    for (int i = 0; i < 7; ++i)
    {
        const int root = (k + steps[i]) % 12;
        const Quality q = qualities[i];
        const int third = (root + (q == major ? 4 : 3)) % 12;
        const int fifth = (root + (q == diminished ? 6 : 7)) % 12;
        const std::string rootName = noteName (root, useFlats);

        Chord c;
        c.name     = rootName + (q == major ? "" : q == minor ? "m" : "dim");
        c.fullName = rootName + (q == major ? " major" : q == minor ? " minor" : " diminished");
        c.notes    = { root, third, fifth };
        result.push_back (c);
    }

    return result;
}

inline int sharedNotes (const Chord& a, const Chord& b)
{
    int count = 0;
    for (int n : a.notes)
        if (std::find (b.notes.begin(), b.notes.end(), n) != b.notes.end())
            ++count;
    return count;
}

// Distance between two notes in half-steps, going whichever way round is shorter.
inline int noteDistance (int a, int b)
{
    const int d = (((a - b) % 12) + 12) % 12;
    return std::min (d, 12 - d);
}

// Tries all six ways of pairing up the notes and keeps the smallest total.
inline int movement (const Chord& from, const Chord& to)
{
    std::array<int, 3> order = { 0, 1, 2 };
    int best = 1000;

    do
    {
        int total = 0;
        for (int i = 0; i < 3; ++i)
            total += noteDistance (from.notes[(size_t) i], to.notes[(size_t) order[(size_t) i]]);
        best = std::min (best, total);
    }
    while (std::next_permutation (order.begin(), order.end()));

    return best;
}

// The main function: the chords to try next, safest first.
inline std::vector<Suggestion> suggest (const Chord& current, int keyRoot)
{
    std::vector<Suggestion> result;

    for (const auto& candidate : chordsInMajorKey (keyRoot))
    {
        std::array<int, 3> a = current.notes, b = candidate.notes;
        std::sort (a.begin(), a.end());
        std::sort (b.begin(), b.end());
        if (a == b)
            continue;   // don't suggest the chord you're already on

        Suggestion s;
        s.chord = candidate;
        s.sharedNotes = sharedNotes (current, candidate);
        s.movement = movement (current, candidate);
        result.push_back (s);
    }

    if (result.empty())
        return result;

    int lowest = result.front().movement, highest = result.front().movement;
    for (const auto& s : result)
    {
        lowest = std::min (lowest, s.movement);
        highest = std::max (highest, s.movement);
    }

    for (auto& s : result)
        s.safety = (highest == lowest) ? 1.0f
                                       : 1.0f - (float) (s.movement - lowest) / (float) (highest - lowest);

    // Safest first. Ties: more shared notes first.
    std::stable_sort (result.begin(), result.end(), [] (const Suggestion& x, const Suggestion& y)
    {
        if (x.movement != y.movement)
            return x.movement < y.movement;
        return x.sharedNotes > y.sharedNotes;
    });

    return result;
}

} // namespace chords
