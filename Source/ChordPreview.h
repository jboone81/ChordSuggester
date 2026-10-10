#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <algorithm>

// A latest-request mailbox: all six pitch classes travel in one atomic value.
// Only the audio thread owns oscillators and timing; no locks or allocation there.
class ChordPreview
{
public:
    void request (const std::array<int, 3>& first, const std::array<int, 3>* second = nullptr)
    {
        uint32_t packed = second != nullptr ? (2u << 24) : (1u << 24);
        for (int i = 0; i < 3; ++i)
        {
            packed |= uint32_t ((first[i] % 12 + 12) % 12) << (4 * i);
            if (second != nullptr)
                packed |= uint32_t (((*second)[i] % 12 + 12) % 12) << (12 + 4 * i);
        }
        mailbox.store (packed, std::memory_order_release);
    }

    void reset (double rate)
    {
        sampleRate = rate > 0.0 ? rate : 44100.0;
        duration = std::max (1, int (sampleRate));
        fade = std::max (1, int (sampleRate * 0.01));
        mailbox.store (0, std::memory_order_release);
        active = pending = 0;
        position = replacementLeft = 0;
        phases.fill (0.0);
    }

    void beginBlock()
    {
        const auto request = mailbox.exchange (0, std::memory_order_acq_rel);
        if (request == 0)
            return;
        pending = request;
        if (active == 0)
            startPending();
        else if (replacementLeft == 0)
            replacementLeft = fade;
    }

    float nextSample()
    {
        if (active == 0)
            return 0.0f;
        const double envelope = std::min (1.0, std::min (double (position) / fade,
                                                        double (duration - 1 - position) / fade));
        double value = 0.0;
        for (int i = 0; i < 3; ++i)
        {
            value += std::sin (phases[i]);
            phases[i] += increments[i];
            if (phases[i] >= twoPi)
                phases[i] -= twoPi;
        }
        const double replacementGain = replacementLeft > 0 ? double (replacementLeft) / fade : 1.0;
        const float output = float (value * envelope * replacementGain * 0.06);
        ++position;
        if (replacementLeft > 0 && --replacementLeft == 0)
            startPending();
        else if (position >= duration)
        {
            if (pending != 0)
                startPending();
            else if ((active >> 24) == 2)
            {
                active = (1u << 24) | ((active >> 12) & 0xfffu);
                startChord();
            }
            else
                active = 0;
        }
        return output;
    }

private:
    void startPending()
    {
        active = pending;
        pending = 0;
        replacementLeft = 0;
        startChord();
    }

    void startChord()
    {
        position = 0;
        phases.fill (0.0);
        int previous = -1;
        for (int i = 0; i < 3; ++i)
        {
            int note = 60 + int ((active >> (4 * i)) & 15u);
            while (note <= previous)
                note += 12;
            previous = note;
            increments[i] = twoPi * (440.0 * std::pow (2.0, (note - 69) / 12.0)) / sampleRate;
        }
    }

    static constexpr double twoPi = 6.2831853071795864769;
    static_assert (std::atomic<uint32_t>::is_always_lock_free, "Preview requests must be lock-free");
    std::atomic<uint32_t> mailbox { 0 };
    uint32_t active = 0, pending = 0;
    double sampleRate = 44100.0;
    int duration = 44100, fade = 441, position = 0, replacementLeft = 0;
    std::array<double, 3> phases {}, increments {};
};
