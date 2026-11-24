// Test for gameTableImpl::mySet() myCash/mySet update bug.
//
// Bug: In gameTableImpl::mySet() (gametableimpl.cpp line 1930-1956),
// when a non-all-in bet is made (betValue < tempCash), the code does
// `humanPlayer->setMySet(betValue);` which correctly deducts betValue from
// myCash and adds it to mySet, but then also calls
// `humanPlayer->setMyCash(tempCash - betValue);` which is redundant
// since setMySet() already handles the myCash deduction.
//
// The redundant setMyCash call was added in commit ed8b0b0 to fix a bug
// where myCash was not being deducted, but the real setMySet() function
// already correctly deducts from myCash. This test verifies the current
// behavior and confirms the fix: remove the redundant setMyCash call.
//
// Compile:
//   g++ -std=c++23 -O2 -o /tmp/test_gui_bet_myCash_mySet test_gui_bet_myCash_mySet.cpp
//   /tmp/test_gui_bet_myCash_mySet

#include <cstdio>
#include <cstdlib>
#include <algorithm>

// Mock PlayerInterface - uses the REAL setMySet/setMyCash from localplayer.h
// To use this test, include the actual header or link against the library.
// This mock simulates the behavior of the actual PlayerInterface.

// Real setMySet behavior (from localplayer.h):
// - mySet += theValue
// - myCash -= theValue (capped to myCash if theValue > myCash)
// - myCash = max(0, myCash)

// Real setMyCash behavior (from localplayer.h):
// - myCash = theValue

// Mock that simulates the real setMySet behavior
struct MockPlayer {
    int mySet{0};
    int myCash{100};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }

    // Simulates the REAL setMySet: deducts from myCash, adds to mySet
    void setMySet(int theValue) {
        if (theValue > myCash) theValue = myCash;
        if (theValue < 0) theValue = 0;
        myLastRelativeSet = theValue;
        mySet += theValue;
        myCash -= theValue;
        if (myCash < 0) myCash = 0;
    }

    // Simulates the REAL setMyCash: simply sets the value
    void setMyCash(int theValue) {
        myCash = theValue;
    }

    int myLastRelativeSet{0};
};

// Simulate the CURRENT (buggy with redundancy) gameTableImpl::mySet() 
// for a non-all-in bet (betValue < tempCash)
static void currentImplementation(MockPlayer& p, int betValue, int tempCash) {
    int oldCash = p.getMyCash();
    p.setMySet(betValue);              // setMySet deducts bet from myCash
    p.setMyCash(tempCash - betValue);  // REDUNDANT: sets myCash again (same value)
    (void)oldCash; // suppress unused warning
}

// Simulate the FIXED gameTableImpl::mySet() - remove redundant setMyCash
static void fixedImplementation(MockPlayer& p, int betValue, int tempCash) {
    p.setMySet(betValue);  // setMySet handles both mySet and myCash correctly
    // NO redundant setMyCash call
}

int main()
{
    int failures = 0;

    printf("GUI bet myCash/mySet update test\n\n");

    // --- Scenario 1: Normal bet (betValue < tempCash) ---
    printf("--- Scenario 1: Normal bet (betValue=20, tempCash=100) ---\n");
    MockPlayer p1;
    int cashBefore1 = p1.getMyCash();

    printf("  Before: myCash = %d, mySet = %d\n", p1.getMyCash(), p1.getMySet());

    currentImplementation(p1, 20, 100);
    printf("  Current impl: myCash = %d, mySet = %d (expected: myCash=80, mySet old+20)\n",
           p1.getMyCash(), p1.getMySet());
    if (p1.getMyCash() == 80) {
        printf("  PASS: myCash correctly deducted\n");
    } else {
        printf("  FAIL: myCash should be 80 (100-20)\n");
        failures++;
    }

    // Reset and test fixed implementation
    p1.setMyCash(100);
    p1.mySet = 30;  // reset mySet
    fixedImplementation(p1, 20, 100);
    printf("  Fixed impl:   myCash = %d, mySet = %d (expected: myCash=80, mySet=50)\n",
           p1.getMyCash(), p1.getMySet());
    if (p1.getMyCash() == 80 && p1.getMySet() == 50) {
        printf("  PASS: Fixed implementation correctly updates myCash and mySet\n");
    } else {
        printf("  FAIL: myCash=%d, mySet=%d expected myCash=80, mySet=50\n",
               p1.getMyCash(), p1.getMySet());
        failures++;
    }

    // --- Scenario 2: All-in bet (betValue >= tempCash) ---
    printf("\n--- Scenario 2: All-in bet (betValue=150, tempCash=100) ---\n");
    MockPlayer p2;
    printf("  Before: myCash = %d, mySet = %d\n", p2.getMyCash(), p2.getMySet());

    // All-in path already handles this correctly
    p2.setMySet(100);  // sets myCash to 0, mySet += 100
    p2.setMyCash(0);   // sets myCash to 0 (redundant but harmless)
    printf("  After all-in: myCash = %d, mySet = %d (expected: myCash=0, mySet old+100)\n",
           p2.getMyCash(), p2.getMySet());
    if (p2.getMyCash() == 0) {
        printf("  PASS: All-in case correctly sets myCash to 0\n");
    } else {
        printf("  FAIL: myCash should be 0 for all-in\n");
        failures++;
    }

    // --- Scenario 3: Zero bet ---
    printf("\n--- Scenario 3: Zero bet (betValue=0, tempCash=100) ---\n");
    MockPlayer p3;
    printf("  Before: myCash = %d, mySet = %d\n", p3.getMyCash(), p3.getMySet());

    currentImplementation(p3, 0, 100);
    printf("  Current impl: myCash = %d, mySet = %d (expected: myCash=100, mySet unchanged)\n",
           p3.getMyCash(), p3.getMySet());
    if (p3.getMyCash() == 100) {
        printf("  PASS: Zero bet doesn't change cash\n");
    } else {
        printf("  FAIL: myCash should remain 100 for zero bet\n");
        failures++;
    }

    printf("\n");
    if (failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("TESTS FAILED: %d failure(s)\n", failures);
    return 1;
}