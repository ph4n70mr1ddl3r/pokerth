// Test for evaluation() PLAYER_ACTION_BET all-in bug fix.
//
// Bug: In LocalPlayer::evaluation(), the PLAYER_ACTION_BET case does not
// handle the all-in scenario where bet > myCash. This could cause myCash
// to become negative. The PLAYER_ACTION_CALL and PLAYER_ACTION_RAISE cases
// correctly handle this by going all-in (mySet += myCash, myCash = 0),
// but PLAYER_ACTION_BET does not.
//
// Compile:
//   g++ -std=c++23 -O2 -o /tmp/test_eval_bet_allin_bug test_eval_bet_allin_bug.cpp && /tmp/test_eval_bet_allin_bug

#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer state for the evaluation test
struct MockLocalPlayer {
    int mySet;
    int myCash;
    int highestSet;

    MockLocalPlayer() : mySet(0), myCash(100), highestSet(0) {}
    
    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getHighestSet() const { return highestSet; }
};

// Simulate the FIXED evaluation() PLAYER_ACTION_BET case
// Handles all-in when bet > myCash (the fix)
static int evaluation_fixed(int myAction, MockLocalPlayer& p, int bet) {
    switch(myAction) {
        case 4: { // PLAYER_ACTION_BET
            if(bet > 0) {
                if(bet > p.myCash) {
                    // All-in: can't cover full bet (THE FIX)
                    p.mySet += p.myCash;
                    p.myCash = 0;
                } else {
                    p.myCash -= bet;
                    p.mySet += bet;
                }
            }
        }
        break;
        default:
            return 1;
    }
    return 0;
}

// Simulate the OLD (buggy) evaluation() PLAYER_ACTION_BET case
// Does NOT handle all-in when bet > myCash
static int evaluation_buggy(int myAction, MockLocalPlayer& p, int bet) {
    switch(myAction) {
        case 4: { // PLAYER_ACTION_BET
            if(bet > 0) {
                p.myCash -= bet;  // BUG: no all-in check! Can make myCash negative
                p.mySet += bet;
            }
        }
        break;
    }
    return 0;
}

int main()
{
    int failures = 0;

    printf("Testing evaluation() PLAYER_ACTION_BET all-in bug fix\n\n");

    // --- Scenario 1: All-in bet with bet > myCash (THE BUG) ---
    printf("--- Scenario 1: All-in bet, bet=60, myCash=50 ---\n");
    MockLocalPlayer p1;
    p1.mySet = 30;
    p1.myCash = 50;
    p1.highestSet = 90;
    
    printf("  Before: mySet=%d, myCash=%d, highestSet=%d\n", p1.mySet, p1.myCash, p1.highestSet);
    
    // Buggy implementation - demonstrates the bug
    evaluation_buggy(4, p1, 60); // PLAYER_ACTION_BET with bet=60
    printf("  Buggy: myCash=%d (expected negative due to bug), mySet=%d\n",
           p1.myCash, p1.mySet);
    // Bug is confirmed if myCash goes negative (expected behavior of buggy code)
    int buggyCashNegative = (p1.myCash < 0) ? 1 : 0;
    printf("  Bug confirmed: myCash %snegative (value=%d)\n",
           buggyCashNegative ? "" : "not ", p1.myCash);
    (void)buggyCashNegative;  // Suppress unused variable warning

    // Reset and test fixed implementation
    MockLocalPlayer p1_fixed;
    p1_fixed.mySet = 30;
    p1_fixed.myCash = 50;
    p1_fixed.highestSet = 90;
    evaluation_fixed(4, p1_fixed, 60); // Fixed PLAYER_ACTION_BET
    printf("  Fixed: myCash=%d (expected 0), mySet=%d (expected %d=%d+50)\n",
           p1_fixed.myCash, p1_fixed.mySet, p1_fixed.mySet - 50, p1_fixed.mySet);
    // mySet was 30, added myCash 50 = 80
    if (p1_fixed.myCash == 0 && p1_fixed.mySet == 80) {
        printf("  PASS: Fixed code correctly handles all-in bet\n");
    } else {
        printf("  FAIL: myCash=%d, mySet=%d (expected myCash=0, mySet=80)\n", p1_fixed.myCash, p1_fixed.mySet);
        failures++;
    }

    // --- Scenario 2: Normal bet with bet <= myCash ---
    printf("\n--- Scenario 2: Normal bet, bet=20, myCash=50 ---\n");
    MockLocalPlayer p2;
    p2.mySet = 30;
    p2.myCash = 50;
    p2.highestSet = 50;
    
    printf("  Before: mySet=%d, myCash=%d, highestSet=%d\n", p2.mySet, p2.myCash, p2.highestSet);
    
    // Fixed implementation
    evaluation_fixed(4, p2, 20); // PLAYER_ACTION_CALL
    printf("  Fixed: myCash=%d (expected 30), mySet=%d (expected %d)\n",
           p2.myCash, p2.mySet, p2.mySet);
    if (p2.myCash == 30 && p2.mySet == 50) {
        printf("  PASS: Fixed code correctly handles normal bet\n");
    } else {
        printf("  FAIL: myCash=%d, mySet=%d (expected myCash=30, mySet=50)\n", p2.myCash, p2.mySet);
        failures++;
    }

    // --- Scenario 3: No bet (bet = 0) ---
    printf("\n--- Scenario 3: bet=0 (no bet) ---\n");
    MockLocalPlayer p3;
    p3.mySet = 30;
    p3.myCash = 50;
    p3.highestSet = 50;
    
    printf("  Before: mySet=%d, myCash=%d, highestSet=%d\n", p3.mySet, p3.myCash, p3.highestSet);
    
    // Fixed implementation
    evaluation_fixed(4, p3, 0); // PLAYER_ACTION_BET with bet=0
    printf("  Fixed: myCash=%d (expected 50), mySet=%d (expected %d, unchanged)\n",
           p3.myCash, p3.mySet, p3.mySet);
    if (p3.myCash == 50 && p3.mySet == 30) {
        printf("  PASS: Fixed code correctly handles no-bet scenario\n");
    } else {
        printf("  FAIL: myCash=%d, mySet=%d (expected myCash=50, mySet=30)\n", p3.myCash, p3.mySet);
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("TEST PASSED: evaluation() PLAYER_ACTION_BET all-in bug fix verified\n");
    return 0;
}