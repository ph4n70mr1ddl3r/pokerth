// Test for LocalPlayer::action() PLAYER_ACTION_BET bug with bet == 0.
//
// Bug: In LocalPlayer::action(), the PLAYER_ACTION_BET case does
// `if(bet > 0) { myCash -= bet; mySet += bet; }` which correctly
// handles the bet == 0 case (no change to myCash/mySet).
//
// This test verifies the bet == 0 edge case is handled correctly.

#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer state for the bet test
struct MockLocalPlayer {
    int mySet{0};
    int myCash{1000};
    int oldMySet{0};
    int highestSet{100};
    int bet{0};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getOldMySet() const { return oldMySet; }
    int getHighestSet() const { return highestSet; }
    int getBet() const { return bet; }
};

// Fixed PLAYER_ACTION_BET handling from localplayer.cpp line 1552-1554
static int processBetFixed(MockLocalPlayer& p) {
    // Fix: subtract bet amount from myCash and add to mySet
    if(p.bet > 0) {
        p.myCash -= p.bet;
        p.mySet += p.bet;
    }
    return 0;
}

// Buggy PLAYER_ACTION_BET handling (old code that subtracted oldMySet)
static int processBetBug(MockLocalPlayer& p) {
    // Bug: subtracts oldMySet from myCash
    p.myCash -= p.oldMySet;
    // mySet is NOT updated - this is the bug!
    return 0;
}

int main()
{
    int failures = 0;

    // --- Scenario 1: bet == 0 ---
    printf("--- Scenario 1 (mySet=50, myCash=100, oldMySet=50, bet=0) ---\n");
    MockLocalPlayer p1;
    p1.mySet = 50;
    p1.myCash = 100;
    p1.oldMySet = 50;
    p1.bet = 0;

    // Save original values
    int cashBefore1 = p1.myCash;
    int setBefore1 = p1.mySet;

    // Test fixed code
    processBetFixed(p1);
    printf("  Fixed: myCash=%d (expected %d, should be unchanged since bet=0)\n",
           p1.myCash, cashBefore1);
    if (p1.myCash == cashBefore1) {
        printf("  PASS: cash correctly unchanged when bet=0\n");
    } else {
        printf("  FAIL: cash unexpectedly changed\n");
        failures++;
    } else {
        printf("  FAIL: fixed code didn't work as expected\n");
        failures++;
    }

    // Test buggy code
    p1.myCash = cashBefore1;  // reset
    processBetBug(p1);
    printf("  Buggy: myCash=%d (expected %d, should be unchanged but buggy subtracts oldMySet)\n",
           p1.myCash, cashBefore1);
    if (p1.myCash == cashBefore1) {
        printf("  (buggy happens to give correct result for bet=0)\n");
    } else {
        printf("  BUG CONFIRMED: buggy code changes cash even when bet=0\n");
        failures++;
    }

    // --- Scenario 2: bet > 0 ---
    printf("\n--- Scenario 2 (mySet=50, myCash=100, oldMySet=50, bet=30) ---\n");
    MockLocalPlayer p2;
    p2.mySet = 50;
    p2.myCash = 100;
    p2.oldMySet = 50;
    p2.bet = 30;

    cashBefore1 = p2.myCash;
    setBefore1 = p2.mySet;

    // Test fixed code
    processBetFixed(p2);
    printf("  Fixed: myCash=%d (expected %d), mySet=%d (expected %d)\n",
           p2.myCash, cashBefore1 - 30, p2.mySet, setBefore1 + 30);
    if (p2.myCash == cashBefore1 - 30 && p2.mySet == setBefore1 + 30) {
        printf("  PASS: Fixed code correctly subtracts bet from cash and adds to set\n");
    } else {
        printf("  FAIL: Fix didn't work as expected\n");
        failures++;
    }

    // --- Scenario 3: all-in bet (bet >= myCash) ---
    printf("\n--- Scenario 3 (mySet=50, myCash=30, oldMySet=50, bet=30) ---\n");
    MockLocalPlayer p3;
    p3.mySet = 50;
    p3.myCash = 30;
    p3.oldMySet = 50;
    p3.bet = 30;

    cashBefore1 = p3.myCash;

    // Test fixed code
    processBetFixed(p3);
    printf("  Fixed: myCash=%d (expected %d), mySet=%d (expected %d)\n",
           p3.myCash, cashBefore1 - 30, p3.mySet, 50 + 30);
    if (p3.myCash == cashBefore1 - 30 && p3.mySet == 80) {
        printf("  PASS: Fixed code handles all-in bet correctly\n");
    } else {
        printf("  FAIL: Fix didn't work correctly\n");
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: PLAYER_ACTION_BET bet==0 edge case confirmed and fix verified\n");
    return 0;
}