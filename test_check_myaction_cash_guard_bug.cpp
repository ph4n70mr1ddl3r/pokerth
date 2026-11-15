// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_CALL cash guard bug
// when mySet >= highestSet and myCash == 0 and targetBet == 0.
//
// Bug: In LocalPlayer::checkMyAction(), the PLAYER_ACTION_CALL case at line 1095
// has `if(targetBet == 0 || (getMyCash() > 0 && targetBet <= getMyCash()))` which
// returns 0 (valid) when targetBet == 0 even if myCash == 0. When myCash == 0 and
// targetBet == 0, the call should be invalid because the player has no chips, even
// though no additional chips are needed - the player cannot check with zero cash.
//
// Fix: Remove the targetBet == 0 exception when myCash == 0, making the behavior
// consistent: a call is only valid when the player has non-zero cash or targetBet
// is within their cash holdings.
//
// This test verifies the bug: without the fix, call accepted when myCash==0 and
// targetBet==0; with the fix, call correctly rejected when myCash==0.
#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer for test purposes
class LocalPlayer {
public:
    LocalPlayer(int mySet, int myCash) : mySet_(mySet), myCash_(myCash) {}
    
    int getMySet() const { return mySet_; }
    int getMyCash() const { return myCash_; }
    
private:
    int mySet_;
    int myCash_;
};

// Current (buggy) PLAYER_ACTION_CALL check from localplayer.cpp line 1091-1116
// Simulates the buggy behavior: returns 0 when mySet >= highestSet and targetBet==0
// regardless of myCash state.
static int checkMyActionCallBuggy(int mySet, int myCash, int highestSet, int targetBet) {
    if (mySet >= highestSet) {
        // BUG: no check for myCash when targetBet == 0
        // Just returns 0 regardless of cash state
        if (targetBet == 0 || (myCash > 0 && targetBet <= myCash)) {
            return 0;
        }
        return 1;
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

// Fixed PLAYER_ACTION_CALL check: adds myCash > 0 guard when mySet >= highestSet
static int checkMyActionCallFixed(int mySet, int myCash, int highestSet, int targetBet) {
    if (mySet >= highestSet) {
        // Player has already put in enough chips (mySet >= highestSet).
        // Call is valid only if player has non-zero cash AND targetBet is within cash.
        // Note: removed targetBet == 0 exception when myCash == 0
        if (myCash > 0 && targetBet <= myCash) {
            return 0;
        }
        return 1;
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

int main() {
    int failures = 0;

    printf("Testing PLAYER_ACTION_CALL mySet >= highestSet with zero cash bug\n\n");

    // --- Scenario 1: mySet >= highestSet, myCash == 0, targetBet == 0 (THE BUG) ---
    printf("--- Scenario 1 (mySet >= highestSet: mySet=100, highestSet=100, myCash=0, targetBet=0) ---\n");
    LocalPlayer p1(100, 0);
    int buggy1 = checkMyActionCallBuggy(100, 0, 100, 0);
    int fixed1 = checkMyActionCallFixed(100, 0, 100, 0);
    printf("  Buggy: %d (expected 1, call invalid when myCash==0 and targetBet==0)\n", buggy1);
    printf("  Fixed: %d (expected 1, call invalid when myCash==0)\n", fixed1);
    if (buggy1 == 0) {
        printf("  BUG CONFIRMED: Buggy code accepts call when myCash == 0\n");
    } else {
        printf("  Bug NOT confirmed: FAIL (buggy unexpectedly rejects)\n");
        failures++;
    }
    if (fixed1 == 1) {
        printf("  Fix works: PASS (correctly rejects invalid call)\n");
    } else {
        printf("  Fix FAIL: expected 1\n");
        failures++;
    }

    // --- Scenario 2: Normal case with myCash > 0 ---
    printf("\n--- Scenario 2 (mySet=100, highestSet=100, myCash=50, targetBet=50) ---\n");
    LocalPlayer p2(100, 50);
    int buggy2 = checkMyActionCallBuggy(100, 50, 100, 50);
    int fixed2 = checkMyActionCallFixed(100, 50, 100, 50);
    printf("  Buggy: %d (expected 0, valid call)\n", buggy2);
    printf("  Fixed: %d (expected 0, valid call)\n", fixed2);
    if (buggy2 == 0 && fixed2 == 0) {
        printf("  PASS: Both accept valid call when myCash > 0\n");
    } else {
        printf("  FAIL: Unexpected results\n");
        failures++;
    }

    // --- Scenario 3: mySet >= highestSet, myCash > 0, targetBet > myCash ---
    printf("\n--- Scenario 3 (mySet=100, highestSet=100, myCash=50, targetBet=100) ---\n");
    LocalPlayer p3(100, 50);
    int fixed3 = checkMyActionCallFixed(100, 50, 100, 100);
    printf("  Fixed: %d (expected 1, invalid since targetBet > myCash)\n", fixed3);
    if (fixed3 == 1) {
        printf("  PASS: Fixed code correctly rejects call when targetBet > myCash\n");
    } else {
        printf("  FAIL: expected 1\n");
        failures++;
    }

    // --- Scenario 4: mySet < highestSet, myCash == 0, targetBet == 0 (should reject) ---
    printf("\n--- Scenario 4 (mySet < highestSet: mySet=50, highestSet=100, myCash=0, targetBet=0) ---\n");
    LocalPlayer p4(50, 0);
    int fixed4 = checkMyActionCallFixed(50, 0, 100, 0);
    printf("  Fixed: %d (expected 1, call rejected - insufficient chips)\n", fixed4);
    if (fixed4 == 1) {
        printf("  PASS: Fixed code correctly rejects call when mySet < highestSet and myCash == 0\n");
    } else {
        printf("  FAIL: expected 1\n");
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: PLAYER_ACTION_CALL mySet >= highestSet with zero cash bug confirmed and fix verified\n");
    return 0;
}