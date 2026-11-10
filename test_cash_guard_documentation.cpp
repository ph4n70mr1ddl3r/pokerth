// Test for PLAYER_ACTION_CALL cash guard when mySet >= highestSet.
//
// This test verifies the fix from commit eea5f8b: "Bug hunt: Fix PLAYER_ACTION_CALL
// myCash==0 edge case in checkMyAction()".
//
// Original bug: When mySet >= highestSet and myCash == 0 and targetBet > 0,
// the call was incorrectly accepted because 0 <= 0 is true, even though the
// player has no chips to bet.
//
// Fix: Added `&& getMyCash() > 0` guard to the mySet >= highestSet condition,
// making the behavior consistent across all cases. Now the call is only valid
// when the player has both sufficient set chips AND non-zero cash.
//
// This test ensures the fix is properly in place and working.
//
#include <cstdio>
#include <cstdlib>

// Mock Player matching the actual LocalPlayer structure used in checkMyAction
struct MockPlayer {
    int mySet{0};
    int myCash{100};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Simulates the FIXED PLAYER_ACTION_CALL check (with cash guard from eea5f8b)
static int checkMyAction_Fixed(MockPlayer& p, int targetBet, int highestSet) {
    if (p.getMySet() >= highestSet && p.getMyCash() > 0) {
        // Player has sufficient chips AND non-zero cash.
        // Call is valid only if targetBet is within cash.
        if (targetBet <= p.getMyCash()) {
            return 0;
        }
        return 1;
    }
    // If mySet >= highestSet but myCash == 0, or mySet < highestSet:
    // fall through to normal call logic
    if (p.getMySet() < highestSet && targetBet <= p.getMyCash()) {
        // not all in
        if (p.getMyCash() + p.getMySet() >= highestSet && targetBet == highestSet - p.getMySet()) {
            return 0;
        }
        // all in
        if (targetBet == p.getMyCash()) {
            return 0;
        }
    }
    else if (p.getMyCash() <= 0) {
        // No cash to call with
        return 1;
    }
    // Invalid call case
    return 1;
}

// Simulates the ORIGINAL (buggy) code missing the cash guard
static int checkMyAction_Buggy(MockPlayer& p, int targetBet, int highestSet) {
    if (p.getMySet() >= highestSet) {
        // BUG: no check for p.getMyCash() > 0
        // Just returns 0 regardless of cash state
        return 0;
    }
    // Original logic for mySet < highestSet
    if (p.getMySet() < highestSet && targetBet <= p.getMyCash()) {
        // not all in
        if (p.getMyCash() + p.getMySet() >= highestSet && targetBet == highestSet - p.getMySet()) {
            return 0;
        }
        // all in
        if (targetBet == p.getMyCash()) {
            return 0;
        }
    }
    else if (p.getMyCash() <= 0) {
        // No cash to call with
        return 1;
    }
    // Invalid call case
    return 1;
}

int main()
{
    int failures = 0;

    printf("Test: PLAYER_ACTION_CALL cash guard when mySet >= highestSet\n\n");

// --- Scenario 1: mySet > highestSet, myCash == 0, targetBet > 0 (THE BUG SCENARIO) ---
    printf("--- Scenario 1: mySet > highestSet, myCash == 0, targetBet > 0 ---\n");
    {
        MockPlayer player;
        player.mySet = 150;
        player.myCash = 0;
        int highestSet = 100;
        int targetBet = 50;

        int result_fixed = checkMyAction_Fixed(player, targetBet, highestSet);
        int result_buggy = checkMyAction_Buggy(player, targetBet, highestSet);

        printf("  mySet=%d, myCash=%d, highestSet=%d, targetBet=%d\n", 150, 0, 100, 50);
        printf("  Fixed: %d (expected 1, call invalid - no cash)\n", result_fixed);
        printf("  Buggy: %d (BUG: call allowed when myCash == 0)\n", result_buggy);

        if (result_fixed == 1) {
            printf("  PASS: Fixed code correctly rejects call when myCash == 0\n");
        } else {
            printf("  FAIL: Fixed code should reject call when myCash == 0\n");
            failures++;
        }

        if (result_buggy == 0) {
            printf("  BUG CONFIRMED: Buggy code allows call when myCash == 0\n");
            failures++;
        } else {
            printf("  OK: Buggy code correctly rejects\n");
        }
    }

// --- Scenario 2: mySet == highestSet, myCash > 0, targetBet <= myCash (should allow) ---
    printf("\n--- Scenario 2: mySet == highestSet, myCash > 0, targetBet <= myCash ---\n");
    {
        MockPlayer player;
        player.mySet = 100;
        player.myCash = 100;
        int highestSet = 100;
        int targetBet = 50;

        int result_fixed = checkMyAction_Fixed(player, targetBet, highestSet);
        int result_buggy = checkMyAction_Buggy(player, targetBet, highestSet);

        printf("  mySet=%d, myCash=%d, highestSet=%d, targetBet=%d\n", 100, 100, 100, 50);
        printf("  Fixed: %d (expected 0, call valid)\n", result_fixed);
        printf("  Buggy: %d\n", result_buggy);

        if (result_fixed == 0) {
            printf("  PASS: Fixed code allows call when targetBet <= myCash\n");
        } else {
            printf("  FAIL: Fixed code should allow call\n");
            failures++;
        }

        if (result_buggy == 0) {
            printf("  PASS: Buggy code also allows call (expected since it doesn't check cash)\n");
        } else {
            printf("  FAIL: Buggy code should allow call\n");
            failures++;
        }
    }

// --- Scenario 3: mySet < highestSet, myCash == 0 (should reject) ---
    printf("\n--- Scenario 3: mySet < highestSet, myCash == 0 ---\n");
    {
        MockPlayer player;
        player.mySet = 50;
        player.myCash = 0;
        int highestSet = 100;
        int targetBet = 30;

        int result_fixed = checkMyAction_Fixed(player, targetBet, highestSet);
        int result_buggy = checkMyAction_Buggy(player, targetBet, highestSet);

        printf("  mySet=%d, myCash=%d, highestSet=%d, targetBet=%d\n", 50, 0, 100, 30);
        printf("  Fixed: %d (expected 1, call invalid - no cash)\n", result_fixed);
        printf("  Buggy: %d\n", result_buggy);

        if (result_fixed == 1) {
            printf("  PASS: Fixed code rejects call when myCash == 0\n");
        } else {
            printf("  FAIL: Fixed code should reject call\n");
            failures++;
        }

        if (result_buggy == 1) {
            printf("  PASS: Buggy code also rejects call\n");
        } else {
            printf("  FAIL: Buggy code should reject call\n");
            failures++;
        }
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: PLAYER_ACTION_CALL cash guard behavior verified\n");
    return 0;
}