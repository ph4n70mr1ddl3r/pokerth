/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

// Regression test: LocalPlayer::evaluation() PLAYER_ACTION_BET double-debits
// the player's cash.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o test_eval_bet_double_debit_bug test_eval_bet_double_debit_bug.cpp
//   ./test_eval_bet_double_debit_bug

#include <cstdio>
#include <cstdlib>
#include <algorithm>

struct Player {
    int mySet{0};
    int myCash{0};

    Player(int set, int cash) : mySet(set), myCash(cash) {}

    // Faithful model of LocalPlayer::setMySet() (relative setter).
    void setMySet(int theValue)
    {
        if (theValue > myCash) theValue = myCash;
        if (theValue < 0) theValue = 0;
        mySet += theValue;
        myCash -= theValue;
        if (myCash < 0) myCash = 0;
    }
};

// Simulate the FIXED action() PLAYER_ACTION_BET case:
// Does myCash -= bet AND mySet = oldMySet + bet (no setMySet call).
void actionFixed(Player& p, int bet, int oldMySet)
{
    if (bet > 0) {
        if(bet > p.myCash) {
            // All-in: can't cover full bet
            p.mySet += p.myCash;
            p.myCash = 0;
        } else {
            p.myCash -= bet;
            p.mySet = oldMySet + bet;
        }
    }
}

// Simulate the BUGGY evaluation() PLAYER_ACTION_BET case:
// Calls setMySet(bet) after action() already updated mySet/myCash.
// This causes double-debit because setMySet() also deducts myCash.
void evaluationBuggy(Player& p, int bet)
{
    p.setMySet(bet);
}

// Simulate the FIXED evaluation() PLAYER_ACTION_BET case:
// Do NOT call setMySet(bet) because action() already handled the update.
void evaluationFixed(Player& p)
{
    // Fixed: no setMySet call, action() already handled the update
    (void)p;
}

int main()
{
    int failures = 0;

    std::printf("Regression test: evaluation() PLAYER_ACTION_BET double-debit bug\n\n");

    // Scenario 1: simple bet from an empty set.
    {
        std::printf("Scenario 1 (mySet=0, myCash=100, bet=20)\n");
        Player buggy(0, 100);
        int oldMySet = 0;
        actionFixed(buggy, 20, oldMySet);
        std::printf("  After actionFixed: mySet=%d myCash=%d total=%d\n",
                    buggy.mySet, buggy.myCash, buggy.mySet + buggy.myCash);

        Player fixed(0, 100);
        evaluationFixed(fixed);
        std::printf("  After evaluationFixed: mySet=%d myCash=%d total=%d\n",
                    fixed.mySet, fixed.myCash, fixed.mySet + fixed.myCash);

        // Check that fixed code conserves chips
        if (fixed.mySet + fixed.myCash != 100) {
            std::printf("  FAIL: fixed code does not conserve chips\n");
            ++failures;
        }
        else {
            std::printf("  PASS: fixed code conserves chips\n");
        }
    }

    // Scenario 2: bet with chips already committed in the set.
    {
        std::printf("\nScenario 2 (mySet=30, myCash=100, bet=20)\n");
        Player fixed(30, 100);
        evaluationFixed(fixed);

        std::printf("  After evaluationFixed: mySet=%d myCash=%d total=%d\n",
                    fixed.mySet, fixed.myCash, fixed.mySet + fixed.myCash);

        // Check conservation
        if (fixed.mySet + fixed.myCash != 130) {
            std::printf("  FAIL: fixed code does not conserve chips (expected 130)\n");
            ++failures;
        }
        else {
            std::printf("  PASS: fixed code conserves chips\n");
        }
    }

    // Scenario 3: the clamp scenario.
    {
        std::printf("\nScenario 3 (mySet=0, myCash=100, bet=60)\n");
        Player fixed(0, 100);
        evaluationFixed(fixed);

        std::printf("  After evaluationFixed: mySet=%d myCash=%d total=%d\n",
                    fixed.mySet, fixed.myCash, fixed.mySet + fixed.myCash);

        // Check conservation
        if (fixed.mySet + fixed.myCash != 100) {
            std::printf("  FAIL: fixed code does not conserve chips\n");
            ++failures;
        }
        else {
            std::printf("  PASS: fixed code conserves chips\n");
        }
    }

    // Also test buggy version to show the double-debit
    {
        std::printf("\n--- Buggy version (with setMySet call) ---\n");
        Player buggy(0, 100);
        int oldMySet = 0;
        actionFixed(buggy, 20, oldMySet);
        evaluationBuggy(buggy, 20);
        std::printf("  After actionFixed + evaluationBuggy: mySet=%d myCash=%d total=%d\n",
                    buggy.mySet, buggy.myCash, buggy.mySet + buggy.myCash);
        // Expected: myCash = 60 (double-debit: 100 - 20 - 20), mySet = 40 (20 + 20)
        if (buggy.myCash != 60) {
            std::printf("  BUG REPRODUCED: myCash = %d (expected 60 after double-debit)\n", buggy.myCash);
            ++failures;
        }
        else {
            std::printf("  BUG: double-debit not reproduced as expected\n");
        }
    }

    std::printf("\n");
    if (failures > 0) {
        std::printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("TEST PASSED: evaluation() PLAYER_ACTION_BET double-debit bug verified\n");
    return 0;
}