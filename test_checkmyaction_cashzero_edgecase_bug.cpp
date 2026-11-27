// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_CALL myCash==0 edge case.
//
// Bug: In LocalPlayer::checkMyAction(), the PLAYER_ACTION_CALL case at line 1092
// has the condition `getMySet() >= highestSet && getMyCash() > 0 && targetBet <= getMyCash()`,
// which unconditionally requires myCash > 0 even when the player's set chips already
// cover the highest set and no additional bet is required (targetBet == 0).
//
// This means a player with mySet >= highestSet and myCash == 0 and targetBet == 0
// is incorrectly rejected, even though the player has already contributed enough chips
// and no additional chips are needed.
//
// Fix: Modify the condition to allow the case where mySet >= highestSet && targetBet == 0,
// even when myCash == 0, while still preventing the general myCash == 0 exploit when
// targetBet > 0.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_checkmyaction_cashzero_edgecase_bug
//          test_checkmyaction_cashzero_edgecase_bug.cpp && /tmp/test_checkmyaction_cashzero_edgecase_bug

#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer minimal subset needed for the call test.
struct MockLocalPlayer {
    int mySet{0};
    int myCash{0};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Simulate the OLD (buggy) PLAYER_ACTION_CALL check from checkMyAction
static int checkMyActionCallBuggy(int mySet, int myCash, int highestSet, int targetBet) {
    if (mySet >= highestSet && myCash > 0 && targetBet <= myCash) {
        return 0;  // BUG: rejects when myCash == 0 even if targetBet == 0
    }
    else if (myCash > 0 && targetBet <= myCash) {
        if (myCash + mySet >= highestSet && targetBet == highestSet - mySet) {
            return 0;
        }
        if (targetBet == myCash) {
            return 0;
        }
    }
    else if (myCash <= 0) {
        return 1;
    }
    return 1;
}

// Simulate the FIXED PLAYER_ACTION_CALL check from checkMyAction
static int checkMyActionCallFixed(int mySet, int myCash, int highestSet, int targetBet) {
    // Fix: allow the edge case where mySet >= highestSet && targetBet == 0
    // even when myCash == 0, while still preventing the myCash == 0 exploit
    if (mySet >= highestSet && targetBet <= myCash && (myCash > 0 || targetBet == 0)) {
        return 0;  // call valid: player has sufficient chips and/or no additional bet
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
        // No cash to call with AND targetBet > 0 (exploit prevention)
        return 1;
    }
    return 1;  // invalid call case
}

int main()
{
    int failures = 0;

    printf("Testing PLAYER_ACTION_CALL myCash==0 edge case in checkMyAction\n\n");

    // --- Scenario 1: mySet >= highestSet, myCash == 0, targetBet == 0 (THE BUG) ---
    printf("--- Scenario 1 (mySet=100, highestSet=100, myCash=0, targetBet=0) ---\n");
    printf("  Expected: call ALLOWED (player has sufficient set chips, no additional bet)\n");
    int buggy1 = checkMyActionCallBuggy(100, 0, 100, 0);
    int fixed1 = checkMyActionCallFixed(100, 0, 100, 0);
    printf("  Buggy: %d (expected 0, call allowed - but buggy rejects)\n", buggy1);
    printf("  Fixed: %d ", fixed1);
    if (fixed1 == 0) {
        printf("(correctly allows call - FIX WORKS)\n");
    } else {
        printf("(incorrectly rejects call - FIX FAILS)\n");
        failures++;
    }
    if (buggy1 == 1) {
        printf("  BUG CONFIRMED: Buggy code rejects call when myCash == 0 && targetBet == 0\n");
    } else {
        printf("  Bug NOT confirmed: FAIL\n");
        failures++;
    }

    // --- Scenario 2: mySet >= highestSet, myCash == 0, targetBet > 0 (should still reject) ---
    printf("\n--- Scenario 2 (mySet=100, highestSet=100, myCash=0, targetBet=50) ---\n");
    printf("  Expected: call REJECTED (cannot call with targetBet > myCash == 0)\n");
    int buggy2 = checkMyActionCallBuggy(100, 0, 100, 50);
    int fixed2 = checkMyActionCallFixed(100, 0, 100, 50);
    printf("  Buggy: %d ", buggy2);
    if (buggy2 == 1) {
        printf("(correctly rejects - buggy works for this case)\n");
    } else {
        printf("(buggy allows - BUG)\n");
    }
    printf("  Fixed: %d ", fixed2);
    if (fixed2 == 1) {
        printf("(correctly rejects - FIX WORKS)\n");
    } else {
        printf("(incorrectly allows - FIX FAILS)\n");
        failures++;
    }

    // --- Scenario 3: mySet >= highestSet, myCash > 0, targetBet <= myCash (should allow) ---
    printf("\n--- Scenario 3 (mySet=100, highestSet=100, myCash=50, targetBet=50) ---\n");
    printf("  Expected: call ALLOWED\n");
    int buggy3 = checkMyActionCallBuggy(100, 50, 100, 50);
    int fixed3 = checkMyActionCallFixed(100, 50, 100, 50);
    printf("  Buggy: %d ", buggy3);
    if (buggy3 == 0) {
        printf("(correctly allows)\n");
    } else {
        printf("(incorrectly rejects - BUG)\n");
        failures++;
    }
    printf("  Fixed: %d ", fixed3);
    if (fixed3 == 0) {
        printf("(correctly allows - FIX WORKS)\n");
    } else {
        printf("(incorrectly rejects - FIX FAILS)\n");
        failures++;
    }

    // --- Scenario 4: mySet < highestSet, myCash == 0, targetBet == 0 (should reject) ---
    printf("\n--- Scenario 4 (mySet=50, highestSet=100, myCash=0, targetBet=0) ---\n");
    printf("  Expected: call REJECTED (insufficient set chips)\n");
    int buggy4 = checkMyActionCallBuggy(50, 0, 100, 0);
    int fixed4 = checkMyActionCallFixed(50, 0, 100, 0);
    printf("  Buggy: %d ", buggy4);
    if (buggy4 == 1) {
        printf("(correctly rejects)\n");
    } else {
        printf("(buggy allows - BUG)\n");
        failures++;
    }
    printf("  Fixed: %d ", fixed4);
    if (fixed4 == 1) {
        printf("(correctly rejects - FIX WORKS)\n");
    } else {
        printf("(incorrectly allows - FIX FAILS)\n");
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }
    printf("TEST PASSED: PLAYER_CALL myCash==0 edge case bug confirmed and fix verified\n");
    return 0;
}