#pragma once

// ChordExplain.h
// Explains, in plain language, why one chord works after another.
// Plain C++ with no JUCE in it, like ChordEngine.h, so it can be tested on its own.
//
// An explanation has up to three points:
//   1. the notes:  which notes the two chords share and how far the others move
//   2. the role:   what job the next chord does in the key (home, away, tension...)
//   3. real songs: how often songs make this move, from the data in ChordTransitions.h

#include <string>
#include <vector>

#include "ChordEngine.h"

namespace chords
{

struct Explanation
{
    std::string title;                  // e.g. "Why Am works after C"
    std::vector<std::string> points;    // one short paragraph each
};

namespace explain_detail
{

// Where a chord sits in a major key: 0 = the home chord, 1 = the chord on the key's
// second note ... 6 = the chord on its seventh note. -1 if the chord isn't in the key.
inline int placeInKey (const Chord& chord, int keyRoot)
{
    const auto inKey = chordsInMajorKey (keyRoot);
    for (size_t i = 0; i < inKey.size(); ++i)
        if (sameNotes (inKey[i], chord))
            return (int) i;
    return -1;
}

// Whether to write this chord's notes with flats (Bb) rather than sharps (A#).
inline bool spelledWithFlats (const Chord& chord, int keyRoot)
{
    if (chord.name.size() > 1 && chord.name[1] == 'b') return true;
    if (chord.name.size() > 1 && chord.name[1] == '#') return false;

    const int k = ((keyRoot % 12) + 12) % 12;
    if (placeInKey (chord, keyRoot) >= 0)
        return k == 5 || k == 10 || k == 3 || k == 8 || k == 1;   // the keys written with flats

    // A chord from outside the key on a plain-letter note: minor and diminished chords
    // are written with flats (F minor is F, Ab, C), major ones with sharps (D major is D, F#, A).
    return chordType (chord) != 0;
}

// "1 piano key" or "3 piano keys".
inline std::string pianoKeys (int count)
{
    return std::to_string (count) + (count == 1 ? " piano key" : " piano keys");
}

// "A", "A and B" or "A, B and C".
inline std::string joinWithAnd (const std::vector<std::string>& items)
{
    std::string text;
    for (size_t i = 0; i < items.size(); ++i)
        text += (i == 0 ? "" : i + 1 == items.size() ? " and " : ", ") + items[i];
    return text;
}

// Point 1: the notes the chords share, and how the rest move.
inline std::string notesPoint (const Chord& current, const Chord& next, int keyRoot)
{
    // Pair each note of the current chord with the note of the next chord it moves to,
    // choosing the pairing where the notes travel the least in total.
    std::array<int, 3> order = { 0, 1, 2 }, best = order;
    int bestTotal = 1000;
    do
    {
        int total = 0;
        for (size_t i = 0; i < 3; ++i)
            total += noteDistance (current.notes[i], next.notes[(size_t) order[i]]);
        if (total < bestTotal) { bestTotal = total; best = order; }
    }
    while (std::next_permutation (order.begin(), order.end()));

    const bool currentFlats = spelledWithFlats (current, keyRoot), nextFlats = spelledWithFlats (next, keyRoot);
    std::vector<std::string> kept, moves;
    for (size_t i = 0; i < 3; ++i)
    {
        const int from = current.notes[i], to = next.notes[(size_t) best[i]];
        const int distance = noteDistance (from, to);
        if (distance == 0) { kept.push_back (noteName (to, nextFlats)); continue; }

        const bool up = (((to - from) % 12) + 12) % 12 == distance;
        // Only the first move spells out "piano keys"; the rest just give the number.
        moves.push_back (noteName (from, currentFlats) + (up ? " moves up " : " moves down ")
                         + (moves.empty() ? pianoKeys (distance) : std::to_string (distance))
                         + " to " + noteName (to, nextFlats));
    }

    if (kept.size() == 2)
        return "It keeps two notes (" + joinWithAnd (kept) + ") and changes only one: " + moves[0]
               + ". That is about as smooth as a chord change gets.";
    if (kept.size() == 1)
        return "It keeps one note (" + kept[0] + ") and the other two move: " + joinWithAnd (moves) + "."
               + (bestTotal <= 4 ? " They don't travel far, so the change is smooth." : "");
    if (kept.empty())
        return "No notes stay the same, so all three move: " + joinWithAnd (moves) + "."
               + (bestTotal <= 4 ? " Each one only takes a small step, so it still sounds connected."
                                 : " That makes it a bigger, more noticeable change.");
    return "It uses the same three notes, so nothing has to move.";
}

// Point 2: the job the next chord does in the key, or what this particular move is known for.
inline std::string rolePoint (const Chord& current, const Chord& next, int keyRoot)
{
    const auto inKey = chordsInMajorKey (keyRoot);
    const std::string home = inKey[0].name, tension = inKey[4].name;
    const std::string key = inKey[0].fullName;                         // e.g. "C major"
    const int from = placeInKey (current, keyRoot), to = placeInKey (next, keyRoot);
    const std::string a = current.name, b = next.name;

    if (to < 0)
    {
        // The next chord uses a note from outside the key.
        std::string text = b + " is not one of the seven chords of " + key + ", so it stands out as a surprise.";
        const int steps = (((next.notes[0] - keyRoot) % 12) + 12) % 12;   // half-steps above the key note
        const int type = chordType (next);

        const bool borrowedFromMinor = (type == 0 && (steps == 3 || steps == 8 || steps == 10))
                                    || (type == 1 && (steps == 0 || steps == 5 || steps == 7));
        if (borrowedFromMinor)
            return text + " It is borrowed from " + noteName (keyRoot, spelledWithFlats (inKey[0], keyRoot))
                   + " minor, the darker version of the same key. Rock and pop songs do this often.";

        // A major chord built on a note of the key pulls towards the key chord a fifth below it.
        if (type == 0 && (steps == 2 || steps == 4 || steps == 9 || steps == 11))
            for (const auto& target : inKey)
                if (target.notes[0] == (next.notes[0] + 5) % 12)
                    return text + " As a major chord it pulls strongly towards " + target.name
                           + ", so it works well as a stepping stone to that chord.";
        return text;
    }

    // Moves that are well known in their own right.
    std::string text;
    if      (from == 4 && to == 0) text = a + " to " + b + " is the strongest \"coming home\" move in music: " + a + " builds tension and " + b + " releases it.";
    else if (from == 3 && to == 0) text = a + " to " + b + " is a gentle way home. It is the \"Amen\" sound that ends many hymns.";
    else if (from == 6 && to == 0) text = a + " is full of tension that " + b + " resolves, so this lands firmly at home.";
    else if (from == 4 && to == 5) text = "After " + a + " the ear expects the home chord (" + home + "). Going to " + b + " instead is a well-known surprise that keeps the song moving.";
    else if (from == 3 && to == 4) text = a + " then " + b + " is the classic build-up: a step away from home, then tension, ready to return to " + home + ".";
    else if (from == 1 && to == 4) text = a + " to " + b + " is one of the most used lead-ups in pop and jazz, and it points straight at the home chord (" + home + ").";
    else if (from == 0 && to == 5) text = b + " is the sad twin of " + a + ": it belongs to the same key but has a darker mood, so the feeling turns inward without going far from home.";
    else if (from == 5 && to == 3) text = a + " to " + b + " is a pop staple. It lifts the mood from sad to open and hopeful.";
    // Otherwise, describe the job the next chord does in the key.
    else if (to == 0) text = b + " is the home chord of " + key + ". Arriving on it feels settled and finished.";
    else if (to == 1) text = b + " is a \"moving on\" chord. It rarely stays put and usually leads towards " + tension + ".";
    else if (to == 2) text = b + " is a soft in-between chord. It shares two notes with the home chord (" + home + "), so it changes the colour without adding much tension.";
    else if (to == 3) text = b + " is the main \"away from home\" chord of " + key + ". It sounds open and bright, like stepping out of the door.";
    else if (to == 4) text = b + " is the tension chord of " + key + ". It pulls back towards the home chord (" + home + "), which makes the listener want to hear what comes next.";
    else if (to == 5) text = b + " is the sad twin of the home chord (" + home + "): it belongs to the same key but has a darker mood.";
    else              text = b + " is the least stable chord in the key. It sounds tense and restless, and usually moves straight on to " + home + ".";

    if (from < 0)
        text += " Coming from " + a + ", which is outside the key, it also brings the music back to familiar ground.";
    return text;
}

// Point 3: how often real songs make this move. Empty if the data doesn't cover these chords.
inline std::string songsPoint (const Chord& current, const Chord& next, int keyRoot)
{
    const int from = dataState (current, keyRoot), to = dataState (next, keyRoot);
    if (from < 0 || to < 0)
        return {};
    if (data::majorKeyTimesSeen[from] < minimumTimesSeenInData)
        return current.name + " is rare in the song data, so there isn't enough evidence to say how often songs do this.";

    const float chance = data::chanceOfNext (false, from, to);
    bool mostCommon = chance > 0.0f;
    for (int other = 0; other < data::numStates; ++other)
        if (other != from && data::chanceOfNext (false, from, other) > chance)
            mostCommon = false;

    const int percent = (int) (chance * 100.0f + 0.5f);
    const std::string howOften = percent < 1 ? "less than 1%" : "about " + std::to_string (percent) + "%";
    const std::string fact = howOften + " of the time, " + current.name + " is followed by " + next.name;

    if (mostCommon)         return "In real songs it is the most common next chord: " + fact + ".";
    if (chance >= 0.10f)    return "Real songs do this often: " + fact + ".";
    if (chance >= 0.03f)    return "Real songs do this fairly often: " + fact + ".";
    if (chance >= 0.005f)   return "Real songs only do this now and then (" + fact + "), so it will sound a little less familiar.";
    return "Real songs almost never do this (" + fact + "), so expect it to sound unusual.";
}

} // namespace explain_detail

// Why `next` works after `current` in the major key that starts on keyRoot (0 = C major).
inline Explanation explain (const Chord& current, const Chord& next, int keyRoot)
{
    using namespace explain_detail;

    Explanation result;
    result.points.push_back (notesPoint (current, next, keyRoot));
    result.points.push_back (rolePoint (current, next, keyRoot));
    const std::string songs = songsPoint (current, next, keyRoot);
    if (! songs.empty())
        result.points.push_back (songs);

    // An honest title for moves that real songs almost never make and that share no notes.
    const int from = dataState (current, keyRoot), to = dataState (next, keyRoot);
    const bool unusual = sharedNotes (current, next) == 0 && from >= 0 && to >= 0
                         && data::majorKeyTimesSeen[from] >= minimumTimesSeenInData
                         && data::chanceOfNext (false, from, to) < 0.005f;
    result.title = (unusual ? "How " + next.name + " sounds after " : "Why " + next.name + " works after ") + current.name;
    return result;
}

} // namespace chords
