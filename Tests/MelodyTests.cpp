#include "PreviewPlayback.h"
#include <iostream>
#include <cstdlib>
#include <thread>

void check (bool condition, const char* message)
{
    if (! condition) { std::cerr << message << '\n'; std::exit (1); }
}
std::vector<float> render (PreviewPlayback& player, int count)
{
    std::vector<float> samples;
    for (int i = 0; i < count; ++i)
    {
        if (i % 128 == 0) player.beginBlock();
        samples.push_back (player.nextSample());
    }
    return samples;
}
int main()
{
    const auto chords = chords::chordsInMajorKey (0);
    const std::vector<chords::Chord> progression { chords[0], chords[5], chords[3], chords[4] };
    melody::Progression history (chords[0]);
    PreviewPlayback player;
    player.chord (chords[5]); // Preview does not record a confirmation.
    check (history.all().size() == 1, "Preview must not change history");
    for (int i = 0; i < 10; ++i) history.confirm (chords[i % 7]);
    check (history.all().size() == 11 && history.recent().size() == 8, "History and recent limit");
    check (history.recent().front().name == history.all()[3].name, "Use most recent chords");
    history.reset (chords[5]);
    check (history.all().size() == 1 && history.all()[0].name == "Am", "Reset to current chord");
    check (! history.removeLast() && history.all().size() == 1, "Undo keeps the first chord");
    history.confirm (chords[4]);
    check (history.removeLast() && history.all().size() == 1 && history.all().back().name == "Am", "Undo removes the latest chord");
    const auto estimated = melody::estimate (progression);
    check (estimated.best.root == 0 && ! estimated.best.minor, "C Am F G estimates C major");
    const auto minor = melody::estimate ({ chords[5], chords[1], chords[2], chords[5] });
    check (minor.best.root == 9 && minor.best.minor, "Tonic evidence supports A minor");
    check (melody::estimate ({ chords[1], chords[2] }).ambiguous, "Ambiguous progression shows alternative");
    const auto phrase = melody::generate (progression, estimated.best, 42);
    const auto repeat = melody::generate (progression, estimated.best, 42);
    check (phrase.notes.size() == 16, "Four notes per chord");
    for (size_t i = 0; i < phrase.notes.size(); ++i)
    {
        const auto note = phrase.notes[i];
        check (note.midi >= 60 && note.midi <= 84 && estimated.best.contains (note.midi), "Scale and register");
        check (note.beats == 1 && note.midi == repeat.notes[i].midi, "Quarter notes and deterministic seed");
        if (i % 2 == 0)
            check (std::find (progression[i / 4].notes.begin(), progression[i / 4].notes.end(), note.midi % 12)
                   != progression[i / 4].notes.end(), "Strong beats prefer chord tones");
    }
    bool varied = false;
    const auto variation = melody::generate (progression, estimated.best, 43);
    for (size_t i = 0; i < phrase.notes.size(); ++i) varied |= variation.notes[i].midi != phrase.notes[i].midi;
    check (varied, "Different seeds produce variations");
    const auto overridePhrase = melody::generate (progression, { 1, true }, 42);
    for (auto note : overridePhrase.notes) check (melody::Scale {1, true}.contains (note.midi), "Override respects scale");
    check (! melody::Scale {1, true}.contains (0), "Out-of-scale chord note detectable");
    std::vector<chords::Chord> longHistory (12, chords[0]);
    check (melody::generate (longHistory, {0, false}, 42).notes.size() == 32, "Generation bounded to 8 bars");
    check (melody::generate ({}, {0, false}, 42).notes.empty(), "Empty progression");

    const auto repeated = melody::chordsForBars ({ chords[0], chords[5] }, 4);
    check (repeated.size() == 4 && repeated[2].name == "C" && repeated[3].name == "Am", "Short progression repeats");
    const auto latestBars = melody::chordsForBars (progression, 2);
    check (latestBars[0].name == "F" && latestBars[1].name == "G", "Long history uses latest bars");
    for (int bars : { 1, 2, 4, 8 })
        for (double grid : { 1.0, 0.5, 0.25 })
        {
            const auto generated = melody::generate (progression, estimated.best, 42, bars, grid);
            check (generated.lengthBeats == bars * 4 && generated.notes.size() == size_t (bars * 4 / grid), "Bars and subdivision");
            for (size_t i = 0; i < generated.notes.size(); ++i)
            {
                check (generated.notes[i].startBeat == i * grid && generated.notes[i].beats == grid, "Explicit note timing");
                check (melody::canPlace (generated, generated.notes[i], int (i)), "Generated phrase is monophonic");
            }
        }
    melody::Phrase edited;
    edited.lengthBeats = 4;
    check (melody::addNote (edited, {61, 0.5, 0}) == 0, "Out-of-scale pitch permitted");
    check (melody::addNote (edited, {64, 1, 2}) == 1, "Rest between notes permitted");
    check (! melody::editNote (edited, 0, {61, 3, 0}), "Overlapping resize rejected");
    check (melody::editNote (edited, 0, {65, 1, 0.5}), "Pitch, start, and duration editable");
    check (melody::addNote (edited, {60, 1, 1}) == -1, "Overlapping addition rejected");
    check (! melody::editNote (edited, 0, {85, 1, 0.5}) && ! melody::editNote (edited, 0, {60, 1, 3.5}), "Register and phrase bounds");
    check (melody::snap (1.3, 0.5) == 1.5 && melody::deleteNote (edited, 0), "Snap and delete");

    for (const int rate : { 44100, 48000, 96000 })
    {
        player.reset (rate);
        player.phrase (phrase);
        const auto samples = render (player, rate * 8 + 128);
        double energy = 0;
        for (int i = 0; i < rate * 8; ++i)
        {
            check (std::isfinite (samples[i]) && std::abs (samples[i]) <= 0.061f, "Single melody voice only");
            energy += samples[i] * samples[i];
        }
        check (energy > 1, "Melody audible");
        for (int i = rate * 8; i < int (samples.size()); ++i) check (samples[i] == 0, "Phrase stops on time");
        player.phrase (phrase);
        render (player, rate / 4);
        player.beginBlock();
        check (std::abs (player.playheadBeat() - 0.5) < 0.002, "Playhead follows audio sample time");
        player.stop();
        render (player, rate / 100 + 128);
        check (player.nextSample() == 0, "Stop fades to silence");
        player.phrase (phrase);
        render (player, rate / 4);
        player.chord (chords[0]);
        player.beginBlock();
        render (player, rate / 100);
        PreviewPlayback reference;
        reference.reset (rate);
        reference.chord (chords[0]);
        check (render (player, rate + 128) == render (reference, rate + 128), "Chord replaces phrase");
        player.chord (chords[0], &chords[5]);
        render (player, rate / 4);
        player.phrase (phrase);
        player.beginBlock();
        render (player, rate / 100);
        reference.reset (rate);
        reference.phrase (phrase);
        check (render (player, rate * 8 + 128) == render (reference, rate * 8 + 128), "Phrase cancels chord transition");
        player.phrase (phrase);
        player.reset (rate);
        for (float sample : render (player, 1024)) check (sample == 0, "Reset discards queued playback");

        melody::Phrase rests;
        rests.lengthBeats = 4;
        rests.chords = { chords[0] };
        rests.notes = { {72, 0.5, 1.0}, {74, 0.5, 3.0} };
        player.phrase (rests);
        const auto restSamples = render (player, rate * 2);
        for (int i = 0; i < rate / 2; ++i) check (restSamples[i] == 0, "Leading rest silent");
        for (int i = rate * 3 / 4; i < rate * 3 / 2; ++i) check (restSamples[i] == 0, "Middle rest silent");
        for (int i = rate * 7 / 4; i < rate * 2; ++i) check (restSamples[i] == 0, "Trailing rest silent");
        check (std::abs (restSamples[rate / 2 + 100]) > 0, "Timed note sounds after rest");
        player.phrase (rests, false, true);
        const auto looping = render (player, rate * 6);
        for (int i = 0; i < rate * 2; ++i)
            check (looping[i] == looping[rate * 2 + i] && looping[i] == looping[rate * 4 + i], "Loop repeats complete phrase including rests");
        player.beginBlock();
        check (player.playheadBeat() >= 0 && player.playheadBeat() < 4, "Loop playhead wraps");
        player.stop();
        render (player, rate / 100 + 128);
        check (player.playheadBeat() < 0 && player.nextSample() == 0, "Stop cancels loop and playhead");
        player.phrase (rests, true, false);
        const auto backed = render (player, rate * 2);
        const int testSample = rate / 4;
        double expectedChord = 0;
        for (int midi : { 48, 52, 55 }) expectedChord += 0.025 * std::sin (6.2831853071795864769 * 440
                                                         * std::pow (2.0, (midi - 69) / 12.0) * testSample / rate);
        check (std::abs (backed[testSample] - expectedChord) < 0.00001, "Quiet backing sounds during melody rest");
        check (backed.front() == 0 && backed.back() == 0, "Backing boundaries fade to zero");
        player.reset (rate);
        reference.reset (rate);
        player.phrase (phrase, false, true);
        reference.phrase (phrase, false, true);
        check (render (player, rate / 4) == render (reference, rate / 4), "Backing starts disabled");
        player.setMelodyBackingEnabled (true);
        const auto enabledSamples = render (player, rate / 4);
        const auto melodyOnly = render (reference, rate / 4);
        check (enabledSamples != melodyOnly, "Live backing becomes audible");
        player.beginBlock();
        check (std::abs (player.playheadBeat() - 1.0) < 0.002, "Live toggle does not restart melody");
        player.setMelodyBackingEnabled (false);
        render (player, rate / 100 + 1);
        render (reference, rate / 100 + 1);
        check (render (player, rate / 4) == render (reference, rate / 4), "Backing fades out leaving identical uninterrupted melody");
        player.reset (rate);
        reference.reset (rate);
        player.phrase (rests, false, true);
        reference.phrase (rests, true, true);
        render (player, rate / 4);
        render (reference, rate / 4);
        player.setMelodyBackingEnabled (true);
        render (player, rate / 100 + 1);
        render (reference, rate / 100 + 1);
        check (render (player, rate * 4) == render (reference, rate * 4), "Mid-bar enable aligns backing and preserves it through loop boundaries");
        player.reset (rate);
        melody::Phrase backingOnly;
        backingOnly.lengthBeats = 8;
        backingOnly.chords = { chords[0], chords[5] };
        player.phrase (backingOnly, true, true);
        const auto chordLoop = render (player, rate * 8);
        const int amSample = rate / 4;
        double expectedAm = 0;
        for (int midi : {57, 60, 64}) expectedAm += 0.025 * std::sin (6.2831853071795864769 * 440
                                                   * std::pow (2.0, (midi - 69) / 12.0) * amSample / rate);
        check (std::abs (chordLoop[rate * 2 + amSample] - expectedAm) < 0.00001, "Backing changes to next chord at bar boundary");
        for (int i = 0; i < rate * 4; ++i) check (chordLoop[i] == chordLoop[rate * 4 + i], "Chord backing repeats with loop");
        player.stop();
        render (player, rate / 100 + 128);
        player.phrase (melody::generate (progression, estimated.best, 42, 8, 0.25));
        const auto dense = render (player, rate * 16 + 128);
        check (dense[rate * 15 + 100] != 0 && dense[rate * 16] == 0, "All 128 sixteenth notes play without truncation");
    }
    // Exercise ownership exchanges while producer and consumer run concurrently.
    player.reset (48000);
    std::thread producer ([&]
    {
        for (int i = 0; i < 10000; ++i)
        {
            if (i % 3 == 0) player.phrase (phrase);
            else if (i % 3 == 1) player.chord (chords[0], &chords[5]);
            else player.stop();
        }
    });
    for (int block = 0; block < 10000; ++block)
    {
        player.beginBlock();
        for (int i = 0; i < 32; ++i)
            check (std::isfinite (player.nextSample()), "Concurrent requests remain finite");
    }
    producer.join();
    std::cout << "Melody, history, and playback tests passed\n";
}
