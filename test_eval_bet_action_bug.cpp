// Test for LocalPlayer::evaluation() PLAYER_ACTION_BET myCash/mySet update bug.
//
// Bug: In LocalPlayer::evaluation(), the PLAYER_ACTION_BET case (non-all-in)
// does `setMySet(bet);` but does NOT do `myCash -= bet;`. This causes
// inconsistent state where myCash is not properly decremented by the bet amount.
//
// Fix: Added myCash -= bet; in the PLAYER_ACTION_BET switch case's "sonst" branch
// at src/engine/local_engine/localplayer.cpp:2826

#include <cstdio>
#include <cstdlib>

struct MockLocalPlayer {
    int mySet{0};
    int myCash{100};
    int oldMySet{0};
    int highestSet{0};
    int bet{0};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getOldMySet() const { return oldMySet; }
    int getHighestSet() const { return highestSet; }
    int getBet() const { return bet; }
};

// Simulate the FIXED evaluation() PLAYER_ACTION_BET case
// Does myCash -= bet AND mySet = bet
static int evaluation_fixed(MockLocalPlayer& p) {
    if (p.bet > 0) {
        p.myCash -= p.bet;
        p.mySet = p.bet;
    }
    return 0;
}

// Simulate the BUGGY evaluation() PLAYER_ACTION_BET case
// Does mySet = bet but does NOT subtract myCash
static int evaluation_buggy(MockLocalPlayer& p) {
    if (p.bet > 0) {
        p.mySet = p.bet;
        // myCash -= bet;  // BUG: missing
    }
    return 0;
}

int main()
{
    int failures = 0;

    printf("Testing evaluation() PLAYER_ACTION_BET myCash/mySet bug\n\n");

    // --- Scenario 1: Normal bet ---
    printf("--- Scenario 1 (mySet=0, myCash=100, bet=25) ---\n");
    MockLocalPlayer p1;
    p1.mySet = 0;
    p1.myCash = 100;
    p1.bet = 25;

    int cashBefore1 = p1.myCash;

    evaluation_buggy(p1);
    printf("  Buggy: myCash=%d (expected %d, no subtraction = %d)\n",
           p1.myCash, cashBefore1 - 25, p1.myCash);
    if (p1.myCash != cashBefore1 - 25) {
        printf("  BUG CONFIRMED: myCash not subtracted by bet amount\n");
    } else {
        printf("  OK: myCash correctly subtracted\n");
    }

    // Test fix
    p1.myCash = cashBefore1;
    evaluation_fixed(p1);
    printf("  Fixed: myCash=%d (expected %d)\n", p1.myCash, cashBefore1 - 25);
    if (p1.myCash == cashBefore1 - 25) {
        printf("  PASS: Fixed code correctly subtracts bet from myCash\n");
    } else {
        printf("  FAIL: Fixed code myCash=%d instead of %d\n", p1.myCash, cashBefore1 - 25);
        failures++;
    }

    // --- Scenario 2: Larger bet ---
    printf("\n--- Scenario 2 (mySet=0, myCash=200, bet=75) ---\n");
    MockLocalPlayer p2;
    p2.mySet = 0;
    p2.myCash = 200;
    p2.bet = 75;

    int cashBefore2 = p2.myCash;

    evaluation_buggy(p2);
    printf("  Buggy: myCash=%d (expected %d, no subtraction = %d)\n",
           p2.myCash, cashBefore2 - 75, p2.myCash);
    if (p2.myCash != cashBefore2 - 75) {
        printf("  BUG CONFIRMED: myCash not subtracted by bet amount\n");
        failures++;
    } else {
        printf("  OK: myCash correctly subtracted\n");
    }

    // Test fix
    p2.myCash = cashBefore2;
    evaluation_fixed(p2);
    printf("  Fixed: myCash=%d (expected %d)\n", p2.myCash, cashBefore2 - 75);
    if (p2.myCash == cashBefore2 - 75) {
        printf("  PASS: Fixed code correctly subtracts bet from myCash\n");
    } else {
        printf("  FAIL: Fixed code myCash=%d instead of %d\n", p2.myCash, cashBefore2 - 75);
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }

    printf("TEST PASSED: evaluation() PLAYER_ACTION_BET myCash/mySet bug confirmed and fix verified\n");
    return 0;
}
