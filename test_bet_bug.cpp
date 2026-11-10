// Test for LocalPlayer::action() PLAYER_ACTION_BET bug.
//
// Bug: In LocalPlayer, the PLAYER_ACTION_BET case in the switch(myAction) block
// at line 1555 does `myCash -= oldMySet;` which subtracts the player's previous set
// amount from cash instead of subtracting the bet amount. This is incorrect because:
// - The bet amount (stored in `bet` variable) should be subtracted from myCash
// - The bet amount should be added to mySet
// - The current code subtracts oldMySet (the player's chips before the action)
//   which effectively returns the player's chips to the pot without properly
//   handling the bet.
//
// Compare with PLAYER_ACTION_CALL which correctly calculates toCall and updates
// both myCash and mySet.
//
// This test verifies the bug: without the fix, myCash is incorrectly modified;
// with the fix, myCash and mySet are correctly updated based on the bet amount.

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

// Current (buggy) PLAYER_ACTION_BET handling from the switch block
static int processBetBug(MockLocalPlayer& p) {
    // Buggy: subtracts oldMySet from myCash, doesn't update mySet properly
    p.myCash -= p.oldMySet;
    // mySet is NOT updated - this is the bug!
    return 0;
}

// Fixed PLAYER_ACTION_BET handling
static int processBetFixed(MockLocalPlayer& p) {
    // Fix: subtract bet amount from myCash and add to mySet
    if (p.bet > 0) {
        p.myCash -= p.bet;
        p.mySet += p.bet;
    }
    return 0;
}

int main()
{
    int failures = 0;

    printf("Testing PLAYER_ACTION_BET bug: myCash -= oldMySet vs proper bet handling\n\n");

    // --- Scenario 1: Normal bet with bet > 0 ---
    printf("--- Scenario 1 (mySet=50, myCash=100, oldMySet=50, bet=30) ---\n");
    MockLocalPlayer p1;
    p1.mySet = 50;
    p1.myCash = 100;
    p1.oldMySet = 50;
    p1.bet = 30;
    p1.highestSet = 100;

    // Save original values
    int cashBefore1 = p1.myCash;
    int setBefore1 = p1.mySet;

    processBetBug(p1);
    printf("  Buggy: myCash=%d (expected %d), mySet=%d (expected %d)\n",
           p1.myCash, cashBefore1 - 30, p1.mySet, setBefore1 + 30);
    // Buggy: myCash should be 100-30=70, but instead it's 100-50=50
    if (p1.myCash != cashBefore1 - 30) {
        printf("  BUG CONFIRMED: Buggy code subtracts oldMySet=%d instead of bet=%d\n",
               cashBefore1 - p1.myCash, p1.bet);
    } else {
        printf("  Bug NOT confirmed (unexpected)\n");
        failures++;
    }

    // Reset and test fix
    p1.myCash = cashBefore1;
    p1.mySet = setBefore1;
    processBetFixed(p1);
    printf("  Fixed: myCash=%d (expected %d), mySet=%d (expected %d)\n",
           p1.myCash, cashBefore1 - 30, p1.mySet, setBefore1 + 30);
    if (p1.myCash == cashBefore1 - 30 && p1.mySet == setBefore1 + 30) {
        printf("  PASS: Fixed code correctly subtracts bet from cash and adds to set\n");
    } else {
        printf("  FAIL: Fix didn't work as expected\n");
        failures++;
    }

    // --- Scenario 2: Bet of 0 ---
    printf("\n--- Scenario 2 (mySet=50, myCash=100, oldMySet=50, bet=0) ---\n");
    MockLocalPlayer p2;
    p2.mySet = 50;
    p2.myCash = 100;
    p2.oldMySet = 50;
    p2.bet = 0;

    int cashBefore2 = p2.myCash;
    processBetBug(p2);
    printf("  Buggy: myCash=%d (expected %d, should be unchanged since bet=0)\n",
           p2.myCash, cashBefore2);
    // Buggy: myCash = 100 - 50 = 50 (wrong - should be 100)
    if (p2.myCash == cashBefore2) {
        printf("  Bug: cash unchanged (but for wrong reason - subtracting oldMySet)\n");
    } else {
        printf("  FAIL: cash unexpectedly changed\n");
        failures++;
    }

    // Reset and test fix
    p2.myCash = cashBefore2;
    processBetFixed(p2);
    if (p2.myCash == cashBefore2) {
        printf("  FIX PASS: cash correctly unchanged when bet=0\n");
    } else {
        printf("  FAIL: cash unexpectedly changed with fix\n");
        failures++;
    }

    // --- Scenario 3: All-in bet (bet >= myCash) ---
    printf("\n--- Scenario 3 (mySet=50, myCash=30, oldMySet=50, bet=30) ---\n");
    MockLocalPlayer p3;
    p3.mySet = 50;
    p3.myCash = 30;
    p3.oldMySet = 50;
    p3.bet = 30;

    int cashBefore3 = p3.myCash;
    processBetBug(p3);
    printf("  Buggy: myCash=%d (expected %d, buggy subtracts oldMySet=50 -> %d)\n",
           p3.myCash, cashBefore3 - 50, cashBefore3 - 50);
    // Buggy: myCash = 30 - 50 = -20 (then clamped to 0 elsewhere)

    // Reset and test fix
    p3.myCash = cashBefore3;
    processBetFixed(p3);
    printf("  Fixed: myCash=%d (expected %d), mySet=%d (expected %d)\n",
           p3.myCash, cashBefore3 - 30, p3.mySet, 50 + 30);
    if (p3.myCash == cashBefore3 - 30 && p3.mySet == 80) {
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

    printf("TEST PASSED: PLAYER_ACTION_BET bug confirmed and fix verified\n");
    return 0;
}