// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_CALL bug when mySet >= highestSet.
//
// Bug: In LocalPlayer::checkMyAction(), the PLAYER_ACTION_CALL case only
// checks `getMySet() < highestSet`, but does not handle the case where
// mySet >= highestSet. When mySet >= highestSet, a call should be valid
// (player has already put in enough chips), but the function returns 1
// (invalid) because the outer condition fails.
//
// Fix: Add handling for the case where mySet >= highestSet, returning
// 0 (valid) when the player has sufficient chips to call.
//
#include <cstdio>
#include <cstdlib>

// Mock Player: minimal subset needed for the call test.
struct MockPlayer {
    int mySet{0};
    int myCash{1000};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Fixed PLAYER_ACTION_CALL check: handles mySet >= highestSet
static int checkMyActionCallFixed(MockPlayer& p, int targetBet, int highestSet) {
    // Handle mySet >= highestSet: player has already put in enough chips
    if (p.getMySet() >= highestSet) {
        // Player has sufficient chips - call is valid if targetBet is within cash
        if (targetBet <= p.getMyCash()) {
            return 0;
        }
        return 1;
    }
    // Original logic for mySet < highestSet
    if (p.getMySet() < highestSet && targetBet <= p.getMyCash()) {
        // not all in
        if (p.getMyCash() + p.getMySet() >= highestSet && targetBet == highestSet - p.getMySet()) {
            return 0;
        }
        // all in
        if (p.getMyCash() + p.getMySet() <= highestSet && targetBet == p.getMyCash()) {
            return 0;
        }
    }
    return 1;
}

// Buggy PLAYER_ACTION_CALL check: only handles mySet < highestSet
static int checkMyActionCallBuggy(MockPlayer& p, int targetBet, int highestSet) {
    if (p.getMySet() < highestSet && targetBet <= p.getMyCash()) {
        // not all in
        if (p.getMyCash() + p.getMySet() >= highestSet && targetBet == highestSet - p.getMySet()) {
            return 0;
        }
        // all in
        if (p.getMyCash() + p.getMySet() <= highestSet && targetBet == p.getMyCash()) {
            return 0;
        }
    }
    return 1;
}

int main()
{
    int failures = 0;

    printf("Testing checkMyAction PLAYER_CALL bug when mySet >= highestSet\n\n");

    // --- Scenario 1: mySet == highestSet (THE BUG) ---
    printf("--- Scenario 1 (mySet == highestSet: mySet=100, highestSet=100) ---\n");
    MockPlayer p1;
    p1.mySet = 100;
    int buggy1 = checkMyActionCallBuggy(p1, 100, 100);
    int fixed1 = checkMyActionCallFixed(p1, 100, 100);
    printf("  Buggy: %d (expected 0, call should be valid when mySet == highestSet)\n", buggy1);
    printf("  Fixed: %d (expected 0, call valid when mySet >= highestSet)\n", fixed1);
    if (buggy1 != 0) {
        printf("  Bug confirmed: PASS (bug present - call not accepted when mySet == highestSet)\n");
    } else {
        printf("  Bug NOT confirmed: FAIL\n");
        failures++;
    }
    if (fixed1 != 0) {
        printf("  Fix FAIL: expected 0\n");
        failures++;
    } else {
        printf("  Fix works: PASS\n");
    }

    // --- Scenario 2: mySet > highestSet ---
    printf("\n--- Scenario 2 (mySet > highestSet: mySet=150, highestSet=100) ---\n");
    MockPlayer p2;
    p2.mySet = 150;
    int buggy2 = checkMyActionCallBuggy(p2, 100, 100);
    int fixed2 = checkMyActionCallFixed(p2, 100, 100);
    printf("  Buggy: %d\n", buggy2);
    printf("  Fixed: %d\n", fixed2);
    // Fixed should accept the call when mySet > highestSet
    if (fixed2 == 0) {
        printf("  Fix works: PASS (call accepted when mySet > highestSet)\n");
    } else {
        printf("  Fix FAIL: expected 0\n");
        failures++;
    }

    // --- Scenario 3: mySet < highestSet (normal call) ---
    printf("\n--- Scenario 3 (mySet < highestSet: mySet=50, highestSet=100, need to call 50) ---\n");
    MockPlayer p3;
    p3.mySet = 50;
    int buggy3 = checkMyActionCallBuggy(p3, 50, 100);
    int fixed3 = checkMyActionCallFixed(p3, 50, 100);
    printf("  Buggy: %d\n", buggy3);
    printf("  Fixed: %d\n", fixed3);
    if (buggy3 == fixed3 && buggy3 == 0) {
        printf("  Both handle normal call: PASS\n");
    } else {
        printf("  Inconsistent handling: FAIL\n");
        failures++;
    }

    // --- Scenario 4: Insufficient targetBet ---
    printf("\n--- Scenario 4 (Insufficient targetBet: mySet=100, highestSet=100, targetBet=5000) ---\n");
    MockPlayer p4;
    p4.mySet = 100;
    int fixed4 = checkMyActionCallFixed(p4, 5000, 100);
    printf("  Fixed: %d (expected 1, call invalid due to high bet)\n", fixed4);
    if (fixed4 != 1) {
        printf("  Fix FAIL: expected 1\n");
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: PLAYER_CALL bug confirmed and fix verified\n");
    return 0;
}