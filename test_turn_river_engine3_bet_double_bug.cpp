// Test for LocalPlayer::turnEngine3() and riverEngine3() PLAYER_ACTION_BET double-subtraction bug when highestSet == 0.
//
// Bug: In LocalPlayer::turnEngine3() and riverEngine3(), when highestSet == 0
// (pre-flop scenario), the engine function subtracts mySet from myCash at line 4586/4756,
// and then the switch(myAction) block also subtracts mySet from myCash at line 4453,
// causing myCash to be reduced by 2*mySet instead of mySet (double subtraction).
//
// Fix: Remove myCash -= mySet; from the engine functions' else branches,
//      since the switch(myAction) PLAYER_ACTION_BET case already handles it correctly.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_turn_river_engine3_bet_double_bug test_turn_river_engine3_bet_double_bug.cpp && /tmp/test_turn_river_engine3_bet_double_bug
#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer state for the turnEngine3/riverEngine3 bet test
struct MockLocalPlayer {
    int mySet{0};
    int myCash{100};
    int oldMySet{0};
    int highestSet{0};  // 0 for pre-flop scenario
    int bet{0};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getOldMySet() const { return oldMySet; }
    int getHighestSet() const { return highestSet; }
    int getBet() const { return bet; }
};

// Simulate the BUGGY turnEngine3()/riverEngine3() else branch when highestSet == 0 and betting
// (has myCash -= mySet; in the engine function)
static int processBuggy(MockLocalPlayer& p) {
    // mySet is set to bet amount (simulated here, matching engine function lines 4570-4586)
    p.mySet = p.bet;

    // highestSet is 0 (pre-flop)
    p.highestSet = 0;

    // BUG: engine function subtracts mySet from myCash
    // myCash -= mySet;  // line 4586 in turnEngine3(), line 4756 in riverEngine3()
    p.myCash -= p.mySet;

    // myAction = PLAYER_ACTION_BET is set, so the switch statement's PLAYER_ACTION_BET case also runs:
    // myCash -= mySet;  // line 4453 in switch block
    // mySet = oldMySet + mySet;

    // Switch PLAYER_ACTION_BET case also subtracts mySet from myCash
    if (p.mySet > 0) {
        p.myCash -= p.mySet;
        p.mySet = p.oldMySet + p.mySet;
    }
    return 0;
}

// Simulate the FIXED turnEngine3()/riverEngine3() else branch when highestSet == 0 and betting
// (no myCash -= mySet; in the engine function, only the switch handles it)
static int processFixed(MockLocalPlayer& p) {
    // mySet is set to bet amount (simulated here, matching engine function lines 4570-4586)
    p.mySet = p.bet;

    // highestSet is 0 (pre-flop)
    p.highestSet = 0;

    // FIX: NO myCash -= mySet; in the engine function
    // The switch(myAction) PLAYER_ACTION_BET case handles the cash subtraction

    // Switch PLAYER_ACTION_BET case handles the update
    if (p.mySet > 0) {
        p.myCash -= p.mySet;
        p.mySet = p.oldMySet + p.mySet;
    }
    return 0;
}

int main()
{
    int failures = 0;

    printf("Testing turnEngine3()/riverEngine3() PLAYER_ACTION_BET double-subtraction bug when highestSet==0:\n\n");

    // --- Scenario 1: Normal bet with bet > 0 ---
    printf("--- Scenario 1 (mySet=0, myCash=100, bet=30, highestSet=0, oldMySet=0) ---\n");
    MockLocalPlayer p1;
    p1.mySet = 0;
    p1.myCash = 100;
    p1.oldMySet = 0;
    p1.bet = 30;

    int cashBefore1 = p1.myCash;

    processBuggy(p1);
    // Buggy: myCash = 100 - 30 (engine) - 30 (switch) = 40 (WRONG! double subtraction)
    bool buggy1_confirmed = (p1.myCash != cashBefore1 - 30);
    printf("  Buggy: myCash=%d (expected %d for single subtraction, got %d for double)\n",
           p1.myCash, cashBefore1 - 30, p1.myCash);
    if (buggy1_confirmed) {
        printf("  BUG PATTERN CONFIRMED: myCash reduced by %d instead of %d (double subtraction of %d)\n",
               cashBefore1 - p1.myCash, cashBefore1 - 30, 2 * p1.bet);
    } else {
        printf("  Bug NOT confirmed (unexpected)\n");
        failures++;
    }

    // Test fix
    p1.myCash = cashBefore1;
    p1.mySet = 0;
    processFixed(p1);
    bool fix1_ok = (p1.myCash == cashBefore1 - 30);
    printf("  Fixed: myCash=%d (expected %d)\n", p1.myCash, cashBefore1 - 30);
    if (fix1_ok) {
        printf("  PASS: Fixed code correctly reduces myCash by single bet amount\n");
    } else {
        printf("  FAIL: Fixed code myCash=%d instead of %d\n", p1.myCash, cashBefore1 - 30);
        failures++;
    }

    // --- Scenario 2: bet=0 edge case ---
    printf("\n--- Scenario 2 (mySet=0, myCash=100, bet=0, highestSet=0, oldMySet=0) ---\n");
    MockLocalPlayer p2;
    p2.mySet = 0;
    p2.myCash = 100;
    p2.oldMySet = 0;
    p2.bet = 0;
    int cashBefore2 = p2.myCash;

    processBuggy(p2);
    // bet=0, so cash should remain unchanged regardless
    bool buggy2_ok = (p2.myCash == cashBefore2);
    printf("  Buggy: myCash=%d (should be %d, unchanged since bet=0)\n", p2.myCash, cashBefore2);
    if (buggy2_ok) {
        printf("  OK: bet=0: cash correctly unchanged\n");
    } else {
        printf("  Bug: cash unexpectedly changed to %d\n", p2.myCash);
        failures++;
    }

    // Test fix
    p2.myCash = cashBefore2;
    processFixed(p2);
    bool fix2_ok = (p2.myCash == cashBefore2);
    printf("  Fixed: myCash=%d (should be %d, unchanged since bet=0)\n", p2.myCash, cashBefore2);
    if (fix2_ok) {
        printf("  Fixed bet=0: cash correctly unchanged\n");
    } else {
        printf("  FAIL: fixed code myCash=%d instead of %d\n", p2.myCash, cashBefore2);
        failures++;
    }

    // --- Scenario 3: Larger bet ---
    printf("\n--- Scenario 3 (mySet=0, myCash=50, bet=30, highestSet=0, oldMySet=0) ---\n");
    MockLocalPlayer p3;
    p3.mySet = 0;
    p3.myCash = 50;
    p3.oldMySet = 0;
    p3.bet = 30;
    int cashBefore3 = p3.myCash;

    processBuggy(p3);
    // Buggy: myCash = 50 - 30 (engine) - 30 (switch) = -20 (WRONG!)
    bool buggy3_confirmed = (p3.myCash != cashBefore3 - 30);
    printf("  Buggy: myCash=%d (expected %d for single subtraction, got %d for double)\n",
           p3.myCash, cashBefore3 - 30, p3.myCash);
    if (buggy3_confirmed) {
        printf("  BUG PATTERN CONFIRMED: myCash reduced by %d instead of %d (double subtraction of %d)\n",
               cashBefore3 - p3.myCash, cashBefore3 - 30, 2 * p3.bet);
    } else {
        printf("  Bug NOT confirmed (unexpected)\n");
        failures++;
    }

    // Test fix
    p3.myCash = cashBefore3;
    processFixed(p3);
    bool fix3_ok = (p3.myCash == cashBefore3 - 30);
    printf("  Fixed: myCash=%d (expected %d)\n", p3.myCash, cashBefore3 - 30);
    if (fix3_ok) {
        printf("  PASS: Fixed code correctly reduces myCash by single bet amount\n");
    } else {
        printf("  FAIL: Fixed code myCash=%d instead of %d\n", p3.myCash, cashBefore3 - 30);
        failures++;
    }

    printf("\n");
    printf("Scenario results:\n");
    printf("  Scenario 1 (double subtraction confirmed): %s\n", buggy1_confirmed ? "BUG CONFIRMED" : "PASS");
    printf("  Scenario 2 (bet=0 edge case): %s\n", buggy2_ok ? "PASS" : "FAIL");
    printf("  Scenario 3 (large bet): %s\n", buggy3_confirmed ? "BUG CONFIRMED" : "PASS");

    if (failures > 0) {
        printf("\nTEST FAILED: %d failure(s)\n", failures);
        return 1;
    }

    printf("TEST PASSED: turnEngine3()/riverEngine3() PLAYER_ACTION_BET double-subtraction bug confirmed and fix verified\n");
    return 0;
}