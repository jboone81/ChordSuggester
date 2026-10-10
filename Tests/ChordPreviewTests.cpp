#include "ChordPreview.h"
#include <cstdlib>
#include <iostream>
#include <vector>

static void require (bool condition, const char* message)
{
    if (! condition)
    {
        std::cerr << message << '\n';
        std::exit (1);
    }
}

static std::vector<float> render (ChordPreview& preview, int count)
{
    std::vector<float> result;
    for (int i = 0; i < count; ++i)
    {
        if (i % 128 == 0)
            preview.beginBlock();
        result.push_back (preview.nextSample());
    }
    return result;
}

int main()
{
    const std::array<int, 3> c { 0, 4, 7 }, am { 9, 0, 4 }, bdim { 11, 2, 5 };
    for (const int rate : { 44100, 48000, 96000 })
    {
        ChordPreview preview;
        preview.reset (rate);
        require (preview.nextSample() == 0.0f, "Initialization must be silent");
        for (const auto notes : { c, am, bdim })
        {
            preview.reset (rate);
            preview.request (notes);
            const auto samples = render (preview, rate + 128);
            double energy = 0.0;
            int previous = -1;
            std::array<double, 3> frequencies {};
            for (int i = 0; i < 3; ++i)
            {
                int midi = 60 + notes[i];
                while (midi <= previous)
                    midi += 12;
                previous = midi;
                frequencies[i] = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
            }
            for (int i = 0; i < rate; ++i)
            {
                require (std::isfinite (samples[i]) && std::abs (samples[i]) <= 0.181f,
                         "Preview must have bounded finite output");
                energy += samples[i] * samples[i];
                if (i >= rate / 50 && i < rate - rate / 50)
                {
                    double expected = 0.0;
                    for (const double frequency : frequencies)
                        expected += 0.06 * std::sin (6.2831853071795864769 * frequency * i / rate);
                    require (std::abs (samples[i] - expected) < 0.00001,
                             "All three correctly voiced notes must sound simultaneously");
                }
            }
            require (energy > 1.0, "Preview must be audible");
            require (samples.front() == 0.0f && samples[rate - 1] == 0.0f,
                     "Envelope must start and end at zero");
            for (int i = rate; i < int (samples.size()); ++i)
                require (samples[i] == 0.0f, "Preview must stop after one second");
        }

        ChordPreview first, second;
        first.reset (rate);
        second.reset (rate);
        first.request (c);
        second.request (am);
        const auto expectedFirst = render (first, rate);
        const auto expectedSecond = render (second, rate);
        preview.reset (rate);
        preview.request (c, &am);
        const auto sequence = render (preview, 2 * rate + 128);
        for (int i = 0; i < rate; ++i)
        {
            require (sequence[i] == expectedFirst[i], "Sequence must start with current chord");
            require (sequence[rate + i] == expectedSecond[i], "Next chord must follow without a gap");
        }
        for (int i = 2 * rate; i < int (sequence.size()); ++i)
            require (sequence[i] == 0.0f, "Sequence must stop after two seconds");

        preview.reset (rate);
        preview.request (c, &am);
        render (preview, rate / 2);
        preview.request (am);
        preview.request (bdim); // Latest complete request wins.
        preview.beginBlock();
        float previousSample = preview.nextSample();
        for (int i = 1; i < rate / 100; ++i)
        {
            const float sample = preview.nextSample();
            require (std::abs (sample - previousSample) < 0.04f, "Replacement must fade smoothly");
            previousSample = sample;
        }
        first.reset (rate);
        first.request (bdim);
        const auto replacement = render (preview, rate + 128);
        const auto latest = render (first, rate + 128);
        require (replacement == latest, "Replacement must cancel queued chord and use latest request");

        preview.request (c, &am);
        preview.reset (rate);
        const auto silence = render (preview, 2 * rate);
        for (const float sample : silence)
            require (sample == 0.0f, "Restart must clear active and pending playback");
    }
    std::cout << "Chord preview tests passed at 44.1, 48, and 96 kHz\n";
}
