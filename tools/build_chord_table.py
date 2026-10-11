#!/usr/bin/env python3
"""
Builds the "which chord usually comes next" table for the EZ Chords & Harmony plugin.

It reads the Hooktheory dataset (Hooktheory.json.gz), counts how often each chord
is followed by each other chord in real songs, and writes the result to
Source/ChordTransitions.h so the plugin can use it with no internet connection.

Run it from anywhere:

    python3 tools/build_chord_table.py

or point it at the dataset if it lives somewhere else:

    python3 tools/build_chord_table.py /path/to/Hooktheory.json.gz

Data credit: the Hooktheory dataset released with Sheet Sage (Donahue, Thickstun
and Liang, 2022), derived from Hooktheory's TheoryTab database.
Licence: CC BY-NC-SA 3.0. The table this script writes is derived from that data,
so the same terms apply to it: give credit, non-commercial use only, share alike.
"""

import gzip
import json
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent
DEFAULT_DATASET = PROJECT / "Hooktheory.json.gz"
OUTPUT = PROJECT / "Source" / "ChordTransitions.h"

# The gaps between the notes of a scale, in half-steps.
MAJOR_SCALE = (2, 2, 1, 2, 2, 2)
MINOR_SCALE = (2, 1, 2, 2, 1, 2)

# A chord's first two gaps tell us its basic type. Extra notes (7ths, 9ths) are
# ignored, so C7 counts as C. Anything else (sus chords, power chords) is skipped.
QUALITIES = {(4, 3): 0, (3, 4): 1, (3, 3): 2}      # 0 = major, 1 = minor, 2 = diminished
QUALITY_SUFFIX = ["", "m", "dim"]

# Every chord becomes one of 36 "states": 12 possible roots x 3 chord types.
# The root is measured in half-steps above the key note, so C -> G in the key of C
# and D -> A in the key of D are counted as the same move.
NUM_STATES = 36

# Used only for the printed summary, which shows everything as if the key were C.
NOTE_NAMES = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]


def state_of(chord, tonic):
    """Turns one chord from the dataset into a state number 0-35, or None to skip it."""
    quality = QUALITIES.get(tuple(chord["root_position_intervals"][:2]))
    if quality is None:
        return None
    steps_above_key_note = (chord["root_pitch_class"] - tonic) % 12
    return steps_above_key_note * 3 + quality


def state_name(state):
    return NOTE_NAMES[state // 3] + QUALITY_SUFFIX[state % 3]


def key_at(keys, beat):
    """The key in force at a given beat (a few songs change key part-way through)."""
    current = keys[0]
    for key in keys:
        if key["beat"] <= beat:
            current = key
    return current


def count_transitions(dataset):
    """Returns {"major": counts, "minor": counts}; counts[a][b] = times a was followed by b."""
    counts = {mode: [[0] * NUM_STATES for _ in range(NUM_STATES)] for mode in ("major", "minor")}
    sections_used = {"major": 0, "minor": 0}

    for song in dataset.values():
        annotations = song["annotations"]
        keys = annotations.get("keys") or []
        harmony = annotations.get("harmony") or []
        if not keys or not harmony:
            continue

        keys = sorted(keys, key=lambda k: k["beat"])
        modes_seen = set()
        previous = None          # (key, state) of the chord before this one

        for chord in sorted(harmony, key=lambda c: c["onset"]):
            key = key_at(keys, chord["onset"])
            scale = tuple(key["scale_degree_intervals"])
            mode = "major" if scale == MAJOR_SCALE else "minor" if scale == MINOR_SCALE else None
            state = state_of(chord, key["tonic_pitch_class"]) if mode else None

            if state is None:
                previous = None                      # unknown chord or scale: break the chain
                continue

            here = (key["beat"], mode, state)
            if previous is not None and previous[:2] == here[:2] and previous[2] != state:
                counts[mode][previous[2]][state] += 1
                modes_seen.add(mode)
            previous = here

        for mode in modes_seen:
            sections_used[mode] += 1

    return counts, sections_used


def to_chances(counts):
    """Turns raw counts into chances that add up to 1 across each row."""
    chances = []
    for row in counts:
        total = sum(row)
        chances.append([n / total if total else 0.0 for n in row])
    return chances


def cpp_table(name, rows, fmt):
    lines = [f"static constexpr {'int' if fmt == 'd' else 'float'} {name}[{NUM_STATES}][{NUM_STATES}] =", "{"]
    for state, row in enumerate(rows):
        cells = ", ".join((f"{v:d}" if fmt == "d" else (f"{v:.4f}f" if v else "0.0f")) for v in row)
        lines.append(f"    {{ {cells} }},   // from {state_name(state)}")
    lines.append("};")
    return "\n".join(lines)


def cpp_row_totals(name, counts):
    totals = ", ".join(str(sum(row)) for row in counts)
    return f"static constexpr int {name}[{NUM_STATES}] = {{ {totals} }};"


def write_header(counts, sections_used):
    major, minor = counts["major"], counts["minor"]
    text = f"""#pragma once

// GENERATED FILE. Do not edit by hand: run tools/build_chord_table.py to rebuild it.
//
// How often each chord is followed by each other chord in real songs.
//
// Data credit: Hooktheory dataset released with Sheet Sage (Donahue, Thickstun and
// Liang, 2022), derived from Hooktheory's TheoryTab database.
// Licence: CC BY-NC-SA 3.0. This table is derived from that data, so the same terms
// apply: give credit, non-commercial use only, share alike.
//
// Built from {sections_used['major']} major-key and {sections_used['minor']} minor-key song sections.
//
// A chord is a "state" number from 0 to 35:
//     state = halfStepsAboveKeyNote * 3 + type      (type: 0 major, 1 minor, 2 diminished)
// The comments on each row show the chord as it would be in the key of C.

namespace chords
{{
namespace data
{{

static constexpr int numStates = {NUM_STATES};

// How many times each chord was seen with something after it. Rows with a small
// number here are based on little evidence, so treat their chances with caution.
{cpp_row_totals('majorKeyTimesSeen', major)}
{cpp_row_totals('minorKeyTimesSeen', minor)}

// majorKeyChance[from][to] = chance (0 to 1) that `to` comes right after `from`.
{cpp_table('majorKeyChance', to_chances(major), 'f')}

{cpp_table('minorKeyChance', to_chances(minor), 'f')}

inline int stateOf (int halfStepsAboveKeyNote, int type)
{{
    return (((halfStepsAboveKeyNote % 12) + 12) % 12) * 3 + type;
}}

// The chance that the chord `toState` comes right after `fromState`.
inline float chanceOfNext (bool minorKey, int fromState, int toState)
{{
    return minorKey ? minorKeyChance[fromState][toState] : majorKeyChance[fromState][toState];
}}

}} // namespace data
}} // namespace chords
"""
    OUTPUT.write_text(text)


def print_summary(counts):
    print("\nMost common next chords in major keys, shown as if the key were C:")
    chances = to_chances(counts["major"])
    for start in ("C", "Dm", "Em", "F", "G", "Am"):
        state = next(s for s in range(NUM_STATES) if state_name(s) == start)
        ranked = sorted(range(NUM_STATES), key=lambda s: -chances[state][s])[:6]
        row = ",  ".join(f"{state_name(s)} {chances[state][s] * 100:.0f}%" for s in ranked)
        print(f"  after {start:<3} ({sum(counts['major'][state]):>6} times):  {row}")


def main():
    dataset_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_DATASET
    if not dataset_path.exists():
        sys.exit(f"Could not find the dataset at {dataset_path}\n"
                 "Put Hooktheory.json.gz in the project folder, or pass its path to this script.")

    print(f"Reading {dataset_path.name} (this takes a few seconds)...")
    with gzip.open(dataset_path, "rt", encoding="utf-8") as f:
        dataset = json.load(f)

    counts, sections_used = count_transitions(dataset)
    write_header(counts, sections_used)

    total = sum(sum(row) for mode in counts.values() for row in mode)
    print(f"Counted {total} chord changes across {len(dataset)} song sections.")
    print(f"Wrote {OUTPUT.relative_to(PROJECT)}")
    print_summary(counts)


if __name__ == "__main__":
    main()
