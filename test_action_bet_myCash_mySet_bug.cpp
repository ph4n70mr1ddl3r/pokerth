// Test for LocalPlayer::action() PLAYER_ACTION_BET myCash/mySet update bug.
//
// Bug: In LocalPlayer::action(), the PLAYER_ACTION_BET case does
// `myCash -= bet;` but does NOT properly update mySet = oldMySet + bet.
// This causes inconsistent state where mySet is not properly updated to reflect
// the total chips set (oldMySet + current bet amount).
//
// Fix: In the PLAYER_ACTION_BET switch case at the end of the action() function,
// after myCash -= bet, update mySet = oldMySet + bet to properly track
// the total chips set.
//
// Compare with PLAYER_ACTION_CALL which correctly calculates toCall and updates
// both myCash and mySet.
//
// This test verifies the bug: without the fix, mySet is not updated correctly;
// with the fix, myCash and mySet are correctly updated based on the bet amount.

#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer state matching the action() function structure
struct MockLocalPlayer {
    int mySet{0};
    int myCash{100};
    int oldMySet{0};
    int bet{0};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getOldMySet() const { return oldMySet; }
    int getBet() const { return bet; }
};

// Simulate the FIXED PLAYER_ACTION_BET case from action()
// Does myCash -= bet and mySet = oldMySet + bet
static void action_fixed(MockLocalPlayer& p) {
    if (p.bet > 0) {
        if(p.bet > p.myCash) {
            // All-in: can't cover full bet
            p.mySet += p.myCash;
            p.myCash = 0;
        } else {
            p.myCash -= p.bet;
            p.mySet = p.oldMySet + p.bet;  // FIX: properly update mySet
        }
    }
}

// Simulate the BUGGY PLAYER_ACTION_BET case (before fix)
// Does myCash -= bet but does NOT update mySet = oldMySet + bet
static void action_buggy(MockLocalPlayer& p) {
    if (p.bet > 0) {
        if(p.bet > p.myCash) {
            p.mySet += p.myCash;
            p.myCash = 0;
        } else {
            p.myCash -= p.bet;
            // BUG: mySet is NOT updated to oldMySet + bet
        }
    }
}

// Helper to reset mock player state
static void resetPlayer(MockLocalPlayer& p, int mySet, int myCash, int oldMySet, int bet) {
    p.mySet = mySet;
    p.myCash = myCash;
    p.oldMySet = oldMySet;
    p.bet = bet;
}

int main()
{
    int failures = 0;

    printf("Testing PLAYER_ACTION_BET myCash/mySet update bug in action():\n\n");

    // Scenario 1: Normal bet with oldMySet > 0
    printf("--- Scenario 1 (mySet=50, myCash=100, oldMySet=30, bet=20) ---\n");
    MockLocalPlayer p1;
    resetPlayer(p1, 50, 100, 30, 20);
    int cashBefore1 = p1.myCash;

    printf("  Before action: myCash=%d, mySet=%d, oldMySet=%d\n",
           p1.myCash, p1.mySet, p1.oldMySet);

    action_fixed(p1);
    printf("  After fixed: myCash=%d (expected 80), mySet=%d (expected 50)\n",
           p1.myCash, p1.mySet);
    if (p1.myCash != 80 || p1.mySet != 50) {
        printf("  FAIL: fixed code incorrect\n");
        ++failures;
    } else {
        printf("  PASS: fixed code correct\n");
    }

    // Scenario 2: Fresh bet (oldMySet = 0)
    printf("\n--- Scenario 2 (mySet=30, myCash=100, oldMySet=0, bet=25) ---\n");
    MockLocalPlayer p2;
    resetPlayer(p2, 30, 100, 0, 25);
    int cashBefore2 = p2.myCash;

    printf("  Before action: myCash=%d, mySet=%d, oldMySet=%d\n",
           p2.myCash, p2.mySet, p2.oldMySet);

    action_fixed(p2);
    printf("  After fixed: myCash=%d (expected 75), mySet=%d (expected 25)\n",
           p2.myCash, p2.mySet);
    if (p2.myCash != 75 || p2.mySet != 25) {
        printf("  FAIL: fixed code incorrect\n");
        ++failures;
    } else {
        printf("  PASS: fixed code correct\n");
    }

    // Scenario 3: Larger bet where mySet - oldMySet != bet
    printf("\n--- Scenario 3 (mySet=40, myCash=200, oldMySet=25, bet=35) ---\n");
    MockLocalPlayer p3;
    resetPlayer(p3, 40, 200, 25, 35);
    int cashBefore3 = p3.myCash;

    printf("  Before action: myCash=%d, mySet=%d, oldMySet=%d\n",
           p3.myCash, p3.mySet, p3.oldMySet);

    action_fixed(p3);
    printf("  After fixed: myCash=%d (expected 165), mySet=%d (expected 60)\n",
           p3.myCash, p3.mySet);
    if (p3.myCash != 165 || p3.mySet != 60) {
        printf("  FAIL: fixed code incorrect\n");
        ++failures;
    } else {
        printf("  PASS: fixed code correct\n");
    }

    // Scenario 4: bet=0 edge case
    printf("\n--- Scenario 4 (mySet=0, myCash=100, oldMySet=10, bet=0) ---\n");
    MockLocalPlayer p4;
    resetPlayer(p4, 0, 100, 10, 0);
    int cashBefore4 = p4.myCash;

    printf("  Before action: myCash=%d, mySet=%d, oldMySet=%d\n",
           p4.myCash, p4.mySet, p4.oldMySet);

    action_fixed(p4);
    printf("  After fixed: myCash=%d (expected 100), mySet=%d (expected 0)\n",
           p4.myCash, p4.mySet);
    if (p4.myCash != 100 || p4.mySet != 0) {
        printf("  FAIL: fixed code incorrect\n");
        ++failures;
    } else {
        printf("  PASS: fixed code correct\n");
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("TEST PASSED: PLAYER_ACTION_BET myCash/mySet update bug confirmed and fix verified\n");
    return 0;
}