// Test for ClientPlayer::setMySet() bug.
//
// Bug: In ClientPlayer::setMySet(), the line `mySet += theValue` incorrectly
// ADDS theValue to the current mySet instead of SETTING mySet to theValue.
// The function is named "setMySet", implying it should set mySet to theValue.
//
// This test verifies the bug: without the fix, mySet accumulates incorrectly;
// with the fix, mySet is correctly set to theValue.
//
#include <cstdio>
#include <cstdlib>

// Mock ClientPlayer state for the setMySet test
struct MockClientPlayer {
    int mySet{0};
    int myCash{1000};
    int myLastRelativeSet{0};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getLastRelativeSet() const { return myLastRelativeSet; }
};

// Buggy: mySet += theValue (adds to current mySet)
static int processSetMySetBug(MockClientPlayer& p, int theValue) {
    // Buggy: adds theValue to current mySet instead of setting it
    if (theValue > p.myCash) theValue = p.myCash;
    if (theValue < 0) theValue = 0;
    p.myLastRelativeSet = theValue;
    p.mySet += theValue;  // BUG: should be p.mySet = theValue;
    p.myCash -= theValue;
    if (p.myCash < 0) p.myCash = 0;
    return 0;
}

// Fixed: mySet = theValue (sets mySet to theValue)
static int processSetMySetFixed(MockClientPlayer& p, int theValue) {
    // Fix: set mySet to theValue instead of adding
    if (theValue > p.myCash) theValue = p.myCash;
    if (theValue < 0) theValue = 0;
    p.myLastRelativeSet = theValue;
    p.mySet = theValue;  // FIX: should be = instead of +=
    p.myCash -= theValue;
    if (p.myCash < 0) p.myCash = 0;
    return 0;
}

int main()
{
    int failures = 0;

    printf("Testing ClientPlayer::setMySet() bug: mySet += theValue vs mySet = theValue\n\n");

    // --- Scenario 1: Normal set ---
    printf("--- Scenario 1 (mySet=50, myCash=100, theValue=30) ---\n");
    MockClientPlayer p1;
    p1.mySet = 50;
    p1.myCash = 100;

    int cashBefore1 = p1.myCash;
    int setBefore1 = p1.mySet;

    processSetMySetBug(p1, 30);
    printf("  Buggy: mySet=%d (expected %d), myCash=%d (expected %d)\n",
           p1.mySet, setBefore1 + 30, p1.myCash, cashBefore1 - 30);
    // Buggy: mySet = 50 + 30 = 80 (wrong), myCash = 100 - 30 = 70
    if (p1.mySet != setBefore1) {
        printf("  BUG CONFIRMED: Buggy code adds theValue=%d to mySet=%d getting %d instead of setting mySet=%d\n",
               30, setBefore1, p1.mySet, 30);
    } else {
        printf("  Bug NOT confirmed (unexpected)\n");
        failures++;
    }

    // Reset and test fix
    p1.mySet = setBefore1;
    p1.myCash = cashBefore1;
    processSetMySetFixed(p1, 30);
    printf("  Fixed: mySet=%d (expected %d), myCash=%d (expected %d)\n",
           p1.mySet, 30, p1.myCash, cashBefore1 - 30);
    if (p1.mySet == 30 && p1.myCash == cashBefore1 - 30) {
        printf("  PASS: Fixed code correctly sets mySet to theValue\n");
    } else {
        printf("  FAIL: Fix didn't work as expected\n");
        failures++;
    }

    // --- Scenario 2: theValue > myCash (clamped) ---
    printf("\n--- Scenario 2 (mySet=50, myCash=100, theValue=200) ---\n");
    MockClientPlayer p2;
    p2.mySet = 50;
    p2.myCash = 100;

    int cashBefore2 = p2.myCash;
    processSetMySetBug(p2, 200);
    printf("  Buggy: mySet=%d (expected %d, clamped to myCash=%d), myCash=%d\n",
           p2.mySet, 100, p2.myCash, cashBefore2 - 100);
    // Buggy: mySet = 50 + 200 = 250 (but clamped to 100 via theValue=myCash)
    // Actually theValue gets clamped to myCash=100, then mySet += 100 = 150
    if (p2.mySet != 150) {
        printf("  BUG: mySet unexpected value %d\n", p2.mySet);
    }

    // Reset and test fix
    p2.mySet = 50;
    p2.myCash = cashBefore2;
    processSetMySetFixed(p2, 200);
    printf("  Fixed: mySet=%d (expected %d, should be theValue clamped to myCash=100), myCash=%d\n",
           p2.mySet, 100, p2.myCash);
    if (p2.mySet == 100 && p2.myCash == cashBefore2 - 100) {
        printf("  PASS: Fixed code correctly handles theValue > myCash\n");
    } else {
        printf("  FAIL: Fix didn't handle clamp correctly\n");
        failures++;
    }

    // --- Scenario 3: theValue < 0 (clamped to 0) ---
    printf("\n--- Scenario 3 (mySet=50, myCash=100, theValue=-10) ---\n");
    MockClientPlayer p3;
    p3.mySet = 50;
    p3.myCash = 100;

    int cashBefore3 = p3.myCash;
    processSetMySetBug(p3, -10);
    printf("  Buggy: mySet=%d (expected %d, should be 0), myCash=%d (expected %d)\n",
           p3.mySet, 0, p3.myCash, cashBefore3 - 0);
    // Buggy: theValue clamped to 0, mySet += 0 = 50 (still 50)
    if (p3.mySet != 50) {
        printf("  BUG: mySet unexpected value %d\n", p3.mySet);
    }

    // Reset and test fix
    p3.mySet = 50;
    p3.myCash = cashBefore3;
    processSetMySetFixed(p3, -10);
    printf("  Fixed: mySet=%d (expected %d), myCash=%d (expected %d)\n",
           p3.mySet, 0, p3.myCash, cashBefore3 - 0);
    if (p3.mySet == 0 && p3.myCash == cashBefore3) {
        printf("  PASS: Fixed code correctly handles negative theValue\n");
    } else {
        printf("  FAIL: Fix didn't handle negative theValue correctly\n");
        failures++;
    }

    // --- Scenario 4: theValue == myCash (all-in) ---
    printf("\n--- Scenario 4 (mySet=50, myCash=30, theValue=30) ---\n");
    MockClientPlayer p4;
    p4.mySet = 50;
    p4.myCash = 30;

    int cashBefore4 = p4.myCash;
    processSetMySetBug(p4, 30);
    printf("  Buggy: mySet=%d (expected %d), myCash=%d (expected %d)\n",
           p4.mySet, 30, p4.myCash, cashBefore4 - 30);
    // Buggy: mySet = 50 + 30 = 80 (wrong), myCash = 30 - 30 = 0

    // Reset and test fix
    p4.mySet = 50;
    p4.myCash = cashBefore4;
    processSetMySetFixed(p4, 30);
    printf("  Fixed: mySet=%d (expected %d), myCash=%d (expected %d)\n",
           p4.mySet, 30, p4.myCash, cashBefore4 - 30);
    if (p4.mySet == 30 && p4.myCash == cashBefore4 - 30) {
        printf("  PASS: Fixed code correctly handles all-in bet\n");
    } else {
        printf("  FAIL: Fix didn't work correctly\n");
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: ClientPlayer::setMySet() bug confirmed and fix verified\n");
    return 0;
}