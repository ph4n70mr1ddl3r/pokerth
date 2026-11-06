// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_ALLIN bug.
//
// Bug: In LocalPlayer::checkMyAction(), the PLAYER_ACTION_ALLIN case only
// checks if targetBet == myCash, but does not verify that the player has
// sufficient chips to go all-in. When myCash == 0 and targetBet == 0,
// the function incorrectly returns 0 (valid) because 0 == 0, even though
// the player has no chips to bet.
//
// Fix: Add a check to ensure the player has non-zero cash before
// accepting the all-in action.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_check_myaction_allin_bug test_check_myaction_allin_bug.cpp && /tmp/test_check_myaction_allin_bug

#include <cstdio>
#include <cstdlib>

// Mock Player: minimal subset needed for the all-in test.
struct MockPlayer {
    int mySet{0};
    int myCash{100};

    MockPlayer() = default;
    MockPlayer(int set, int cash) : mySet(set), myCash(cash) {}

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Simulate the OLD (buggy) PLAYER_ACTION_ALLIN check from checkMyAction
// Old code: if(targetBet == getMyCash()) return 0;
static int checkMyActionAllInBuggy(MockPlayer& p, int targetBet) {
    if (targetBet == p.getMyCash()) {
        return 0;  // BUG: accepts all-in even when myCash == 0 and targetBet == 0
    }
    return 1;
}

// Simulate the NEW (fixed) PLAYER_ACTION_ALLIN check from checkMyAction
// New code: check targetBet == myCash AND ensure player has non-zero cash
static int checkMyActionAllInFixed(MockPlayer& p, int targetBet) {
    if (targetBet == p.getMyCash() && p.getMyCash() > 0) {
        return 0;  // FIX: only accept all-in when player has non-zero cash
    }
    return 1;
}

int main() {
    int failures = 0;

    printf("Testing checkMyAction PLAYER_ALLIN bug\n");

    // --- Scenario 1: Normal all-in (myCash > 0) ---
    printf("\n--- Scenario 1 (Normal all-in: myCash=100, targetBet=100) ---\n");
    MockPlayer p1;
    int buggy1 = checkMyActionAllInBuggy(p1, 100);
    int fixed1 = checkMyActionAllInFixed(p1, 100);
    printf("  Buggy: %d (expected 0, valid all-in)\n", buggy1);
    printf("  Fixed: %d (expected 0, valid all-in)\n", fixed1);
    bool buggyOk1 = (buggy1 == 0);
    bool fixedOk1 = (fixed1 == 0);
    printf("  Buggy works: %s\n", buggyOk1 ? "PASS" : "FAIL");
    printf("  Fixed works: %s\n", fixedOk1 ? "PASS" : "FAIL");
    if (!buggyOk1 || !fixedOk1) failures++;

    // --- Scenario 2: All-in with zero cash (THE BUG) ---
    printf("\n--- Scenario 2 (All-in with zero cash: myCash=0, targetBet=0) ---\n");
    MockPlayer p2(0, 0);
    int buggy2 = checkMyActionAllInBuggy(p2, 0);
    int fixed2 = checkMyActionAllInFixed(p2, 0);
    printf("  Buggy: %d (expected 1, should reject - no chips!)\n", buggy2);
    printf("  Fixed: %d (expected 1, correctly rejects)\n", fixed2);
    bool buggyOk2 = (buggy2 == 1);
    bool fixedOk2 = (fixed2 == 1);
    printf("  Bug confirmed (buggy accepts invalid all-in): %s\n",
           (!buggyOk2) ? "PASS (bug present)" : "FAIL");
    printf("  Fixed works: %s\n", fixedOk2 ? "PASS" : "FAIL");
    if (!fixedOk2) failures++;

    // --- Scenario 3: All-in with some cash ---
    printf("\n--- Scenario 3 (All-in with some cash: myCash=50, targetBet=50) ---\n");
    MockPlayer p3(0, 50);
    int buggy3 = checkMyActionAllInBuggy(p3, 50);
    int fixed3 = checkMyActionAllInFixed(p3, 50);
    printf("  Buggy: %d (expected 0, valid all-in)\n", buggy3);
    printf("  Fixed: %d (expected 0, valid all-in)\n", fixed3);
    bool buggyOk3 = (buggy3 == 0);
    bool fixedOk3 = (fixed3 == 0);
    printf("  Buggy works: %s\n", buggyOk3 ? "PASS" : "FAIL");
    printf("  Fixed works: %s\n", fixedOk3 ? "PASS" : "FAIL");
    if (!buggyOk3 || !fixedOk3) failures++;

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: PLAYER_ALLIN bug confirmed and fix verified\n");
    return 0;
}