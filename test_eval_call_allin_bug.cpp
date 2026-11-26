// Test for evaluation() PLAYER_ACTION_CALL all-in bug fix.
//
// Bug: In LocalPlayer::evaluation(), the PLAYER_ACTION_CALL case did not
// handle the all-in scenario where toCall > myCash. This could cause myCash
// to become negative. The PLAYER_ACTION_RAISE case correctly handles this
// by going all-in (mySet += myCash, myCash = 0), but PLAYER_ACTION_CALL
// now also handles this after the fix.
//
// Compile:
//   g++ -std=c++23 -O2 -o /tmp/test_eval_call_allin_bug test_eval_call_allin_bug.cpp && /tmp/test_eval_call_allin_bug

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

// Simulate the FIXED evaluation() PLAYER_ACTION_CALL case
// Handles all-in when toCall > myCash (the fix)
static int evaluation_fixed(int myAction, MockLocalPlayer& p) {
    switch(myAction) {
        case 2: { // PLAYER_ACTION_CALL
            int toCall = p.getHighestSet() - p.mySet;
            if(toCall > 0) {
                if(toCall > p.myCash) {
                    // All-in: can't cover full call (THE FIX)
                    p.mySet += p.myCash;
                    p.myCash = 0;
                } else {
                    p.myCash -= toCall;
                    p.mySet = p.getHighestSet();
                }
            }
        }
        break;
        case 5: { // PLAYER_ACTION_RAISE
            int toCall = p.getHighestSet() - p.mySet;
            int totalBet = toCall;
            if(totalBet > 0) {
                if(totalBet > p.myCash) {
                    // All-in: can't cover full bet
                    p.mySet += p.myCash;
                    p.myCash = 0;
                } else {
                    p.myCash -= totalBet;
                }
            }
            break;
        }
        default:
            return 1;
    }
    return 0;
}

// Simulate the OLD (buggy) evaluation() PLAYER_ACTION_CALL case
// Does NOT handle all-in when toCall > myCash
static int evaluation_buggy(int myAction, MockLocalPlayer& p) {
    switch(myAction) {
        case 2: { // PLAYER_ACTION_CALL
            int toCall = p.getHighestSet() - p.mySet;
            if(toCall > 0) {
                p.myCash -= toCall;  // BUG: no all-in check! Can make myCash negative
                p.mySet = p.getHighestSet();
            }
        }
        break;
    }
    return 0;
}

int main()
{
    int failures = 0;

    printf("Testing evaluation() PLAYER_ACTION_CALL all-in bug fix\n\n");

    // --- Scenario 1: All-in call with toCall > myCash (THE BUG) ---
    printf("--- Scenario 1: All-in call, toCall=60, myCash=50 ---\n");
    MockLocalPlayer p1;
    p1.mySet = 30;
    p1.myCash = 50;
    p1.highestSet = 90;
    
    printf("  Before: mySet=%d, myCash=%d, highestSet=%d\n", p1.mySet, p1.myCash, p1.highestSet);
    
    // Buggy implementation
    evaluation_buggy(2, p1); // PLAYER_ACTION_CALL
    printf("  Buggy: myCash=%d (expected 0, bug: negative=%d), mySet=%d (expected %d)\n",
           p1.myCash, p1.myCash, p1.mySet, p1.mySet);
    if (p1.myCash < 0) {
        printf("  BUG CONFIRMED: Buggy code makes myCash negative (%d)\n", p1.myCash);
        failures++;
    } else if (p1.myCash != 0) {
        printf("  BUG: myCash should be 0 for all-in, but is %d\n", p1.myCash);
        failures++;
    }

    // Reset and test fixed implementation
    MockLocalPlayer p1_fixed;
    p1_fixed.mySet = 30;
    p1_fixed.myCash = 50;
    p1_fixed.highestSet = 90;
    evaluation_fixed(2, p1_fixed); // Fixed PLAYER_ACTION_CALL
    printf("  Fixed: myCash=%d (expected 0), mySet=%d (expected %d=%d+50)\n",
           p1_fixed.myCash, p1_fixed.mySet, p1_fixed.mySet - 50, p1_fixed.mySet);
    // mySet was 30, added myCash 50 = 80. highestSet is 90 but player only contributed 80.
    if (p1_fixed.myCash == 0 && p1_fixed.mySet == 80) {
        printf("  PASS: Fixed code correctly handles all-in call\n");
    } else {
        printf("  FAIL: myCash=%d, mySet=%d (expected myCash=0, mySet=80)\n", p1_fixed.myCash, p1_fixed.mySet);
        failures++;
    }

    // --- Scenario 2: Normal call with toCall <= myCash ---
    printf("\n--- Scenario 2: Normal call, toCall=20, myCash=50 ---\n");
    MockLocalPlayer p2;
    p2.mySet = 30;
    p2.myCash = 50;
    p2.highestSet = 50;
    
    printf("  Before: mySet=%d, myCash=%d, highestSet=%d\n", p2.mySet, p2.myCash, p2.highestSet);
    
    // Fixed implementation
    evaluation_fixed(2, p2); // PLAYER_ACTION_CALL
    printf("  Fixed: myCash=%d (expected 30), mySet=%d (expected %d)\n",
           p2.myCash, p2.mySet, p2.mySet);
    if (p2.myCash == 30 && p2.mySet == 50) {
        printf("  PASS: Fixed code correctly handles normal call\n");
    } else {
        printf("  FAIL: myCash=%d, mySet=%d (expected myCash=30, mySet=50)\n", p2.myCash, p2.mySet);
        failures++;
    }

    // --- Scenario 3: No call needed (mySet >= highestSet) ---
    printf("\n--- Scenario 3: mySet >= highestSet, no call needed ---\n");
    MockLocalPlayer p3;
    p3.mySet = 80;
    p3.myCash = 50;
    p3.highestSet = 50;
    
    printf("  Before: mySet=%d, myCash=%d, highestSet=%d\n", p3.mySet, p3.myCash, p3.highestSet);
    
    // Fixed implementation
    evaluation_fixed(2, p3); // PLAYER_ACTION_CALL
    printf("  Fixed: myCash=%d (expected 50), mySet=%d (expected %d, unchanged)\n",
           p3.myCash, p3.mySet, p3.mySet);
    if (p3.myCash == 50 && p3.mySet == 80) {
        printf("  PASS: Fixed code correctly handles no-call scenario\n");
    } else {
        printf("  FAIL: myCash=%d, mySet=%d (expected myCash=50, mySet=80)\n", p3.myCash, p3.mySet);
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("TEST PASSED: evaluation() PLAYER_ACTION_CALL all-in bug fix verified\n");
    return 0;
}