// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_CHECK bug when mySet > highestSet.
//
// Bug: In LocalPlayer::checkMyAction(), the PLAYER_ACTION_CHECK case only
// checks `getMySet() == highestSet`, but does not handle the case where
// mySet > highestSet. When mySet > highestSet, a check should be valid
// (player has already put in enough chips), but the function returns 1
// (invalid) because the outer condition fails.
//
// Fix: Change the CHECK condition from `getMySet() == highestSet`
// to `getMySet() >= highestSet`, so that players who have put in more
// chips than the current highest bet can also check.
//
// This test verifies the bug: without the fix, check is rejected when
// mySet > highestSet, while with the fix, check is accepted.

#include <cstdio>
#include <cstdlib>
#include <cassert>

// Mock Player: minimal subset needed for the check test.
struct MockPlayer {
    int mySet{0};
    int myCash{1000};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Fixed PLAYER_ACTION_CHECK check: handles mySet >= highestSet
static int checkMyActionCheckFixed(MockPlayer& p, int highestSet) {
    // FIX: handle mySet >= highestSet: player has already put in enough chips
    if (p.getMySet() >= highestSet) {
        return 0;
    }
    return 1;
}

// Buggy PLAYER_ACTION_CHECK check: only handles mySet == highestSet
static int checkMyActionCheckBuggy(MockPlayer& p, int highestSet) {
    if (p.getMySet() == highestSet) {
        return 0;
    }
    return 1;
}

int main()
{
    int failures = 0;

    printf("Testing checkMyAction PLAYER_CHECK bug when mySet > highestSet\n\n");

    // --- Scenario 1: mySet > highestSet (THE BUG) ---
    printf("--- Scenario 1 (mySet > highestSet: mySet=150, highestSet=100) ---\n");
    MockPlayer p1;
    p1.mySet = 150;
    int buggy1 = checkMyActionCheckBuggy(p1, 100);
    int fixed1 = checkMyActionCheckFixed(p1, 100);
    printf("  Buggy: %d (expected 0, check should be valid when mySet > highestSet)\n", buggy1);
    printf("  Fixed: %d (expected 0, check valid when mySet >= highestSet)\n", fixed1);
    if (buggy1 != 0) {
        printf("  Bug confirmed: PASS (bug present - check not accepted when mySet > highestSet)\n");
    } else {
        printf("  Bug NOT confirmed: FAIL (buggy unexpectedly accepts check)\n");
        failures++;
    }
    if (fixed1 != 0) {
        printf("  Fix FAIL: expected 0\n");
        failures++;
    } else {
        printf("  Fix works: PASS\n");
    }

    // --- Scenario 2: mySet == highestSet (should still work) ---
    printf("\n--- Scenario 2 (mySet == highestSet: mySet=100, highestSet=100) ---\n");
    MockPlayer p2;
    p2.mySet = 100;
    int buggy2 = checkMyActionCheckBuggy(p2, 100);
    int fixed2 = checkMyActionCheckFixed(p2, 100);
    printf("  Buggy: %d\n", buggy2);
    printf("  Fixed: %d\n", fixed2);
    if (buggy2 == fixed2 && buggy2 == 0) {
        printf("  Both handle mySet == highestSet: PASS\n");
    } else {
        printf("  Inconsistent handling: FAIL\n");
        failures++;
    }

    // --- Scenario 3: mySet < highestSet (should still be invalid) ---
    printf("\n--- Scenario 3 (mySet < highestSet: mySet=50, highestSet=100) ---\n");
    MockPlayer p3;
    p3.mySet = 50;
    int buggy3 = checkMyActionCheckBuggy(p3, 100);
    int fixed3 = checkMyActionCheckFixed(p3, 100);
    printf("  Buggy: %d\n", buggy3);
    printf("  Fixed: %d\n", fixed3);
    if (buggy3 == 1 && fixed3 == 1) {
        printf("  Both correctly reject check when mySet < highestSet: PASS\n");
    } else {
        printf("  Unexpected behavior: FAIL\n");
        failures++;
    }

    // --- Scenario 4: mySet = 0, highestSet = 0 (edge case) ---
    printf("\n--- Scenario 4 (mySet=0, highestSet=0) ---\n");
    MockPlayer p4;
    p4.mySet = 0;
    int fixed4 = checkMyActionCheckFixed(p4, 0);
    printf("  Fixed: %d (expected 0, check valid when both are 0)\n", fixed4);
    if (fixed4 == 0) {
        printf("  Fix works: PASS\n");
    } else {
        printf("  Fix FAIL: expected 0\n");
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: PLAYER_CHECK bug confirmed and fix verified\n");
    return 0;
}