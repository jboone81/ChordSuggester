#pragma once

// ChordEngine.h
// The "brain" of the plugin: given the chord you have and the key you're in,
// it returns the other chords in that key, ranked from safest to most dramatic.
// Plain C++ with no JUCE in it, so it can be tested on its own.
//
// "Safe" blends two things:
//   - theory: how smoothly the notes move from one chord to the next
//   - data:   how often that move happens in real songs (see ChordTransitions.h)

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "ChordTransitions.h"

namespace chords
{

// Notes are numbers 0-11: C=0, C#=1, D=2 ... B=11.
struct Chord
{
    std::string name;            // e.g. "Em"
    std::string fullName;        // e.g. "E minor"
    std::array<int, 3> notes;    // e.g. { 4, 7, 11 }, with the chord's root note first
};

struct Suggestion
{
    Chord chord;
    int sharedNotes = 0;   // notes in common with the current chord (0-3)
    int movement = 0;      // fewest half-steps the three notes travel in total

    float smoothness = 0.0f;   // theory score: 1 = the notes barely move, 0 = they move the most
    float popularity = 0.0f;   // data score: 1 = the most common next chord in real songs
    float chance = 0.0f;       // how often real songs make this move (0.29 = 29% of the time)

    float safety = 0.0f;   // the blend of the two: 1 = safest (most blue), 0 = most dramatic (most gray)

    bool inKey = true;     // false = this chord uses a note from outside the key
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

// The chord's type: 0 = major, 1 = minor, 2 = diminished, -1 = anything else.
inline int chordType (const Chord& chord)
{
    const int third = (((chord.notes[1] - chord.notes[0]) % 12) + 12) % 12;
    const int fifth = (((chord.notes[2] - chord.notes[0]) % 12) + 12) % 12;

    if (third == 4 && fifth == 7) return 0;
    if (third == 3 && fifth == 7) return 1;
    if (third == 3 && fifth == 6) return 2;
    return -1;
}

// Where to find this chord in the real-song table, or -1 if it isn't a type the table covers.
inline int dataState (const Chord& chord, int keyRoot)
{
    const int type = chordType (chord);
    return type < 0 ? -1 : data::stateOf (chord.notes[0] - keyRoot, type);
}

// True if two chords are made of the same three notes.
inline bool sameNotes (const Chord& first, const Chord& second)
{
    std::array<int, 3> a = first.notes, b = second.notes;
    std::sort (a.begin(), a.end());
    std::sort (b.begin(), b.end());
    return a == b;
}

// Builds a chord from its place in the real-song table (the opposite of dataState).
inline Chord chordFromDataState (int state, int keyRoot)
{
    const int k = ((keyRoot % 12) + 12) % 12;
    const int stepsAboveKeyNote = state / 3;
    const int type = state % 3;   // 0 major, 1 minor, 2 diminished
    const int root = (k + stepsAboveKeyNote) % 12;

    // How to spell the root. Notes just below a scale note are written as flats
    // (Bb, Eb and Ab in the key of C), the note between the 4th and 5th as a sharp
    // (F# in the key of C), and everything else follows the key.
    const bool keyUsesFlats = (k == 5 || k == 10 || k == 3 || k == 8 || k == 1);
    const bool useFlats = (stepsAboveKeyNote == 1 || stepsAboveKeyNote == 3
                           || stepsAboveKeyNote == 8 || stepsAboveKeyNote == 10) ? true
                        : (stepsAboveKeyNote == 6) ? false
                        : keyUsesFlats;
    const std::string rootName = noteName (root, useFlats);

    Chord c;
    c.name     = rootName + (type == 0 ? "" : type == 1 ? "m" : "dim");
    c.fullName = rootName + (type == 0 ? " major" : type == 1 ? " minor" : " diminished");
    c.notes    = { root, (root + (type == 0 ? 4 : 3)) % 12, (root + (type == 2 ? 6 : 7)) % 12 };
    return c;
}

// If real songs rarely use a chord, there isn't enough evidence to go on.
static constexpr int minimumTimesSeenInData = 100;

// Scores every candidate against the current chord, then sorts them safest first.
// Used by both suggest() and connectedChords() below.
inline void scoreAndSort (std::vector<Suggestion>& result, const Chord& current, int keyRoot, float dataWeight)
{
    if (result.empty())
        return;

    for (auto& s : result)
    {
        s.sharedNotes = sharedNotes (current, s.chord);
        s.movement = movement (current, s.chord);
    }

    // Theory score: the smoothest move gets 1, the biggest jump gets 0.
    int lowest = result.front().movement, highest = result.front().movement;
    for (const auto& s : result)
    {
        lowest = std::min (lowest, s.movement);
        highest = std::max (highest, s.movement);
    }

    for (auto& s : result)
        s.smoothness = (highest == lowest) ? 1.0f
                                           : 1.0f - (float) (s.movement - lowest) / (float) (highest - lowest);

    // Data score: the most common next chord gets 1, the rest are measured against it.
    // With too little evidence for the current chord, fall back to theory alone.
    const int from = dataState (current, keyRoot);
    const bool haveData = from >= 0 && data::majorKeyTimesSeen[from] >= minimumTimesSeenInData;

    if (haveData)
    {
        float mostCommon = 0.0f;
        for (auto& s : result)
        {
            const int to = dataState (s.chord, keyRoot);
            s.chance = to >= 0 ? data::chanceOfNext (false, from, to) : 0.0f;
            mostCommon = std::max (mostCommon, s.chance);
        }

        if (mostCommon > 0.0f)
            for (auto& s : result)
                s.popularity = s.chance / mostCommon;
    }

    // Blend the two, then stretch so the best suggestion is always fully blue.
    const float weight = haveData ? std::min (1.0f, std::max (0.0f, dataWeight)) : 0.0f;

    float best = 0.0f;
    for (auto& s : result)
    {
        s.safety = weight * s.popularity + (1.0f - weight) * s.smoothness;
        best = std::max (best, s.safety);
    }

    if (best > 0.0f)
        for (auto& s : result)
            s.safety /= best;

    // Safest first. Ties: the smoother move first.
    std::stable_sort (result.begin(), result.end(), [] (const Suggestion& x, const Suggestion& y)
    {
        if (x.safety != y.safety)
            return x.safety > y.safety;
        return x.movement < y.movement;
    });
}

// The main function: the chords in the key to try next, safest first.
//
// dataWeight sets the mix: 0 = theory only (smooth note movement),
// 1 = data only (what real songs do), 0.5 = an even blend of the two.
inline std::vector<Suggestion> suggest (const Chord& current, int keyRoot, float dataWeight = 0.5f)
{
    std::vector<Suggestion> result;

    for (const auto& candidate : chordsInMajorKey (keyRoot))
    {
        if (sameNotes (candidate, current))
            continue;   // don't suggest the chord you're already on

        Suggestion s;
        s.chord = candidate;
        s.inKey = true;
        result.push_back (s);
    }

    scoreAndSort (result, current, keyRoot, dataWeight);
    return result;
}

// The longer list, meant for a "more chords" drop-down menu: every chord in the key,
// plus chords from OUTSIDE the key that real songs do follow the current chord with.
//
// Each entry has inKey set (true = in the key, false = outside it), so the interface
// can label or group the outside ones. The list is sorted safest first, the same way
// as suggest(), and the scores are worked out across this whole list.
//
// minimumChance is how common an outside chord must be to make the list:
// 0.01 means it follows the current chord at least 1% of the time in real songs.
inline std::vector<Suggestion> connectedChords (const Chord& current, int keyRoot,
                                                float dataWeight = 0.5f, float minimumChance = 0.01f)
{
    const std::vector<Chord> inKeyChords = chordsInMajorKey (keyRoot);
    std::vector<Suggestion> result;

    for (const auto& candidate : inKeyChords)
    {
        if (sameNotes (candidate, current))
            continue;

        Suggestion s;
        s.chord = candidate;
        s.inKey = true;
        result.push_back (s);
    }

    const int from = dataState (current, keyRoot);

    if (from >= 0 && data::majorKeyTimesSeen[from] >= minimumTimesSeenInData)
    {
        for (int state = 0; state < data::numStates; ++state)
        {
            if (data::chanceOfNext (false, from, state) < minimumChance)
                continue;

            const Chord candidate = chordFromDataState (state, keyRoot);

            if (sameNotes (candidate, current))
                continue;

            const bool alreadyListed = std::any_of (inKeyChords.begin(), inKeyChords.end(),
                                                    [&candidate] (const Chord& c) { return sameNotes (c, candidate); });
            if (alreadyListed)
                continue;

            Suggestion s;
            s.chord = candidate;
            s.inKey = false;
            result.push_back (s);
        }
    }

    scoreAndSort (result, current, keyRoot, dataWeight);
    return result;
}

} // namespace chords
