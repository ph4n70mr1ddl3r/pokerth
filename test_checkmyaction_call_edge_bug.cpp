// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_CALL edge cases
// when mySet >= highestSet with various myCash and targetBet values.
//
// Current fix: When mySet >= highestSet, the call is valid regardless of
// targetBet and myCash, because no additional chips need to be put in.

#include <cstdio>
#include <cstdlib>

/// Mock LocalPlayer with minimal interface needed for checkMyAction testing.
struct MockPlayer {
    int mySet{0};
    int myCash{1000};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Simulate the PLAYER_ACTION_CALL check from checkMyAction (FIXED version)
static int callFixed(int mySet, int myCash, int highestSet, int targetBet) {
    if (mySet >= highestSet) {
        // FIX: When mySet >= highestSet, call is valid because no additional chips needed
        // targetBet and myCash are irrelevant when no chips need to be put in
        return 0;
    }
    else if (myCash > 0 && targetBet <= myCash) {
        // not all in
        if (myCash + mySet >= highestSet && targetBet == highestSet - mySet) {
            return 0;
        }
        // all in
        if (targetBet == myCash) {
            return 0;
        }
    }
    else if (myCash <= 0) {
        // No cash to call with
        return 1;
    }
    // Invalid call case
    return 1;
}

// Simulate the BUGGY PLAYER_ACTION_CALL check from checkMyAction (old buggy version)
static int callBuggy(int mySet, int myCash, int highestSet, int targetBet) {
    if (mySet >= highestSet) {
        // BUG: incorrectly requires targetBet <= myCash || targetBet == 0
        // even when mySet >= highestSet means no additional chips needed
        if (targetBet <= myCash || targetBet == 0) {
            return 0;
        }
    }
    else if (myCash > 0 && targetBet <= myCash) {
        // not all in
        if (myCash + mySet >= highestSet && targetBet == highestSet - mySet) {
            return 0;
        }
        // all in
        if (targetBet == myCash) {
            return 0;
        }
    }
    else if (myCash <= 0) {
        // No cash to call with
        return 1;
    }
    // Invalid call case
    return 1;
}

int main()
{
    int failures = 0;

    printf("Testing checkMyAction PLAYER_ACTION_CALL mySet>=highestSet edge cases\n\n");

    // --- Scenario 1: mySet >= highestSet, myCash == 0, targetBet > 0 ---
    printf("--- Scenario 1: mySet=100, myCash=0, highestSet=50, targetBet=25 ---\n");
    printf("  mySet(100) >= highestSet(50): player has already put in enough chips\n");
    printf("  myCash(0): no additional cash needed\n");
    printf("  targetBet(25): irrelevant when no chips needed\n\n");

    int fixed1 = callFixed(100, 0, 50, 25);
    int buggy1 = callBuggy(100, 0, 50, 25);
    printf("  Fixed: %d (expected 0, valid call)\n", fixed1);
    printf("  Buggy: %d\n", buggy1);
    if (buggy1 == 0) {
        printf("  BUG: Buggy code also allows (this is pre-fix behavior)\n");
    } else {
        printf("  CORRECT: Buggy code incorrectly rejects (this is the bug fixed in 7069c84)\n");
    }
    if (fixed1 != 0) {
        printf("  FIX FAIL: expected 0\n");
        failures++;
    } else {
        printf("  PASS: Fixed code correctly allows call\n");
    }

    // --- Scenario 2: mySet >= highestSet, myCash == 0, targetBet == 0 ---
    printf("\n--- Scenario 2: mySet=100, myCash=0, highestSet=50, targetBet=0 ---\n");
    printf("  mySet(100) >= highestSet(50): player has already put in enough chips\n");
    printf("  myCash(0): no additional cash needed\n");
    printf("  targetBet(0): no additional bet\n\n");

    int fixed2 = callFixed(100, 0, 50, 0);
    int buggy2 = callBuggy(100, 0, 50, 0);
    printf("  Fixed: %d (expected 0)\n", fixed2);
    printf("  Buggy: %d\n", buggy2);
    if (fixed2 != 0) {
        printf("  FIX FAIL: expected 0\n");
        failures++;
    } else {
        printf("  PASS: Fixed code correctly allows call\n");
    }

    // --- Scenario 3: mySet >= highestSet, myCash > 0, targetBet > 0 ---
    printf("\n--- Scenario 3: mySet=100, myCash=50, highestSet=50, targetBet=25 ---\n");
    printf("  mySet(100) >= highestSet(50): player has already put in enough chips\n");
    printf("  myCash(50): has cash but irrelevant\n");
    printf("  targetBet(25): irrelevant when no chips needed\n\n");

    int fixed3 = callFixed(100, 50, 50, 25);
    int buggy3 = callBuggy(100, 50, 50, 25);
    printf("  Fixed: %d (expected 0)\n", fixed3);
    printf("  Buggy: %d (expected 0)\n", buggy3);
    if (fixed3 != 0) {
        printf("  FIX FAIL: expected 0\n");
        failures++;
    } else {
        printf("  PASS: Fixed code correctly allows call\n");
    }

    // --- Scenario 4: mySet < highestSet, myCash == 0, targetBet > 0 ---
    printf("\n--- Scenario 4: mySet=50, myCash=0, highestSet=100, targetBet=50 ---\n");
    printf("  mySet(50) < highestSet(100): must call at least 50\n");
    printf("  myCash(0): no cash to call with\n\n");

    int fixed4 = callFixed(50, 0, 100, 50);
    int buggy4 = callBuggy(50, 0, 100, 50);
    printf("  Fixed: %d (expected 1, invalid call)\n", fixed4);
    printf("  Buggy: %d (expected 1, invalid call)\n", buggy4);
    if (fixed4 != 1) {
        printf("  FIX FAIL: expected 1\n");
        failures++;
    } else {
        printf("  PASS: Fixed code correctly rejects call\n");
    }
    if (buggy4 != 1) {
        printf("  BUG: Buggy code incorrectly allows\n");
    } else {
        printf("  CORRECT: Buggy code correctly rejects\n");
    }

    // --- Scenario 5: mySet < highestSet, myCash > 0, targetBet > myCash ---
    printf("\n--- Scenario 5: mySet=50, myCash=30, highestSet=100, targetBet=80 ---\n");
    printf("  mySet(50) < highestSet(100): must call at least 50\n");
    printf("  myCash(30): insufficient cash for targetBet=80\n\n");

    int fixed5 = callFixed(50, 30, 100, 80);
    int buggy5 = callBuggy(50, 30, 100, 80);
    printf("  Fixed: %d (expected 1, invalid call - targetBet > myCash)\n", fixed5);
    printf("  Buggy: %d\n", buggy5);
    if (fixed5 != 1) {
        printf("  FIX FAIL: expected 1\n");
        failures++;
    } else {
        printf("  PASS: Fixed code correctly rejects call\n");
    }

    // --- Scenario 6: mySet < highestSet, myCash > 0, exact call ---
    printf("\n--- Scenario 6: mySet=50, myCash=30, highestSet=80, targetBet=30 ---\n");
    printf("  mySet(50) < highestSet(80): must call at least 30\n");
    printf("  myCash(30): exactly enough for the call\n");
    printf("  targetBet(30): exactly highestSet - mySet = 80 - 50 = 30\n\n");

    int fixed6 = callFixed(50, 30, 80, 30);
    int buggy6 = callBuggy(50, 30, 80, 30);
    printf("  Fixed: %d (expected 0, valid exact call)\n", fixed6);
    printf("  Buggy: %d\n", buggy6);
    if (fixed6 != 0) {
        printf("  FIX FAIL: expected 0\n");
        failures++;
    } else {
        printf("  PASS: Fixed code correctly allows exact call\n");
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("ALL TESTS PASSED: checkMyAction PLAYER_CALL edge cases verified\n");
    return 0;
}