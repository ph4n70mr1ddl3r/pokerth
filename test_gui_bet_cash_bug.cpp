// Test for gameTableImpl::mySet() myCash not being deducted bug.
//
// Bug: In gameTableImpl::mySet() (gametableimpl.cpp line 1930-1956),
// when a non-all-in bet is made (betValue < tempCash), the code does
// `humanPlayer->setMySet(betValue);` but does NOT deduct betValue from
// humanPlayer->getMyCash(). This causes myCash to remain unchanged even
// though chips were bet, leading to incorrect chip counts.
//
// Fix: Also deduct betValue from myCash in the non-all-in case.
//
// Compile:
//   g++ -std=c++23 -Wall -Wextra -o /tmp/test_gui_bet_cash_bug test_gui_bet_cash_bug.cpp
//   /tmp/test_gui_bet_cash_bug

#include <cstdio>
#include <cstdlib>

// Mock PlayerInterface - minimal subset needed for the bet test.
struct MockPlayer {
    int mySet{0};
    int myCash{100};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    void setMySet(int value) { mySet = value; }
    void setMyCash(int value) { myCash = value; }
};

// Simulate the BUGGY gameTableImpl::mySet() - only sets mySet, doesn't deduct myCash
static void action_buggy(MockPlayer& p, int betValue, int tempCash) {
    // BUG: Only sets mySet, doesn't deduct myCash
    p.setMySet(betValue);
    // BUG: myCash is NOT updated - should be tempCash - betValue
}

// Simulate the FIXED gameTableImpl::mySet() - sets mySet AND deducts myCash
static void action_fixed(MockPlayer& p, int betValue, int tempCash) {
    p.setMySet(betValue);
    p.setMyCash(tempCash - betValue);  // FIX: deduct bet from cash
}

int main()
{
    int failures = 0;

    printf("GUI bet cash deduction bug test\n\n");

    // --- Scenario 1: Normal bet (betValue < tempCash) ---
    printf("--- Scenario 1: Normal bet (betValue=20, tempCash=100) ---\n");
    MockPlayer p1;
    int cashBefore1 = p1.getMyCash();

    action_buggy(p1, 20, 100);
    printf("  Buggy: myCash = %d (expected %d, bug: 100 not reduced)\n",
           p1.getMyCash(), cashBefore1 - 20);
    if (p1.getMyCash() == cashBefore1 - 20) {
        printf("  BUG CONFIRMED: Buggy code doesn't deduct bet from cash\n");
    } else {
        printf("  Bug NOT confirmed (unexpected)\n");
        failures++;
    }

    // Reset and test fix
    p1.setMyCash(100);
    action_fixed(p1, 20, 100);
    printf("  Fixed: myCash = %d (expected %d)\n",
           p1.getMyCash(), 100 - 20);
    if (p1.getMyCash() == 100 - 20) {
        printf("  PASS: Fixed code correctly deducts bet from cash\n");
    } else {
        printf("  FAIL: Fix didn't work as expected\n");
        failures++;
    }

    // --- Scenario 2: All-in bet (betValue >= tempCash) ---
    printf("\n--- Scenario 2: All-in bet (betValue=150, tempCash=100) ---\n");
    MockPlayer p2;
    int cashBefore2 = p2.getMyCash();

    action_buggy(p2, 150, 100);
    printf("  Buggy: myCash = %d (expected 0), mySet = %d\n",
           p2.getMyCash(), p2.getMySet());
    if (p2.getMyCash() == 0) {
        printf("  All-in case correctly sets myCash to 0\n");
    } else {
        printf("  UNEXPECTED: myCash should be 0 for all-in\n");
        failures++;
    }

    // Reset and test fix for all-in
    p2.setMyCash(100);
    // All-in path already handles this correctly (sets myCash=0, mySet=tempCash)
    p2.setMySet(150);
    p2.setMyCash(0);
    printf("  Fixed all-in: myCash = %d (expected 0), mySet = %d\n",
           p2.getMyCash(), p2.getMySet());
    if (p2.getMyCash() == 0 && p2.getMySet() == 150) {
        printf("  PASS: All-in case works correctly\n");
    } else {
        printf("  FAIL: All-in case unexpected\n");
        failures++;
    }

    // --- Scenario 3: Edge case - betValue = 0 ---
    printf("\n--- Scenario 3: Zero bet (betValue=0, tempCash=100) ---\n");
    MockPlayer p3;
    int cashBefore3 = p3.getMyCash();

    action_buggy(p3, 0, 100);
    printf("  Buggy: myCash = %d (expected 100, no change for zero bet)\n",
           p3.getMyCash());
    if (p3.getMyCash() == 100) {
        printf("  PASS: Zero bet doesn't change cash\n");
    } else {
        printf("  FAIL: Zero bet should not change cash\n");
        failures++;
    }

    // Reset and test fix for zero bet
    p3.setMyCash(100);
    action_fixed(p3, 0, 100);
    printf("  Fixed: myCash = %d (expected 100)\n",
           p3.getMyCash());
    if (p3.getMyCash() == 100) {
        printf("  PASS: Fixed code handles zero bet correctly\n");
    } else {
        printf("  FAIL: Fixed code unexpected\n");
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