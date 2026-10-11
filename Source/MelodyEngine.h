#pragma once
#include "ChordEngine.h"
#include <random>
#include <numeric>

namespace melody
{
struct Scale
{
    int root = 0;
    bool minor = false;
    std::array<int, 7> notes() const
    {
        const std::array<int, 7> steps = minor ? std::array<int, 7>{0, 2, 3, 5, 7, 8, 10}
                                               : std::array<int, 7>{0, 2, 4, 5, 7, 9, 11};
        auto result = steps;
        for (auto& n : result) n = (n + root) % 12;
        return result;
    }
    bool contains (int midi) const
    {
        const auto pitches = notes();
        return std::find (pitches.begin(), pitches.end(), midi % 12) != pitches.end();
    }
    std::string name() const { return chords::noteName (root) + (minor ? " natural minor" : " major"); }
};
struct Estimate { Scale best, alternative; bool ambiguous = false; };
struct Note { int midi = 60; double beats = 1.0; double startBeat = 0.0; };
struct Phrase
{
    std::vector<Note> notes;
    double lengthBeats = 0.0;
    std::vector<chords::Chord> chords;
};

inline std::vector<chords::Chord> chordsForBars (const std::vector<chords::Chord>& history, int bars)
{
    std::vector<chords::Chord> result;
    if (history.empty()) return result;
    bars = std::clamp (bars, 1, 8);
    const size_t start = history.size() > size_t (bars) ? history.size() - size_t (bars) : 0;
    for (int i = 0; i < bars; ++i) result.push_back (history[start + size_t (i) % (history.size() - start)]);
    return result;
}

inline double snap (double beat, double grid) { return std::round (beat / grid) * grid; }

inline bool canPlace (const Phrase& phrase, const Note& note, int ignored = -1)
{
    if (note.midi < 60 || note.midi > 84 || ! std::isfinite (note.startBeat) || ! std::isfinite (note.beats)
        || note.startBeat < 0 || note.beats < 0.25 || note.startBeat + note.beats > phrase.lengthBeats + 0.000001)
        return false;
    for (size_t i = 0; i < phrase.notes.size(); ++i)
        if (int (i) != ignored && note.startBeat < phrase.notes[i].startBeat + phrase.notes[i].beats - 0.000001
            && note.startBeat + note.beats > phrase.notes[i].startBeat + 0.000001) return false;
    return true;
}

inline bool editNote (Phrase& phrase, int index, Note note)
{
    if (index < 0 || size_t (index) >= phrase.notes.size() || ! canPlace (phrase, note, index)) return false;
    phrase.notes[size_t (index)] = note;
    return true;
}

inline int addNote (Phrase& phrase, Note note)
{
    if (phrase.notes.size() >= 128 || ! canPlace (phrase, note)) return -1;
    phrase.notes.push_back (note);
    return int (phrase.notes.size()) - 1;
}

inline bool deleteNote (Phrase& phrase, int index)
{
    if (index < 0 || size_t (index) >= phrase.notes.size()) return false;
    phrase.notes.erase (phrase.notes.begin() + index);
    return true;
}

// Editor-lifetime history: only confirmed selections call confirm().
class Progression
{
public:
    explicit Progression (const chords::Chord& start) : history { start } {}
    void confirm (const chords::Chord& chord) { history.push_back (chord); }
    void reset (const chords::Chord& chord) { history = { chord }; }
    // Removes the most recent chord, always keeping the first. False if there was nothing to remove.
    bool removeLast()
    {
        if (history.size() <= 1) return false;
        history.pop_back();
        return true;
    }
    const std::vector<chords::Chord>& all() const { return history; }
    std::vector<chords::Chord> recent() const
    {
        return { history.end() - std::min (size_t (8), history.size()), history.end() };
    }
private:
    std::vector<chords::Chord> history;
};

inline Estimate estimate (const std::vector<chords::Chord>& progression)
{
    struct Candidate { Scale scale; int score; };
    std::vector<Candidate> candidates;
    for (int mode = 0; mode < 2; ++mode)
        for (int root = 0; root < 12; ++root)
        {
            Scale scale { root, mode == 1 };
            int score = 0;
            for (const auto& chord : progression)
            {
                for (int note : chord.notes) score += scale.contains (note) ? 10 : -20;
                if (chord.notes[0] == root && chords::chordType (chord) == mode)
                    score += 4;
            }
            if (! progression.empty())
            {
                for (const auto* chord : { &progression.front(), &progression.back() })
                    if (chord->notes[0] == root && chords::chordType (*chord) == mode) score += 2;
            }
            candidates.push_back ({ scale, score });
        }
    std::stable_sort (candidates.begin(), candidates.end(), [] (const auto& a, const auto& b)
                      { return a.score > b.score; });
    return { candidates[0].scale, candidates[1].scale, candidates[0].score - candidates[1].score <= 6 };
}

// One note or rest in a bar's rhythm. Position and length are counted in Timing steps from the start of the bar.
struct RhythmSlot { int start = 0; int length = 1; bool rest = false; };

// Builds the suggested melody.
//
// Rhythm: each bar is a mix of short notes, held notes and the odd rest, in whole Timing steps.
//         A note or rest always starts on beats one and three, and a bar never opens with a rest.
// Shape:  the bars follow a pattern. Bar one states an idea (A) and bar two answers it (B). Later
//         bars bring those ideas back with the same rhythm and the same ups and downs, moved to
//         fit that bar's chord. The last bar leads to one long final note: the scale's home note
//         when it belongs to the last chord, otherwise that chord's own root.
// Pitch:  notes stay in the scale, move mostly by small steps, and notes starting on beats one
//         and three belong to the bar's chord.
inline Phrase generate (const std::vector<chords::Chord>& progression, Scale scale, uint32_t seed,
                        int bars = 0, double grid = 1.0)
{
    Phrase phrase;
    if (progression.empty()) return phrase;
    phrase.chords = chordsForBars (progression, bars == 0 ? int (std::min (size_t (8), progression.size())) : bars);
    phrase.lengthBeats = double (phrase.chords.size()) * 4.0;
    grid = grid == 0.25 || grid == 0.5 ? grid : 1.0;
    std::mt19937 rng (seed);
    const int barCount = int (phrase.chords.size());
    const int stepsPerBar = int (4.0 / grid), half = stepsPerBar / 2;

    // Every note the melody may use: the scale's notes from C4 to C6, low to high.
    std::vector<int> allowed;
    for (int midi = 60; midi <= 84; ++midi) if (scale.contains (midi)) allowed.push_back (midi);

    // The allowed notes that belong to a chord. If none do, any allowed note will have to serve.
    const auto chordTones = [&allowed] (const chords::Chord& chord)
    {
        std::vector<int> tones;
        for (int midi : allowed)
            if (std::find (chord.notes.begin(), chord.notes.end(), midi % 12) != chord.notes.end()) tones.push_back (midi);
        return tones.empty() ? allowed : tones;
    };
    const auto nearest = [] (const std::vector<int>& choices, int target)
    {
        return *std::min_element (choices.begin(), choices.end(), [target] (int a, int b)
                                  { return std::abs (a - target) < std::abs (b - target); });
    };
    // Picks at random, favouring notes close to the previous one. Repeating the same note is unlikely.
    const auto pickNear = [&rng] (const std::vector<int>& choices, int previous)
    {
        std::vector<double> weights;
        for (int midi : choices)
        {
            const int distance = std::abs (midi - previous);
            weights.push_back (distance == 0 ? 0.12 : 1.0 / (1.0 + distance * distance * 0.5));
        }
        std::discrete_distribution<size_t> choose (weights.begin(), weights.end());
        return choices[choose (rng)];
    };

    // Adds a rhythm for the steps from `from` up to (not including) `to`.
    const auto addRhythm = [&] (std::vector<RhythmSlot>& rhythm, int from, int to)
    {
        // How likely a note is to last 1, 2, 3 or 4 steps. Faster timings lean towards longer notes.
        const std::array<double, 4> lengthWeights = grid == 1.0 ? std::array<double, 4> { 5, 3, 0, 0 }
                                                  : grid == 0.5 ? std::array<double, 4> { 4, 4, 1, 2 }
                                                                : std::array<double, 4> { 3, 5, 1, 4 };
        const int longestRest = int (1.0 / grid);   // a rest lasts one beat at most
        for (int position = from; position < to; )
        {
            auto weights = lengthWeights;
            for (int length = 2; length <= 4; ++length)
                if (position + length > to) weights[size_t (length - 1)] = 0;
            std::discrete_distribution<int> chooseLength (weights.begin(), weights.end());
            int length = 1 + chooseLength (rng);
            // A rest now and then, but never to open the bar and never two in a row.
            const bool rest = position != 0 && ! (! rhythm.empty() && rhythm.back().rest)
                              && std::uniform_real_distribution<double> (0.0, 1.0) (rng) < 0.12;
            if (rest) length = std::min (length, longestRest);
            rhythm.push_back ({ position, length, rest });
            position += length;
        }
    };

    int previous = 72;   // the melody starts out around C5
    const auto addNote = [&] (int bar, const RhythmSlot& slot, int midi)
    {
        phrase.notes.push_back ({ midi, slot.length * grid, double (bar) * 4.0 + slot.start * grid });
        previous = midi;
    };
    const auto isStrong = [half] (const RhythmSlot& slot) { return slot.start == 0 || slot.start == half; };

    // Fills a rhythm with freshly chosen notes and returns them (-1 marks a rest).
    const auto writeFresh = [&] (int bar, const std::vector<RhythmSlot>& rhythm)
    {
        const auto tones = chordTones (phrase.chords[size_t (bar)]);
        std::vector<int> pitches;
        for (const auto& slot : rhythm)
        {
            if (slot.rest) { pitches.push_back (-1); continue; }
            addNote (bar, slot, pickNear (isStrong (slot) ? tones : allowed, previous));
            pitches.push_back (previous);
        }
        return pitches;
    };

    // An idea is one bar of music that can come back later: its rhythm and the notes it used.
    struct Idea { std::vector<RhythmSlot> rhythm; std::vector<int> pitches; bool written = false; };

    // Brings an idea back in another bar: same rhythm, same ups and downs, moved up or down the
    // scale so that it starts on a note of this bar's chord.
    const auto writeRepeat = [&] (int bar, const Idea& idea)
    {
        const auto tones = chordTones (phrase.chords[size_t (bar)]);
        const auto indexOf = [&allowed] (int midi)
        { return int (std::find (allowed.begin(), allowed.end(), midi) - allowed.begin()); };
        int shift = 0;
        bool first = true;
        for (size_t i = 0; i < idea.rhythm.size(); ++i)
        {
            if (idea.rhythm[i].rest) continue;
            const int source = idea.pitches[i];
            if (first) shift = indexOf (nearest (tones, source)) - indexOf (source);
            first = false;
            int midi = allowed[size_t (std::clamp (indexOf (source) + shift, 0, int (allowed.size()) - 1))];
            if (isStrong (idea.rhythm[i])) midi = nearest (tones, midi);
            addNote (bar, idea.rhythm[i], midi);
        }
    };

    Idea ideaA, ideaB;
    for (int bar = 0; bar < barCount; ++bar)
    {
        if (bar == barCount - 1)
        {
            // The ending: half a bar that echoes how the melody began, then one long final note.
            std::vector<RhythmSlot> rhythm;
            if (ideaA.written)
                for (const auto& slot : ideaA.rhythm) { if (slot.start < half) rhythm.push_back (slot); }
            else
                addRhythm (rhythm, 0, half);
            writeFresh (bar, rhythm);

            const auto& chord = phrase.chords[size_t (bar)];
            const bool homeFits = std::find (chord.notes.begin(), chord.notes.end(), scale.root) != chord.notes.end();
            const int wanted = homeFits ? scale.root : chord.notes[0];
            std::vector<int> endings;
            for (int midi : allowed) if (midi % 12 == wanted) endings.push_back (midi);
            if (endings.empty()) endings = chordTones (chord);
            addNote (bar, { half, half, false }, nearest (endings, previous));
            break;
        }

        Idea& idea = bar % 2 == 0 ? ideaA : ideaB;
        if (idea.written) { writeRepeat (bar, idea); continue; }
        addRhythm (idea.rhythm, 0, half);
        addRhythm (idea.rhythm, half, stepsPerBar);
        idea.pitches = writeFresh (bar, idea.rhythm);
        idea.written = true;
    }
    return phrase;
}
}
