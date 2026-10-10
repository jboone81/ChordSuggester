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

inline Phrase generate (const std::vector<chords::Chord>& progression, Scale scale, uint32_t seed,
                        int bars = 0, double grid = 1.0)
{
    Phrase phrase;
    if (progression.empty()) return phrase;
    phrase.chords = chordsForBars (progression, bars == 0 ? int (std::min (size_t (8), progression.size())) : bars);
    phrase.lengthBeats = double (phrase.chords.size()) * 4.0;
    grid = grid == 0.25 || grid == 0.5 ? grid : 1.0;
    std::mt19937 rng (seed);
    int previous = 72;
    for (size_t bar = 0; bar < phrase.chords.size(); ++bar)
        for (int step = 0; step < int (4.0 / grid); ++step)
        {
            const double beat = step * grid;
            const bool strong = beat == 0.0 || beat == 2.0;
            std::vector<int> candidates;
            for (int midi = 60; midi <= 84; ++midi)
            {
                const bool chordTone = std::find (phrase.chords[bar].notes.begin(), phrase.chords[bar].notes.end(),
                                                  midi % 12) != phrase.chords[bar].notes.end();
                if (scale.contains (midi) && (! strong || chordTone)) candidates.push_back (midi);
            }
            if (candidates.empty())
                for (int midi = 60; midi <= 84; ++midi) if (scale.contains (midi)) candidates.push_back (midi);
            // Nearer notes receive greater probability; strong beats prefer chord tones.
            std::vector<double> weights;
            for (int midi : candidates)
            {
                const int distance = std::abs (midi - previous);
                weights.push_back (distance == 0 ? 0.12 : 1.0 / (1.0 + distance * distance * 0.5));
            }
            std::discrete_distribution<size_t> choose (weights.begin(), weights.end());
            previous = candidates[choose (rng)];
            phrase.notes.push_back ({ previous, grid, double (bar) * 4.0 + beat });
        }
    return phrase;
}
}
