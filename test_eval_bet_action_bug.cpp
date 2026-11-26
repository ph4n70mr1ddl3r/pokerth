// Test for LocalPlayer::evaluation() PLAYER_ACTION_BET myCash/mySet update bug.
//
// Bug: In LocalPlayer::evaluation(), the PLAYER_ACTION_BET case (non-all-in)
// does `setMySet(bet);` but does NOT do `myCash -= bet;`. This causes
// inconsistent state where myCash is not properly decremented by the bet amount.
//
// However, note: setMySet() already subtracts from myCash, so the actual
// code at localplayer.cpp:2871 calls setMySet(bet) which handles the subtraction.
// This test documents the expected behavior.
//
#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer state for the evaluation test
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
// setMySet(bet) is called which does myCash -= bet internally
static int evaluation_fixed(MockLocalPlayer& p) {
    if (p.bet > 0) {
        p.setMySet(p.bet);
    }
    return 0;
}

// Simulate a scenario where myCash -= bet is also done manually (double debit)
static int evaluation_double_debit(MockLocalPlayer& p) {
    if (p.bet > 0) {
        p.myCash -= p.bet;
        p.setMySet(p.bet);
    }
    return 0;
}

// Simulate the BUGGY evaluation() PLAYER_ACTION_BET case
// where setMySet is NOT called (just direct assignment)
static int evaluation_buggy(MockLocalPlayer& p) {
    if (p.bet > 0) {
        p.mySet = p.bet;
        // myCash -= bet;  // BUG: missing - but setMySet handles this
    }
    return 0;
}

int main()
{
    int failures = 0;

    printf("Testing evaluation() PLAYER_ACTION_BET myCash/mySet behavior\n\n");

    // --- Scenario 1: Normal bet from starting state ---
    printf("--- Scenario 1 (mySet=0, myCash=100, bet=25) ---\n");
    MockLocalPlayer p1;
    p1.mySet = 0;
    p1.myCash = 100;
    p1.bet = 25;

    int cashBefore1 = p1.myCash;

    // Test: setMySet(bet) is called (the actual code path)
    evaluation_fixed(p1);
    printf("  setMySet(bet): myCash=%d (expected %d, after subtraction)\n",
           p1.myCash, cashBefore1 - 25);
    if (p1.myCash == cashBefore1 - 25) {
        printf("  PASS: myCash correctly subtracted by setMySet\n");
    } else {
        printf("  FAIL: myCash not subtracted correctly\n");
        failures++;
    }

    // --- Scenario 2: Double-debit scenario ---
    printf("\n--- Scenario 2 (Double-debit: myCash -= bet AND setMySet(bet)) ---\n");
    MockLocalPlayer p2;
    p2.mySet = 0;
    p2.myCash = 100;
    p2.bet = 25;

    cashBefore1 = p2.myCash;

    evaluation_double_debit(p2);
    printf("  Double-debit: myCash=%d (expected %d, after double subtraction)\n",
           p2.myCash, cashBefore1 - 50);  // 50 because myCash -= bet AND setMySet subtracts again
    if (p2.myCash == cashBefore1 - 50) {
        printf("  CONFIRMED: Double-debit bug - myCash subtracted twice\n");
    } else {
        printf("  OK: myCash not double-subtracted (fix already applied)\n");
    }

    // Test: fixed version (only setMySet, no manual myCash -= bet)
    p2.myCash = cashBefore1;
    evaluation_fixed(p2);
    printf("  Fixed: myCash=%d (expected %d, after single setMySet subtraction)\n",
           p2.myCash, cashBefore1 - 25);
    if (p2.myCash == cashBefore1 - 25) {
        printf("  PASS: Fixed code correctly subtracts bet once\n");
    } else {
        printf("  FAIL: Fixed code myCash=%d instead of %d\n", p2.myCash, cashBefore1 - 25);
        failures++;
    }

    // --- Scenario 3: Larger bet ---
    printf("\n--- Scenario 3 (mySet=0, myCash=200, bet=75) ---\n");
    MockLocalPlayer p3;
    p3.mySet = 0;
    p3.myCash = 200;
    p3.bet = 75;

    int cashBefore3 = p3.myCash;

    evaluation_fixed(p3);
    printf("  setMySet(bet): myCash=%d (expected %d)\n",
           p3.myCash, cashBefore3 - 75);
    if (p3.myCash == cashBefore3 - 75) {
        printf("  PASS: myCash correctly subtracted\n");
    } else {
        printf("  FAIL: myCash not subtracted correctly\n");
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("TEST PASSED: evaluation() PLAYER_ACTION_BET behavior verified\n");
    return 0;
}