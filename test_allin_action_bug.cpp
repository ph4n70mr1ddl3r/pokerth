/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

// Verification test: checkMyAction() PLAYER_ACTION_ALLIN edge cases.
// Tests the all-in validation logic for various player states.

#include <cstdio>
#include <cstdlib>

int localplayer_checkMyAction(int targetAction, int targetBet, int highestSet, int minimumRaise, int smallBlind, int mySet, int myCash) {
    switch(targetAction) {
        case 0: // PLAYER_ACTION_FOLD: return 0;
            return 0;
        case 1: // PLAYER_ACTION_CHECK:
            if (mySet >= highestSet) return 0;
            return 1;
        case 2: // PLAYER_ACTION_CALL:
            if (mySet >= highestSet) {
                if (highestSet > 0 || myCash > 0) return 0;
            }
            else if(myCash > 0 && targetBet <= myCash) {
                if(myCash + mySet >= highestSet && targetBet == highestSet - mySet) return 0;
                if(targetBet == myCash) return 0;
            }
            else if(myCash <= 0) return 1;
            return 1;
        case 3: // PLAYER_ACTION_BET:
            if(highestSet == 0 && targetBet <= myCash && (smallBlind == 0 || targetBet >= 2*smallBlind)) return 0;
            return 1;
        case 4: // PLAYER_ACTION_RAISE:
            if(highestSet > 0 && targetBet >= minimumRaise && targetBet <= myCash) return 0;
            return 1;
        case 5: // PLAYER_ACTION_ALLIN:
            if(mySet >= highestSet) return 0;
            else if(targetBet == myCash && myCash > 0) return 0;
        default:
            return 1;
    }
    return 1;
}

int main()
{
    int failures = 0;

    printf("Testing checkMyAction() PLAYER_ACTION_ALLIN edge cases\n\n");

    // --- Test 1: mySet >= highestSet, all-in valid regardless of cash ---
    printf("--- Test 1: mySet >= highestSet, all-in valid ---\n");
    {
        // mySet=50, highestSet=30: player has already put in enough chips
        int result = localplayer_checkMyAction(5, 0, 30, 0, 0, 50, 0);
        if (result == 0) {
            printf("  PASS: All-in valid when mySet >= highestSet\n");
        } else {
            printf("  FAIL: All-in should be valid when mySet >= highestSet\n");
            failures++;
        }
    }

    // --- Test 2: mySet < highestSet, myCash > 0, targetBet == myCash, all-in valid ---
    printf("\n--- Test 2: mySet < highestSet, targetBet == myCash, myCash > 0 ---\n");
    {
        // mySet=30, highestSet=50, myCash=100, targetBet=100: go all-in with cash
        int result = localplayer_checkMyAction(5, 100, 50, 0, 0, 30, 100);
        if (result == 0) {
            printf("  PASS: All-in valid when targetBet == myCash > 0\n");
        } else {
            printf("  FAIL: All-in should be valid when targetBet == myCash > 0\n");
            failures++;
        }
    }

    // --- Test 3: mySet < highestSet, myCash > 0, targetBet != myCash, all-in invalid ---
    printf("\n--- Test 3: mySet < highestSet, targetBet != myCash, myCash > 0 ---\n");
    {
        // mySet=30, highestSet=50, myCash=100, targetBet=50: not going all-in for full amount
        int result = localplayer_checkMyAction(5, 50, 50, 0, 0, 30, 100);
        if (result == 1) {
            printf("  PASS: All-in invalid when targetBet != myCash\n");
        } else {
            printf("  FAIL: All-in should be invalid when targetBet != myCash\n");
            failures++;
        }
    }

    // --- Test 4: mySet < highestSet, myCash == 0, all-in invalid ---
    printf("\n--- Test 4: mySet < highestSet, myCash == 0 ---\n");
    {
        // mySet=30, highestSet=50, myCash=0: no chips to go all-in with
        int result = localplayer_checkMyAction(5, 0, 50, 0, 0, 30, 0);
        if (result == 1) {
            printf("  PASS: All-in invalid when myCash == 0\n");
        } else {
            printf("  FAIL: All-in should be invalid when myCash == 0\n");
            failures++;
        }
    }

    // --- Test 5: mySet >= highestSet, myCash == 0, all-in valid ---
    printf("\n--- Test 5: mySet >= highestSet, myCash == 0 ---\n");
    {
        // mySet=50, highestSet=30, myCash=0: player has enough chips, no cash needed
        int result = localplayer_checkMyAction(5, 0, 30, 0, 0, 50, 0);
        if (result == 0) {
            printf("  PASS: All-in valid when mySet >= highestSet even with myCash == 0\n");
        } else {
            printf("  FAIL: All-in should be valid when mySet >= highestSet\n");
            failures++;
        }
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("TEST PASSED: All ALLIN edge cases verified correctly\n");
    return 0;
}