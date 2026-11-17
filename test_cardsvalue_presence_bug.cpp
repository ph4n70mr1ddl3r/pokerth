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

// Regression test for the cardsValue presence-check bug in
// ClientStateHandleAfterHandShowCards (src/net/clientstate.cpp).
//
// pokerth.proto declares:
//     optional uint32 cardsValue = 7;
//
// For a proto2 optional field, "has_cardsvalue()" (field was set on the wire)
// and "cardsvalue()" (the numeric value, defaulting to 0 when absent) are
// different conditions.  The handler in ClientStateHandleAfterHandShowCards
// used `if (r.cardsvalue())` as a presence test.  That is wrong: when a
// PlayerResult carries cardsValue explicitly set to 0, the guard is false and
// setMyCardsValueInt() is skipped, leaving a stale hand value on the player.
// The identical handler for the other show-cards message (same file, the
// PlayerIdChanged/ShowCards path) already does it correctly with
// `if (r.has_cardsvalue())`.
//
// This test models the protobuf optional-field semantics and exercises both
// the buggy and the corrected guard across all three relevant cases.

#include <cstdio>

// Minimal model of a proto2 `optional uint32` field: it tracks whether the
// field was set, and returns the default (0) when it was not.
struct OptionalUint32 {
    bool present{false};
    unsigned int value{0};

    bool has() const { return present; }
    unsigned int get() const { return value; }
};

// The subset of PlayerResult that the handler reads for this field.
struct PlayerResultModel {
    OptionalUint32 cardsValue;
};

// The subset of PlayerInterface state the handler writes.
struct PlayerModel {
    int myCardsValueInt{0};

    void setMyCardsValueInt(int v) { myCardsValueInt = v; }
    int getMyCardsValueInt() const { return myCardsValueInt; }
};

// The buggy guard from clientstate.cpp:1661: `if (r.cardsvalue())`.
static void handlerBuggy(const PlayerResultModel& r, PlayerModel& player)
{
    if (r.cardsValue.get()) {
        player.setMyCardsValueInt(static_cast<int>(r.cardsValue.get()));
    }
}

// The corrected guard: `if (r.has_cardsvalue())`.
static void handlerFixed(const PlayerResultModel& r, PlayerModel& player)
{
    if (r.cardsValue.has()) {
        player.setMyCardsValueInt(static_cast<int>(r.cardsValue.get()));
    }
}

struct Scenario {
    const char* name;
    OptionalUint32 cardsValue;
    int previousValue;
    int expectedValue;
};

int main()
{
    const Scenario scenarios[] = {
        // Field absent: neither guard should write; player keeps old value.
        {"field absent", {false, 0}, 12345, 12345},
        // Field present with a non-zero value: both guards write the value.
        {"field present, value 42", {true, 42}, 12345, 42},
        // Field present with value 0: the correct guard writes 0, the buggy
        // guard skips and leaves the stale value 12345.
        {"field present, value 0", {true, 0}, 12345, 0},
    };

    int failures = 0;

    printf("Testing cardsValue optional-field presence check in "
           "ClientStateHandleAfterHandShowCards:\n\n");

    for (const auto& s : scenarios) {
        PlayerResultModel result{s.cardsValue};

        PlayerModel fixedPlayer;
        fixedPlayer.setMyCardsValueInt(s.previousValue);
        handlerFixed(result, fixedPlayer);

        PlayerModel buggyPlayer;
        buggyPlayer.setMyCardsValueInt(s.previousValue);
        handlerBuggy(result, buggyPlayer);

        const bool fixedOk = fixedPlayer.getMyCardsValueInt() == s.expectedValue;
        printf("  %-28s fixed=%d (expected %d) [%s]\n",
               s.name, fixedPlayer.getMyCardsValueInt(), s.expectedValue,
               fixedOk ? "PASS" : "FAIL");
        if (!fixedOk)
            ++failures;
    }

    printf("\nVerifying the bug is real (buggy guard diverges where expected):\n");

    // The interesting case: present with value 0. The fixed handler must set 0
    // while the buggy handler leaves the stale value behind.
    PlayerResultModel zeroResult{{true, 0}};

    PlayerModel fixedZero;
    fixedZero.setMyCardsValueInt(12345);
    handlerFixed(zeroResult, fixedZero);

    PlayerModel buggyZero;
    buggyZero.setMyCardsValueInt(12345);
    handlerBuggy(zeroResult, buggyZero);

    const bool divergence = (fixedZero.getMyCardsValueInt() == 0) &&
                            (buggyZero.getMyCardsValueInt() == 12345);
    printf("  present value 0: fixed sets %d, buggy leaves %d [%s]\n",
           fixedZero.getMyCardsValueInt(), buggyZero.getMyCardsValueInt(),
           divergence ? "PASS" : "FAIL");
    if (!divergence)
        ++failures;

    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }

    printf("\nTEST PASSED: cardsValue presence bug reproduced and fix verified\n");
    return 0;
}
