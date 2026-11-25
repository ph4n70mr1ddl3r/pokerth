// Test for ClientPlayer::setMySet() myCash/mySet update bug.
//
// Bug: In ClientPlayer::setMySet(), only myLastRelativeSet is set but mySet
// and myCash are NOT updated. This is inconsistent with LocalPlayer::setMySet()
// which properly deducts from myCash and adds to mySet.
//
// Compile:
//   g++ -std=c++23 -O2 -o /tmp/test_clientplayer_setmyset_bug test_clientplayer_setmyset_bug.cpp && /tmp/test_clientplayer_setmyset_bug

#include <cstdio>
#include <cstdlib>

// Mock ClientPlayer state matching the actual code structure
struct ClientPlayer {
    int mySet;
    int myCash;
    int myLastRelativeSet;
    
    ClientPlayer() : mySet(0), myCash(100), myLastRelativeSet(0) {}
    
    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getMyLastRelativeSet() const { return myLastRelativeSet; }
    
    // BUGGY: only sets myLastRelativeSet, doesn't update mySet/myCash
    void setMySet_buggy(int theValue) {
        myLastRelativeSet = theValue;
        // BUG: mySet and myCash NOT updated
    }
    
    // FIXED: properly updates mySet and myCash, matching LocalPlayer::setMySet
    void setMySet_fixed(int theValue) {
        if (theValue > myCash) theValue = myCash;
        if (theValue < 0) theValue = 0;
        myLastRelativeSet = theValue;
        mySet += theValue;
        myCash -= theValue;
        if (myCash < 0) myCash = 0;
    }
};

int main()
{
    int failures = 0;

    printf("Testing ClientPlayer::setMySet() myCash/mySet update bug\n\n");

    // --- Scenario 1: Normal bet (theValue < myCash) ---
    printf("--- Scenario 1 (theValue=20, myCash=100, mySet=30) ---\n");
    ClientPlayer p1;
    p1.mySet = 30;
    p1.myCash = 100;
    
    int cashBefore1 = p1.myCash;

    // Buggy implementation
    p1.setMySet_buggy(20);
    printf("  Buggy: myCash=%d (expected %d, bug: not deducted), mySet=%d (expected %d=%d+20), myLastRelativeSet=%d\n",
           p1.myCash, cashBefore1 - 20, p1.mySet, 30 + 20, p1.mySet - 30, p1.myLastRelativeSet);
    if (p1.myCash == cashBefore1 && p1.mySet == 30) {
        printf("  BUG CONFIRMED: Buggy code doesn't update myCash or mySet\n");
    } else {
        printf("  Bug NOT confirmed (unexpected)\n");
        failures++;
    }

    // Reset and test fixed implementation
    p1.myCash = 100;
    p1.mySet = 30;
    p1.setMySet_fixed(20);
    printf("  Fixed: myCash=%d (expected %d), mySet=%d (expected %d=%d+20)\n",
           p1.myCash, cashBefore1 - 20, p1.mySet, 30 + 20, p1.mySet - p1.myLastRelativeSet);
    if (p1.myCash == cashBefore1 - 20 && p1.mySet == 50) {
        printf("  PASS: Fixed code correctly updates myCash and mySet\n");
    } else {
        printf("  FAIL: Fix didn't work as expected\n");
        failures++;
    }

    // --- Scenario 2: All-in bet (theValue >= myCash) ---
    printf("\n--- Scenario 2 (theValue=150, myCash=100, mySet=30) ---\n");
    ClientPlayer p2;
    p2.mySet = 30;
    p2.myCash = 100;
    
    int cashBefore2 = p2.myCash;

    // Buggy implementation
    p2.setMySet_buggy(150);
    printf("  Buggy: myCash=%d (expected 0, bug: not set to 0), mySet=%d (expected %d=%d+100), myLastRelativeSet=%d\n",
           p2.myCash, p2.mySet + p2.myLastRelativeSet, p2.mySet + 100, p2.mySet + p2.myLastRelativeSet, p2.myLastRelativeSet);
    if (p2.myCash != 0 || p2.mySet != 130) {
        printf("  BUG CONFIRMED: Buggy code doesn't set myCash to 0 or mySet to total\n");
    } else {
        printf("  Bug NOT confirmed (unexpected)\n");
        failures++;
    }

    // Reset and test fixed implementation
    p2.myCash = 100;
    p2.mySet = 30;
    p2.setMySet_fixed(150);
    printf("  Fixed: myCash=%d (expected 0), mySet=%d (expected %d=%d+100)\n",
           p2.myCash, p2.mySet, 30 + 100);
    if (p2.myCash == 0 && p2.mySet == 130) {
        printf("  PASS: Fixed code correctly handles all-in case\n");
    } else {
        printf("  FAIL: Fix didn't work correctly\n");
        failures++;
    }

    // --- Scenario 3: Zero bet ---
    printf("\n--- Scenario 3 (theValue=0, myCash=100, mySet=30) ---\n");
    ClientPlayer p3;
    p3.mySet = 30;
    p3.myCash = 100;
    
    int cashBefore3 = p3.myCash;

    // Buggy implementation
    p3.setMySet_buggy(0);
    printf("  Buggy: myCash=%d (expected 100, bug: not changed), mySet=%d (expected 30, unchanged)\n",
           p3.myCash, p3.mySet);
    if (p3.myCash == 100 && p3.mySet == 30) {
        printf("  BUG NOT confirmed: Buggy code happens to work for zero bet\n");
    } else {
        printf("  Bug confirmed for zero bet\n");
    }

    // Reset and test fixed implementation
    p3.myCash = 100;
    p3.mySet = 30;
    p3.setMySet_fixed(0);
    printf("  Fixed: myCash=%d (expected 100), mySet=%d (expected 30)\n",
           p3.myCash, p3.mySet);
    if (p3.myCash == 100 && p3.mySet == 30) {
        printf("  PASS: Fixed code correctly handles zero bet\n");
    } else {
        printf("  FAIL: Fix didn't work correctly\n");
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("TEST PASSED: ClientPlayer::setMySet bug confirmed and fix verified\n");
    return 0;
}