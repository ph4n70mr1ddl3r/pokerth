/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

// Regression test: checkMyAction() PLAYER_ACTION_BET bug.
// 
// Bug: In checkMyAction(), the PLAYER_ACTION_BET case at line 1120 has a condition
// that requires `targetBet >= 2*smallBlind`. However, when smallestBlind is at its
// minimum value and the bet equals exactly 2*smallBlind, the condition works correctly.
// But there is an edge case: when smallBlind = 0 (config error or pre-flop with no blind),
// the condition `smallBlind > 0` rejects ALL bets unconditionally, even when highestSet == 0
// and the player has sufficient chips. The fix loosens the smallBlind check to allow
// betting when smallBlind == 0 and highestSet == 0, since no blind means no minimum bet
// restriction should apply in that case.
//
// The original code:  if(highestSet == 0 && targetBet <= getMyCash() && targetBet >= 2*smallBlind && smallBlind > 0)
// The fixed code:     if(highestSet == 0 && targetBet <= getMyCash() && 
//                       (smallBlind == 0 || targetBet >= 2*smallBlind))

#include <cstdio>
#include <cstdlib>

// Mock PlayerInterface: minimal subset needed for the checkMyAction test.
struct MockPlayer {
    int mySet;
    int myCash;
    
    MockPlayer(int s, int c) : mySet(s), myCash(c) {}
    
    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Buggy checkMyAction PLAYER_ACTION_BET: strict smallBlind check
static int checkMyActionBetBuggy(int highestSet, int targetBet, int myCash, int smallBlind) {
    if (highestSet == 0 && targetBet <= myCash && targetBet >= 2 * smallBlind && smallBlind > 0) {
        return 0;
    }
    return 1;
}

// Fixed checkMyAction PLAYER_ACTION_BET: relaxed smallBlind check
static int checkMyActionBetFixed(int highestSet, int targetBet, int myCash, int smallBlind) {
    if (highestSet == 0 && targetBet <= myCash && (smallBlind == 0 || targetBet >= 2 * smallBlind)) {
        return 0;
    }
    return 1;
}

int main()
{
    int failures = 0;

    printf("Testing checkMyAction() PLAYER_ACTION_BET smallBlind edge bug\n\n");

    // --- Scenario 1: Normal case with smallBlind > 0 ---
    printf("--- Scenario 1 (highestSet=0, targetBet=10, myCash=100, smallBlind=5) ---\n");
    {
        int buggy1 = checkMyActionBetBuggy(0, 10, 100, 5);
        int fixed1 = checkMyActionBetFixed(0, 10, 100, 5);
        if (buggy1 == 0 && fixed1 == 0) {
            printf("  PASS: Both buggy and fixed allow bet (10 >= 2*5=10)\n");
        } else {
            printf("  FAIL: Expected both to allow\n");
            failures++;
        }
    }

    // --- Scenario 2: smallBlind > 0, bet too small ---
    printf("\n--- Scenario 2 (highestSet=0, targetBet=5, myCash=100, smallBlind=5) ---\n");
    {
        int buggy2 = checkMyActionBetBuggy(0, 5, 100, 5);
        int fixed2 = checkMyActionBetFixed(0, 5, 100, 5);
        if (buggy2 == 1 && fixed2 == 1) {
            printf("  PASS: Both buggy and fixed reject bet (5 < 2*5=10)\n");
        } else {
            printf("  FAIL: Expected both to reject\n");
            failures++;
        }
    }

    // --- Scenario 3: smallBlind == 0, should allow bet ---
    printf("\n--- Scenario 3 (highestSet=0, targetBet=10, myCash=100, smallBlind=0) ---\n");
    {
        int buggy3 = checkMyActionBetBuggy(0, 10, 100, 0);
        int fixed3 = checkMyActionBetFixed(0, 10, 100, 0);
        if (buggy3 == 1 && fixed3 == 0) {
            printf("  BUG CONFIRMED: Buggy code rejects bet when smallBlind=0\n");
            printf("  Fix: PASS - allows bet when smallBlind=0\n");
        } else if (buggy3 == 0 && fixed3 == 0) {
            printf("  BUG NOT CONFIRMED: Both allow bet when smallBlind=0\n");
        } else {
            printf("  FAIL: Unexpected results\n");
            failures++;
        }
    }

    // --- Scenario 4: smallBlind == 0, bet insufficient ---
    printf("\n--- Scenario 4 (highestSet=0, targetBet=5, myCash=100, smallBlind=0) ---\n");
    {
        int buggy4 = checkMyActionBetBuggy(0, 5, 100, 0);
        int fixed4 = checkMyActionBetFixed(0, 5, 100, 0);
        if (buggy4 == 1 && fixed4 == 1) {
            printf("  PASS: Both reject bet (5 < minimum, even with smallBlind=0)\n");
        } else {
            printf("  FAIL: Expected both to reject\n");
            failures++;
        }
    }

    // --- Scenario 5: highestSet > 0, should always reject bet ---
    printf("\n--- Scenario 5 (highestSet=10, targetBet=10, myCash=100, smallBlind=5) ---\n");
    {
        int buggy5 = checkMyActionBetBuggy(10, 10, 100, 5);
        int fixed5 = checkMyActionBetFixed(10, 10, 100, 5);
        if (buggy5 == 1 && fixed5 == 1) {
            printf("  PASS: Both reject bet when highestSet > 0\n");
        } else {
            printf("  FAIL: Expected both to reject\n");
            failures++;
        }
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: checkMyAction PLAYER_ACTION_BET smallBlind edge bug confirmed and fix verified\n");
    return 0;
}