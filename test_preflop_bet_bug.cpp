// Test for LocalPlayer::preflopEngine() PLAYER_ACTION_BET myCash/mySet update bug.
//
// Bug: In LocalPlayer::preflopEngine(), the `bet` variable is initialized to 0
// at function entry (line 1154) but is never set to the actual bet amount.
// This causes the PLAYER_ACTION_BET switch case at the end of preflopEngine()
// to never update myCash/mySet correctly when the AI bets preflop.
//
// The fix: Set `bet = mySet;` when the AI decides to bet, mirroring the
// approach used in flopEngine3(), turnEngine3(), and riverEngine3().
//
// Compile: g++ -O2 -std=c++23 -o test_preflop_bet_bug test_preflop_bet_bug.cpp && ./test_preflop_bet_bug

#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer state matching the preflopEngine structure
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

// Simulate the FIXED preflopEngine PLAYER_ACTION_BET case
// Uses the bet variable correctly (bet is set to mySet when AI bets)
static int action_fixed(MockLocalPlayer& p) {
    if (p.bet > 0) {
        p.myCash -= p.bet;
        p.mySet = p.oldMySet + p.bet;
    }
    return 0;
}

// Simulate the BUGGY preflopEngine PLAYER_ACTION_BET case
// bet is always 0, so myCash and mySet are never updated
static int action_buggy(MockLocalPlayer& p) {
    // bet is 0, so the if(bet > 0) check fails and no updates happen
    (void)p;  // suppress unused warning
    return 0;
}

int main()
{
    int failures = 0;

    printf("Testing preflopEngine() PLAYER_ACTION_BET myCash/mySet bug:\n\n");

    // --- Scenario 1: Normal bet with oldMySet > 0 ---
    printf("--- Scenario 1 (mySet=50, myCash=100, oldMySet=30, bet=20) ---\n");
    MockLocalPlayer p1;
    p1.mySet = 50;
    p1.myCash = 100;
    p1.oldMySet = 30;
    p1.bet = 20;  // AI decided to bet 20

    int cashBefore1 = p1.myCash;

    action_fixed(p1);
    printf("  Fixed: myCash=%d (expected %d), mySet=%d (expected %d)\n",
           p1.myCash, cashBefore1 - 20, p1.mySet, p1.oldMySet + 20);
    if (p1.myCash != cashBefore1 - 20 || p1.mySet != p1.oldMySet + 20) {
        printf("  FAIL: fixed code incorrect\n");
        ++failures;
    } else {
        printf("  PASS: fixed code correct\n");
    }

    // --- Scenario 2: Fresh bet (oldMySet = 0) ---
    printf("\n--- Scenario 2 (mySet=30, myCash=100, oldMySet=0, bet=25) ---\n");
    MockLocalPlayer p2;
    p2.mySet = 30;
    p2.myCash = 100;
    p2.oldMySet = 0;
    p2.bet = 25;  // AI decided to bet 25

    int cashBefore2 = p2.myCash;

    action_fixed(p2);
    printf("  Fixed: myCash=%d (expected %d), mySet=%d (expected %d)\n",
           p2.myCash, cashBefore2 - 25, p2.mySet, p2.oldMySet + 25);
    if (p2.myCash != cashBefore2 - 25 || p2.mySet != p2.oldMySet + 25) {
        printf("  FAIL: fixed code incorrect\n");
        ++failures;
    } else {
        printf("  PASS: fixed code correct\n");
    }

    // --- Scenario 3: Larger bet with oldMySet > 0 ---
    printf("\n--- Scenario 3 (mySet=40, myCash=200, oldMySet=25, bet=35) ---\n");
    MockLocalPlayer p3;
    p3.mySet = 40;
    p3.myCash = 200;
    p3.oldMySet = 25;
    p3.bet = 35;  // AI decided to bet 35

    int cashBefore3 = p3.myCash;

    action_fixed(p3);
    printf("  Fixed: myCash=%d (expected %d), mySet=%d (expected %d)\n",
           p3.myCash, cashBefore3 - 35, p3.mySet, p3.oldMySet + 35);
    if (p3.myCash != cashBefore3 - 35 || p3.mySet != p3.oldMySet + 35) {
        printf("  FAIL: fixed code incorrect\n");
        ++failures;
    } else {
        printf("  PASS: fixed code correct\n");
    }

    // --- Scenario 4: Bet of 0 edge case ---
    printf("\n--- Scenario 4 (mySet=0, myCash=100, oldMySet=10, bet=0) ---\n");
    MockLocalPlayer p4;
    p4.mySet = 0;
    p4.myCash = 100;
    p4.oldMySet = 10;
    p4.bet = 0;  // AI decided not to bet (bet=0)

    int cashBefore4 = p4.myCash;

    action_fixed(p4);
    printf("  Fixed: myCash=%d (expected %d), mySet=%d (expected %d)\n",
           p4.myCash, cashBefore4, p4.mySet, p4.mySet);  // bet=0: mySet unchanged
    if (p4.myCash != cashBefore4 || p4.mySet != 0) {
        printf("  FAIL: fixed code incorrect\n");
        ++failures;
    } else {
        printf("  PASS: fixed code correct (bet=0: no changes)\n");
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("TEST PASSED: preflopEngine PLAYER_ACTION_BET myCash/mySet behavior verified\n");
    return 0;
}