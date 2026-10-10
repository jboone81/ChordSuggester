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
// True if this pitch belongs to the chord.
bool inChord (const chords::Chord& chord, int midi)
{
    return std::find (chord.notes.begin(), chord.notes.end(), midi % 12) != chord.notes.end();
}
// The rhythm of one bar as text, e.g. "0+1 1+0.5 " (start within the bar + length), so two bars can be compared.
std::string barRhythm (const melody::Phrase& phrase, int bar)
{
    std::string text;
    for (const auto& note : phrase.notes)
        if (note.startBeat >= bar * 4.0 && note.startBeat < bar * 4.0 + 4.0)
            text += std::to_string (note.startBeat - bar * 4.0) + "+" + std::to_string (note.beats) + " ";
    return text;
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
    check (! phrase.notes.empty() && phrase.notes.size() == repeat.notes.size(), "Same seed gives the same number of notes");
    for (size_t i = 0; i < phrase.notes.size(); ++i)
    {
        const auto note = phrase.notes[i];
        check (note.midi >= 60 && note.midi <= 84 && estimated.best.contains (note.midi), "Scale and register");
        check (note.midi == repeat.notes[i].midi && note.startBeat == repeat.notes[i].startBeat
               && note.beats == repeat.notes[i].beats, "Deterministic seed");
        check (note.beats >= 1 && note.beats == std::floor (note.beats), "Quarter timing gives whole-beat notes");
        if (std::fmod (note.startBeat, 2.0) == 0)
            check (inChord (progression[size_t (note.startBeat / 4)], note.midi), "Strong beats prefer chord tones");
    }
    bool varied = false;
    for (uint32_t seed = 43; seed < 48 && ! varied; ++seed)
    {
        const auto variation = melody::generate (progression, estimated.best, seed);
        varied = variation.notes.size() != phrase.notes.size();
        for (size_t i = 0; i < phrase.notes.size() && ! varied; ++i) varied = variation.notes[i].midi != phrase.notes[i].midi;
    }
    check (varied, "Different seeds produce variations");
    const auto overridePhrase = melody::generate (progression, { 1, true }, 42);
    for (auto note : overridePhrase.notes) check (melody::Scale {1, true}.contains (note.midi), "Override respects scale");
    check (! melody::Scale {1, true}.contains (0), "Out-of-scale chord note detectable");
    std::vector<chords::Chord> longHistory (12, chords[0]);
    const auto bounded = melody::generate (longHistory, {0, false}, 42);
    check (bounded.chords.size() == 8 && bounded.lengthBeats == 32, "Generation bounded to 8 bars");
    check (melody::generate ({}, {0, false}, 42).notes.empty(), "Empty progression");

    const auto repeated = melody::chordsForBars ({ chords[0], chords[5] }, 4);
    check (repeated.size() == 4 && repeated[2].name == "C" && repeated[3].name == "Am", "Short progression repeats");
    const auto latestBars = melody::chordsForBars (progression, 2);
    check (latestBars[0].name == "F" && latestBars[1].name == "G", "Long history uses latest bars");
    for (int bars : { 1, 2, 4, 8 })
        for (double grid : { 1.0, 0.5, 0.25 })
        {
            const auto generated = melody::generate (progression, estimated.best, 42, bars, grid);
            check (generated.lengthBeats == bars * 4 && ! generated.notes.empty()
                   && generated.notes.size() <= size_t (bars * 4 / grid), "Bars and subdivision");
            for (size_t i = 0; i < generated.notes.size(); ++i)
            {
                const auto& note = generated.notes[i];
                check (note.beats >= grid && std::fmod (note.startBeat, grid) == 0 && std::fmod (note.beats, grid) == 0,
                       "Notes start and end on the Timing grid");
                check (i == 0 || note.startBeat >= generated.notes[i - 1].startBeat + generated.notes[i - 1].beats, "Notes in time order");
                check (melody::canPlace (generated, note, int (i)), "Generated phrase is monophonic");
            }
        }

    // Rhythm and shape of generated melodies, over many seeds.
    const std::vector<chords::Chord> endsAtHome { chords[0], chords[5], chords[3], chords[0] };   // C Am F C
    bool heldNote = false, rest = false;
    for (uint32_t seed = 1; seed <= 40; ++seed)
        for (int bars : { 1, 2, 3, 4, 8 })
            for (double grid : { 1.0, 0.5, 0.25 })
                for (const auto& chordsUsed : { progression, endsAtHome })
                {
                    const auto generated = melody::generate (chordsUsed, estimated.best, seed, bars, grid);
                    double expectedStart = 0;
                    for (const auto& note : generated.notes)
                    {
                        const auto& chord = generated.chords[size_t (note.startBeat / 4)];
                        check (estimated.best.contains (note.midi) && note.midi >= 60 && note.midi <= 84, "Generated notes stay in scale and register");
                        if (std::fmod (note.startBeat, 2.0) == 0) check (inChord (chord, note.midi), "Beats one and three use chord notes");
                        check (std::floor (note.startBeat / 2) == std::floor ((note.startBeat + note.beats - 0.001) / 2), "Notes do not cross beats one and three");
                        heldNote |= note.beats > grid;
                        rest |= note.startBeat > expectedStart;
                        check (note.startBeat - expectedStart <= 1.0, "A rest lasts one beat at most");
                        expectedStart = note.startBeat + note.beats;
                    }
                    for (int bar = 0; bar < bars; ++bar)
                    {
                        bool startsOnBeatOne = false;
                        for (const auto& note : generated.notes) startsOnBeatOne |= note.startBeat == bar * 4.0;
                        check (startsOnBeatOne, "Every bar opens with a note");
                    }
                    // The ending: one long note that finishes the phrase on a settled pitch.
                    const auto& last = generated.notes.back();
                    const auto& lastChord = generated.chords.back();
                    check (last.beats == 2 && last.startBeat + last.beats == generated.lengthBeats, "Phrase ends on a held note");
                    check (last.midi % 12 == (inChord (lastChord, estimated.best.root) ? estimated.best.root : lastChord.notes[0]),
                           "Ends on the home note when it fits the last chord, otherwise on that chord's root");
                    // Repetition: the opening idea returns in bar three, and the answer in bar four of a longer phrase.
                    if (bars >= 4) check (barRhythm (generated, 0) == barRhythm (generated, 2), "Bar three repeats bar one's rhythm");
                    if (bars == 8) check (barRhythm (generated, 1) == barRhythm (generated, 3), "Bar four repeats bar two's rhythm");
                }
    check (heldNote && rest, "Melodies include held notes and rests");
    const auto home = melody::generate (endsAtHome, estimated.best, 42, 4, 0.5);
    check (home.notes.back().midi % 12 == 0, "C Am F C ends on C");
    check (phrase.notes.back().midi % 12 == 7, "C Am F G ends on G");
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
        melody::Phrase sixteenths;   // the busiest phrase the piano roll allows: 8 bars of sixteenth notes
        sixteenths.lengthBeats = 32;
        sixteenths.chords = melody::chordsForBars (progression, 8);
        for (int i = 0; i < 128; ++i) sixteenths.notes.push_back ({ 60 + i % 12, 0.25, i * 0.25 });
        player.phrase (sixteenths);
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
