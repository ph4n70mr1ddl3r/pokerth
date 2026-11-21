/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU Affero General Public License as            *
 * published by the Free Software Foundation, either version 3 of the        *
 * License, or (at your option) any later version.                           *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU Affero General Public License for more details.                       *
 *                                                                           *
 * You should have received a copy of the GNU Affero General Public License  *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************/

// Regression test: ArrayData::getHandChancePreflop(int, int**) writes the
// hand-chance table with the wrong index order (and out of bounds).
//
// Background
// ----------
// handChancePreflop[] is a table of calcHandsData whose member is
//     int data[10][2];   // data[handClass][field], field 0 = % chance, 1 = presence
// ArrayData exposes two overloads that copy this table for a given hand code:
//
//   vector<vector<int>> getHandChancePreflop(int)   // arraydata.cpp:1049
//       Returns a 2x10 buffer laid out as chance[field][handClass]:
//           chance[j][i] = handChancePreflop[val].data[i][j];
//       This is the overload used by CardsValue::calcCardsChance()
//       (cardsvalue.cpp:660) and is CORRECT.
//
//   void getHandChancePreflop(int, int** values)     // arraydata.cpp:1029
//       Copies with: values[i][j] = handChancePreflop[val].data[i][j];
//       i in [0,10), j in [0,2).  This is WRONG in two ways:
//
// The bug
// -------
// 1. Transposition: the field/handClass indices are not swapped, so the data
//    lands in the opposite layout from the vector overload. A caller passing
//    the natural 2-row buffer (2 fields x 10 classes) reads garbage.
// 2. Out-of-bounds: with a 2-row buffer (values[0], values[1]), the loop also
//    writes values[2] .. values[9], i.e. eight pointer slots past the end of
//    the intended buffer -> memory corruption.
//
// For hand code 12120 the table row is:
//   { {0,0}, {36,1}, {40,1}, {12,1}, {1,1}, {2,1}, {9,1}, {1,1}, {0,1}, {0,1} }
// so the correct chance[0] (probabilities) must be {0,36,40,12,1,2,9,1,0,0}
// and chance[1] (presence) must be {0,1,1,1,1,1,1,1,0,0}.
//
// The fix
// -------
// Use the same transposed copy as the vector overload:
//     values[j][i] = handChancePreflop[val].data[i][j];
//
// This standalone test models the table and both copy loops without Qt,
// Boost or libircclient. It allocates the natural 2x10 output buffer with
// guard buffers hanging off the pointer slots beyond values[1], so the buggy
// loop's out-of-bounds writes are detected safely rather than crashing.

#include <cstdio>
#include <vector>

#define NUM_HAND_CHANCE_PREFLOP_FIELDS 2
#define NUM_HAND_CHANCE_PREFLOP_CLASSES 10
#define GUARD_SENTINEL -12345

struct CalcHandsData {
    int hand;
    int data[NUM_HAND_CHANCE_PREFLOP_CLASSES][NUM_HAND_CHANCE_PREFLOP_FIELDS];
};

// A faithful subset of the real handChancePreflop[] table (arraydata.cpp).
static const CalcHandsData handChancePreflop[] = {
    { 10100, { {0,0}, {36,1}, {40,1}, {12,1}, {2,1}, {2,1}, {9,1}, {1,1}, {0,1}, {0,1} } },
    { 11110, { {0,0}, {36,1}, {40,1}, {12,1}, {1,1}, {2,1}, {9,1}, {1,1}, {0,1}, {0,1} } },
    { 12120, { {0,0}, {36,1}, {40,1}, {12,1}, {1,1}, {2,1}, {9,1}, {1,1}, {0,1}, {0,1} } },
    { 11121, { {18,1}, {43,1}, {22,1}, {4,1}, {3,1}, {7,1}, {2,1}, {0,1}, {0,1}, {0,1} } }
};
static const unsigned NUM_ENTRIES =
    sizeof(handChancePreflop) / sizeof(handChancePreflop[0]);

static const CalcHandsData *findEntry(int handCode)
{
    for (unsigned val = 0; val < NUM_ENTRIES; ++val) {
        if (handCode == handChancePreflop[val].hand)
            return &handChancePreflop[val];
    }
    return 0;
}

// Model of the buggy pointer overload (arraydata.cpp:1037-1041).
static void copyBuggy(int handCode, int **values)
{
    const CalcHandsData *entry = findEntry(handCode);
    if (!entry) return;
    for (int i = 0; i < NUM_HAND_CHANCE_PREFLOP_CLASSES; ++i) {
        for (int j = 0; j < NUM_HAND_CHANCE_PREFLOP_FIELDS; ++j) {
            values[i][j] = entry->data[i][j];   // no transpose -> OOB
        }
    }
}

// Model of the fixed pointer overload (matches the vector overload).
static void copyFixed(int handCode, int **values)
{
    const CalcHandsData *entry = findEntry(handCode);
    if (!entry) return;
    for (int i = 0; i < NUM_HAND_CHANCE_PREFLOP_CLASSES; ++i) {
        for (int j = 0; j < NUM_HAND_CHANCE_PREFLOP_FIELDS; ++j) {
            values[j][i] = entry->data[i][j];   // transposed -> correct
        }
    }
}

// Model of the correct vector overload (arraydata.cpp:1069-1073).
static std::vector<std::vector<int> > copyVector(int handCode)
{
    std::vector<std::vector<int> > chance(2, std::vector<int>(10, 0));
    const CalcHandsData *entry = findEntry(handCode);
    if (!entry) return chance;
    for (int i = 0; i < NUM_HAND_CHANCE_PREFLOP_CLASSES; ++i) {
        for (int j = 0; j < NUM_HAND_CHANCE_PREFLOP_FIELDS; ++j) {
            chance[j][i] = entry->data[i][j];
        }
    }
    return chance;
}

// Allocate a natural 2x10 output buffer plus guard buffers for every extra
// pointer slot the buggy loop might reach. Returns the base pointer to hand
// to the copy function; guards can be inspected afterwards.
struct OutputBuffer {
    int rows[NUM_HAND_CHANCE_PREFLOP_FIELDS][NUM_HAND_CHANCE_PREFLOP_CLASSES];
    int guards[NUM_HAND_CHANCE_PREFLOP_CLASSES - NUM_HAND_CHANCE_PREFLOP_FIELDS][NUM_HAND_CHANCE_PREFLOP_FIELDS];
    int *slots[NUM_HAND_CHANCE_PREFLOP_CLASSES];

    OutputBuffer()
    {
        for (int j = 0; j < NUM_HAND_CHANCE_PREFLOP_FIELDS; ++j) {
            for (int i = 0; i < NUM_HAND_CHANCE_PREFLOP_CLASSES; ++i)
                rows[j][i] = 0;
        }
        for (int g = 0; g < NUM_HAND_CHANCE_PREFLOP_CLASSES - NUM_HAND_CHANCE_PREFLOP_FIELDS; ++g) {
            for (int j = 0; j < NUM_HAND_CHANCE_PREFLOP_FIELDS; ++j)
                guards[g][j] = GUARD_SENTINEL;
        }
        slots[0] = rows[0];
        slots[1] = rows[1];
        // slots[2..9] point at guard buffers: an OOB write of values[i][j]
        // lands in guards[i-2] instead of crashing.
        for (int i = 2; i < NUM_HAND_CHANCE_PREFLOP_CLASSES; ++i)
            slots[i] = guards[i - 2];
    }

    bool guardsIntact() const
    {
        for (int g = 0; g < NUM_HAND_CHANCE_PREFLOP_CLASSES - NUM_HAND_CHANCE_PREFLOP_FIELDS; ++g) {
            for (int j = 0; j < NUM_HAND_CHANCE_PREFLOP_FIELDS; ++j) {
                if (guards[g][j] != GUARD_SENTINEL)
                    return false;
            }
        }
        return true;
    }
};

static bool matchesVector(const OutputBuffer &buf, const std::vector<std::vector<int> > &v)
{
    for (int j = 0; j < NUM_HAND_CHANCE_PREFLOP_FIELDS; ++j) {
        for (int i = 0; i < NUM_HAND_CHANCE_PREFLOP_CLASSES; ++i) {
            if (buf.slots[j][i] != v[j][i])
                return false;
        }
    }
    return true;
}

int main()
{
    int failures = 0;
    const int handCode = 12120;
    const std::vector<std::vector<int> > expected = copyVector(handCode);

    printf("Testing ArrayData::getHandChancePreflop(int, int**) transposition/OOB bug\n\n");

    // 1. Reproduce the bug: the buggy loop corrupts the guard buffers.
    {
        OutputBuffer buf;
        copyBuggy(handCode, buf.slots);
        if (!buf.guardsIntact()) {
            printf("PASS(bug reproduced): buggy loop wrote past values[1] (guards clobbered)\n");
        } else {
            printf("FAIL: buggy loop did not write out of bounds\n");
            ++failures;
        }
        if (matchesVector(buf, expected)) {
            printf("FAIL: buggy loop unexpectedly matched the vector overload layout\n");
            ++failures;
        } else {
            printf("PASS(bug reproduced): buggy loop layout differs from vector overload\n");
        }
    }

    // 2. Fixed loop must leave the guards intact and match the vector overload.
    {
        OutputBuffer buf;
        copyFixed(handCode, buf.slots);
        if (!buf.guardsIntact()) {
            printf("FAIL: fixed loop wrote out of bounds\n");
            ++failures;
        } else {
            printf("PASS: fixed loop stays within the 2x10 buffer\n");
        }
        if (matchesVector(buf, expected)) {
            printf("PASS: fixed loop output matches the vector overload\n");
        } else {
            printf("FAIL: fixed loop output differs from the vector overload\n");
            ++failures;
        }
    }

    // 3. Verify the exact expected values for hand code 12120.
    {
        OutputBuffer buf;
        copyFixed(handCode, buf.slots);
        const int expectedChance[10] = {0, 36, 40, 12, 1, 2, 9, 1, 0, 0};
        const int expectedPresence[10] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        for (int i = 0; i < 10; ++i) {
            if (buf.slots[0][i] != expectedChance[i] || buf.slots[1][i] != expectedPresence[i]) {
                printf("FAIL: fixed values wrong at class %d (%d,%d)\n",
                       i, buf.slots[0][i], buf.slots[1][i]);
                ++failures;
                break;
            }
        }
        if (failures == 0)
            printf("PASS: fixed chance[0]/chance[1] values are correct\n");
    }

    // 4. Unknown hand codes must be a no-op for both loops (no writes).
    {
        OutputBuffer a, b;
        copyBuggy(999999, a.slots);
        copyFixed(999999, b.slots);
        if (a.guardsIntact() && b.guardsIntact() &&
            a.slots[0][0] == 0 && b.slots[0][0] == 0) {
            printf("PASS: unknown hand code is a no-op\n");
        } else {
            printf("FAIL: unknown hand code modified output\n");
            ++failures;
        }
    }

    if (failures > 0) {
        printf("\nTEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("\nTEST PASSED: transposition/OOB bug reproduced and fix verified\n");
    return 0;
}
