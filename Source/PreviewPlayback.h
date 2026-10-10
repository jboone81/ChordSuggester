#pragma once
#include "MelodyEngine.h"
#include <atomic>
#include <cmath>

// One UI producer and one audio consumer exchange complete requests using three
// fixed slots. Neither thread ever writes the slot owned by the other thread.
class PreviewPlayback
{
    struct Event { int midi = 60; double start = 0, duration = 0.5; };
    struct Request
    {
        std::array<Event, 128> notes {};
        std::array<std::array<int, 3>, 8> chords {};
        int noteCount = 0, chordCount = 0;
        double seconds = 0, chordSeconds = 2.0;
        bool loop = false, melody = false;
    };
public:
    void chord (const chords::Chord& first, const chords::Chord* second = nullptr)
    {
        Request request;
        request.chordCount = second ? 2 : 1;
        request.chordSeconds = 1.0;
        request.seconds = request.chordCount;
        for (int i = 0; i < request.chordCount; ++i)
            request.chords[i] = voicing ((i == 0 ? first : *second).notes, 60);
        publish (request);
    }
    void audition (int midi)
    {
        Request request;
        request.noteCount = 1;
        request.notes[0] = { std::clamp (midi, 60, 84), 0, 0.35 };
        request.seconds = 0.35;
        publish (request);
    }
    void phrase (const melody::Phrase& phrase, bool withChords = false, bool loop = false)
    {
        Request request;
        request.melody = true;
        request.loop = loop;
        request.seconds = std::clamp (phrase.lengthBeats, 0.0, 32.0) * 0.5;
        for (const auto& note : phrase.notes)
        {
            if (request.noteCount == 128) break;
            if (! std::isfinite (note.startBeat) || ! std::isfinite (note.beats) || note.startBeat < 0
                || note.beats <= 0 || note.startBeat + note.beats > phrase.lengthBeats + 0.000001) continue;
            request.notes[request.noteCount++] = { std::clamp (note.midi, 60, 84), note.startBeat * 0.5, note.beats * 0.5 };
        }
        std::sort (request.notes.begin(), request.notes.begin() + request.noteCount,
                   [] (const auto& a, const auto& b) { return a.start < b.start; });
        request.chordCount = int (std::min (size_t (8), phrase.chords.size()));
        for (int i = 0; i < request.chordCount; ++i) request.chords[i] = voicing (phrase.chords[i].notes, 48);
        setMelodyBackingEnabled (withChords);
        publish (request);
    }
    void setMelodyBackingEnabled (bool enabled)
    {
        backingEnabled.store (enabled ? 1 : 0, std::memory_order_release);
    }
    void stop() { publish (Request {}); }
    void reset (double rate)
    {
        sampleRate = rate > 0 ? rate : 44100;
        consume();
        active.seconds = pending.seconds = 0;
        hasPending = false;
        replacementLeft = 0;
        fade = std::max (1, int (sampleRate * 0.01));
        visiblePosition.store (-1, std::memory_order_release);
    }
    // Called once per audio block. Also publishes the preceding block's playhead.
    void beginBlock()
    {
        backingTarget = backingEnabled.load (std::memory_order_acquire) != 0 ? 1.0 : 0.0;
        if (consume())
        {
            pending = slots[front];
            hasPending = true;
            if (active.seconds == 0) startPending();
            else if (replacementLeft == 0) replacementLeft = fade;
        }
        visiblePosition.store (active.seconds > 0 && active.melody ? int (position * 2000 / sampleRate) : -1,
                               std::memory_order_release);
    }
    double playheadBeat() const
    {
        const int positionMilliBeats = visiblePosition.load (std::memory_order_acquire);
        return positionMilliBeats < 0 ? -1.0 : positionMilliBeats / 1000.0;
    }
    float nextSample()
    {
        if (active.seconds <= 0) return 0;
        if (active.melody)
            backingGain += std::clamp (backingTarget - backingGain, -1.0 / fade, 1.0 / fade);
        double tone = 0;
        while (noteIndex < active.noteCount && position >= samples (active.notes[noteIndex].start + active.notes[noteIndex].duration))
        {
            ++noteIndex;
            noteStarted = false;
        }
        if (noteIndex < active.noteCount)
        {
            const auto& note = active.notes[noteIndex];
            const auto start = samples (note.start);
            if (position >= start)
            {
                if (! noteStarted)
                {
                    notePhase = 0;
                    noteIncrement = increment (note.midi);
                    noteStarted = true;
                }
                tone += oscillator (notePhase, noteIncrement) * envelope (position - start, samples (note.duration)) * 0.06;
            }
        }
        const int chordIndex = int (position / std::max (int64_t (1), samples (active.chordSeconds)));
        if (chordIndex < active.chordCount)
        {
            if (chordIndex != soundingChord)
            {
                soundingChord = chordIndex;
                chordPhases.fill (0);
                for (int i = 0; i < 3; ++i) chordIncrements[i] = increment (active.chords[chordIndex][i]);
            }
            double chord = 0;
            for (int i = 0; i < 3; ++i) chord += oscillator (chordPhases[i], chordIncrements[i]);
            tone += chord * envelope (position - samples (chordIndex * active.chordSeconds), samples (active.chordSeconds))
                * (active.melody ? 0.025 * backingGain : 0.06);
        }
        const float output = float (tone * (replacementLeft > 0 ? double (replacementLeft) / fade : 1.0));
        ++position;
        if (replacementLeft > 0 && --replacementLeft == 0) startPending();
        else if (position >= samples (active.seconds))
        {
            if (hasPending) startPending();
            else if (active.loop) startTimeline();
            else { active.seconds = 0; visiblePosition.store (-1, std::memory_order_release); }
        }
        return output;
    }
private:
    static std::array<int, 3> voicing (const std::array<int, 3>& notes, int octave)
    {
        std::array<int, 3> result {};
        int previous = -1;
        for (int i = 0; i < 3; ++i)
        {
            int midi = octave + notes[i];
            while (midi <= previous) midi += 12;
            result[i] = previous = midi;
        }
        return result;
    }
    int64_t samples (double seconds) const { return int64_t (std::llround (seconds * sampleRate)); }
    double increment (int midi) const { return twoPi * 440.0 * std::pow (2.0, (midi - 69) / 12.0) / sampleRate; }
    static double oscillator (double& phase, double increment)
    {
        const double value = std::sin (phase);
        phase += increment;
        if (phase >= twoPi) phase -= twoPi;
        return value;
    }
    double envelope (int64_t local, int64_t duration) const
    {
        return std::max (0.0, std::min (1.0, std::min (double (local) / fade, double (duration - 1 - local) / fade)));
    }
    void publish (const Request& request)
    {
        slots[back] = request;
        back = middle.exchange (back | 4u, std::memory_order_acq_rel) & 3u;
    }
    bool consume()
    {
        if ((middle.load (std::memory_order_acquire) & 4u) == 0) return false;
        front = middle.exchange (front, std::memory_order_acq_rel) & 3u;
        return true;
    }
    void startPending()
    {
        active = pending;
        backingGain = backingTarget;
        hasPending = false;
        replacementLeft = 0;
        startTimeline();
        if (! active.melody || active.seconds == 0) visiblePosition.store (-1, std::memory_order_release);
    }
    void startTimeline()
    {
        position = 0;
        noteIndex = 0;
        noteStarted = false;
        soundingChord = -1;
    }
    static constexpr double twoPi = 6.2831853071795864769;
    static_assert (std::atomic<unsigned>::is_always_lock_free && std::atomic<int>::is_always_lock_free, "Playback must be lock-free");
    std::array<Request, 3> slots {};
    std::atomic<unsigned> middle { 1 };
    std::atomic<int> visiblePosition { -1 };
    std::atomic<int> backingEnabled { 0 };
    unsigned front = 0, back = 2;
    Request active {}, pending {};
    bool hasPending = false, noteStarted = false;
    double sampleRate = 44100, notePhase = 0, noteIncrement = 0;
    double backingTarget = 0, backingGain = 0;
    int fade = 441, noteIndex = 0, soundingChord = -1, replacementLeft = 0;
    int64_t position = 0;
    std::array<double, 3> chordPhases {}, chordIncrements {};
};
