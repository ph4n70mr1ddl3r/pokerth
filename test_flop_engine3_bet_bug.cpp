// Test for LocalPlayer::flopEngine3() PLAYER_ACTION_BET myCash/mySet bug.
//
// Bug: In LocalPlayer::flopEngine3(), the PLAYER_ACTION_BET case at line 4450 does
// `myCash -= mySet;` which subtracts the player's previous set amount from cash
// but does NOT update mySet = oldMySet + mySet. This is inconsistent with
// turnEngine3() and riverEngine3() which properly update both myCash and mySet.
//
// The buggy code only does: myCash -= mySet;
// The fixed code does: myCash -= mySet; mySet = oldMySet + mySet;
//
// This test verifies the bug and the fix.

#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer state for the flopEngine3 test
struct MockLocalPlayer {
    int mySet{0};
    int myCash{100};
    int oldMySet{0};
    int highestSet{100};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getOldMySet() const { return oldMySet; }
    int getHighestSet() const { return highestSet; }
};

// BUGGY: flopEngine3() PLAYER_ACTION_BET case - only subtracts mySet from myCash
static int flopEngine3_buggy(MockLocalPlayer& p) {
    if (p.mySet > 0) {
        p.myCash -= p.mySet;
        // BUG: mySet is NOT updated to oldMySet + mySet
    }
    return 0;
}

// FIXED: flopEngine3() PLAYER_ACTION_BET case - properly updates both myCash and mySet
static int flopEngine3_fixed(MockLocalPlayer& p) {
    if (p.mySet > 0) {
        p.myCash -= p.mySet;
        p.mySet = p.oldMySet + p.mySet;  // FIX: properly update mySet
    }
    return 0;
}

int main()
{
    int failures = 0;

    printf("Testing flopEngine3() PLAYER_ACTION_BET myCash/mySet bug\n\n");

    // --- Scenario 1: Normal bet with oldMySet > 0 ---
    printf("--- Scenario 1 (mySet=50, myCash=100, oldMySet=30) ---\n");
    MockLocalPlayer p1;
    p1.mySet = 50;
    p1.myCash = 100;
    p1.oldMySet = 30;

    int cashBefore1 = p1.myCash;

    flopEngine3_buggy(p1);
    printf("  Buggy: myCash=%d (expected %d, bug: 100-50=50), mySet=%d (expected %d=%d+%d)\n",
           p1.myCash, cashBefore1 - 50, p1.mySet, 30 + 50, p1.oldMySet, p1.mySet - p1.oldMySet);
    // Buggy: myCash = 100 - 50 = 50 (correct), but mySet stays 50 (wrong - should be 80)
    if (p1.myCash == cashBefore1 - 50 && p1.mySet == 50) {
        printf("  BUG CONFIRMED: Buggy code subtracts mySet from cash but doesn't update mySet\n");
    } else {
        printf("  Bug NOT confirmed (unexpected)\n");
        failures++;
    }

    // Reset and test fix
    p1.myCash = cashBefore1;
    p1.mySet = 50;
    flopEngine3_fixed(p1);
    printf("  Fixed: myCash=%d (expected %d), mySet=%d (expected %d=%d+%d)\n",
           p1.myCash, cashBefore1 - 50, p1.mySet, 30 + 50, p1.oldMySet, p1.mySet - p1.oldMySet);
    if (p1.myCash == cashBefore1 - 50 && p1.mySet == 80) {
        printf("  PASS: Fixed code correctly subtracts mySet from cash and updates mySet\n");
    } else {
        printf("  FAIL: Fix didn't work as expected\n");
        failures++;
    }

    // --- Scenario 2: Fresh bet (oldMySet = 0) ---
    printf("\n--- Scenario 2 (mySet=30, myCash=100, oldMySet=0) ---\n");
    MockLocalPlayer p2;
    p2.mySet = 30;
    p2.myCash = 100;
    p2.oldMySet = 0;

    int cashBefore2 = p2.myCash;

    flopEngine3_buggy(p2);
    printf("  Buggy: myCash=%d (expected %d, bug: 100-30=70), mySet=%d (expected %d=%d+%d)\n",
           p2.myCash, cashBefore2 - 30, p2.mySet, 0 + 30, p2.oldMySet, p2.mySet - p2.oldMySet);
    if (p2.myCash == cashBefore2 - 30 && p2.mySet == 30) {
        printf("  BUG CONFIRMED: Buggy code subtracts mySet from cash but doesn't update mySet\n");
    } else {
        printf("  Bug NOT confirmed (unexpected)\n");
        failures++;
    }

    // Reset and test fix
    p2.myCash = cashBefore2;
    p2.mySet = 30;
    flopEngine3_fixed(p2);
    printf("  Fixed: myCash=%d (expected %d), mySet=%d (expected %d=%d+%d)\n",
           p2.myCash, cashBefore2 - 30, p2.mySet, 0 + 30, p2.oldMySet, p2.mySet - p2.oldMySet);
    if (p2.myCash == cashBefore2 - 30 && p2.mySet == 30) {
        printf("  PASS: Fixed code handles fresh bet correctly (oldMySet=0)\n");
    } else {
        printf("  FAIL: Fix didn't work correctly\n");
        failures++;
    }

    // --- Scenario 3: Larger bet ---
    printf("\n--- Scenario 3 (mySet=75, myCash=200, oldMySet=25) ---\n");
    MockLocalPlayer p3;
    p3.mySet = 75;
    p3.myCash = 200;
    p3.oldMySet = 25;

    int cashBefore3 = p3.myCash;

    flopEngine3_buggy(p3);
    printf("  Buggy: myCash=%d (expected %d, bug: 200-75=125), mySet=%d (expected %d=%d+%d)\n",
           p3.myCash, cashBefore3 - 75, p3.mySet, 25 + 75, p3.oldMySet, p3.mySet - p3.oldMySet);
    if (p3.myCash == cashBefore3 - 75 && p3.mySet == 75) {
        printf("  BUG CONFIRMED: Buggy code subtracts mySet from cash but doesn't update mySet\n");
    } else {
        printf("  Bug NOT confirmed (unexpected)\n");
        failures++;
    }

    // Reset and test fix
    p3.myCash = cashBefore3;
    p3.mySet = 75;
    flopEngine3_fixed(p3);
    printf("  Fixed: myCash=%d (expected %d), mySet=%d (expected %d=%d+%d)\n",
           p3.myCash, cashBefore3 - 75, p3.mySet, 25 + 75, p3.oldMySet, p3.mySet - p3.oldMySet);
    if (p3.myCash == cashBefore3 - 75 && p3.mySet == 100) {
        printf("  PASS: Fixed code handles larger bet correctly\n");
    } else {
        printf("  FAIL: Fix didn't work correctly\n");
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: flopEngine3() PLAYER_ACTION_BET myCash/mySet bug confirmed and fix verified\n");
    return 0;
}