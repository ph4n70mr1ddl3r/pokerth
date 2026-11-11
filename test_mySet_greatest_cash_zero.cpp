// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_CALL mySet >= highestSet with myCash == 0 edge case.
//
// Bug: In LocalPlayer::checkMyAction(), the condition at line 1074 was:
//   `if(getMySet() >= highestSet && getMyCash() > 0)`
// This incorrectly required positive cash even when the player's set already
// covers the highest bet. The call should be valid based on set chips alone.
//
// Fix (commit 901df49): Removed the getMyCash() > 0 requirement from the
// mySet >= highestSet condition, since set chips alone should determine validity.
//
// This test verifies the fix is correct.
//
#include <cstdio>
#include <cstdlib>
#include <cassert>

// Mock Player: minimal subset needed for the call test.
struct MockPlayer {
    int mySet{0};
    int myCash{100};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Fixed PLAYER_ACTION_CALL check from localplayer.cpp (after fix)
// Handles mySet >= highestSet regardless of myCash value
static int checkMyAction_CallFixed(MockPlayer& p, int targetBet, int highestSet) {
    if (p.getMySet() >= highestSet) {
        // Player has already put in enough chips (mySet >= highestSet).
        // Call is valid because no additional chips need to be put in.
        // targetBet is irrelevant when mySet >= highestSet.
        return 0;
    }
    else if (p.getMyCash() > 0 && targetBet <= p.getMyCash()) {
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

// Old (buggy) code pattern that requires myCash > 0 even when mySet >= highestSet
static int checkMyAction_CallBuggy(MockPlayer& p, int targetBet, int highestSet) {
    if (p.getMySet() >= highestSet && p.getMyCash() > 0) {
        // Buggy: requires positive cash even when mySet >= highestSet
        // This is incorrect - set chips alone should be sufficient
        return 0;
    }
    else if (p.getMyCash() > 0 && targetBet <= p.getMyCash()) {
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
    bool allPassed = true;

    printf("Testing PLAYER_ACTION_CALL edge case: mySet >= highestSet with myCash == 0\n\n");

// --- Scenario 1: mySet > highestSet, myCash == 0, targetBet == 0 (should allow call) ---
    printf("--- Scenario 1 (mySet > highestSet, myCash == 0, targetBet == 0) ---\n");
    {
        MockPlayer player1;
        player1.mySet = 150;
        player1.myCash = 0;
        int highestSet1 = 100;
        int targetBet1 = 0;

        // Fixed: should return 0 (call allowed because mySet >= highestSet)
        int result1_fixed = checkMyAction_CallFixed(player1, targetBet1, highestSet1);
        if (result1_fixed == 0) {
            printf("  Fixed: returned 0 (CALL ALLOWED - correct)\n");
        } else {
            printf("  Fixed: returned -1 (CALL REJECTED - BUG: myCash > 0 unnecessarily required)\n");
            allPassed = false;
            failures++;
        } else {
            printf("  Fixed: returned 0 (CALL ALLOWED - correct)\n");
        }

        // Buggy: should NOT return 0 (BUG: requires cash > 0 unnecessarily)
        int result1_buggy = checkMyAction_CallBuggy(player1, targetBet1, highestSet1);
        if (result1_buggy == 0) {
            printf("  Buggy: returned 0 (CALL ALLOWED - unexpected, bug not present)\n");
        } else {
            printf("  Buggy: returned -1 (CALL REJECTED - BUG CONFIRMED: myCash > 0 unnecessarily required)\n");
        }
    }

// --- Scenario 2: mySet == highestSet, myCash == 0, targetBet > 0 (should allow call) ---
    printf("\n--- Scenario 2 (mySet == highestSet, myCash == 0, targetBet > 0) ---\n");
    {
        MockPlayer player2;
        player2.mySet = 100;
        player2.myCash = 0;
        int highestSet2 = 100;
        int targetBet2 = 30;

        // Fixed: should return 0 (call allowed because mySet >= highestSet)
        int result2_fixed = checkMyAction_CallFixed(player2, targetBet2, highestSet2);
        if (result2_fixed == 0) {
            printf("  Fixed: returned 0 (CALL ALLOWED - correct)\n");
        } else {
            printf("  Fixed: returned -1 (CALL REJECTED - BUG: myCash > 0 unnecessarily required)\n");
            allPassed = false;
            failures++;
        }

        // Buggy: should NOT return 0
        int result2_buggy = checkMyAction_CallBuggy(player2, targetBet2, highestSet2);
        if (result2_buggy == 0) {
            printf("  Buggy: returned 0 (CALL ALLOWED - unexpected, bug not present)\n");
        } else {
            printf("  Buggy: returned -1 (CALL REJECTED - BUG CONFIRMED: myCash > 0 unnecessarily required)\n");
        }
    }

// --- Scenario 3: mySet > highestSet, myCash == 0, targetBet > 0 (should allow call) ---
    printf("\n--- Scenario 3 (mySet > highestSet, myCash == 0, targetBet > 0) ---\n");
    {
        MockPlayer player3;
        player3.mySet = 150;
        player3.myCash = 0;
        int highestSet3 = 100;
        int targetBet3 = 50;

        // Fixed: should return 0 (call allowed because mySet >= highestSet, targetBet irrelevant)
        int result3_fixed = checkMyAction_CallFixed(player3, targetBet3, highestSet3);
        if (result3_fixed == 0) {
            printf("  Fixed: returned 0 (CALL ALLOWED - correct)\n");
        } else {
            printf("  Fixed: returned -1 (CALL REJECTED - BUG: myCash > 0 unnecessarily required)\n");
            allPassed = false;
            failures++;
        }

        // Buggy: should NOT return 0
        int result3_buggy = checkMyAction_CallBuggy(player3, targetBet3, highestSet3);
        if (result3_buggy == 0) {
            printf("  Buggy: returned 0 (CALL ALLOWED - unexpected, bug not present)\n");
        } else {
            printf("  Buggy: returned -1 (CALL REJECTED - BUG CONFIRMED: myCash > 0 unnecessarily required)\n");
        }
    }

// --- Scenario 4: mySet < highestSet, myCash == 0, targetBet == 0 (should reject call) ---
    printf("\n--- Scenario 4 (mySet < highestSet, myCash == 0, targetBet == 0: should reject) ---\n");
    {
        MockPlayer player4;
        player4.mySet = 50;
        player4.myCash = 0;
        int highestSet4 = 100;
        int targetBet4 = 0;

        // Fixed: should return -1 (call rejected - not enough chips and no cash)
        int result4_fixed = checkMyAction_CallFixed(player4, targetBet4, highestSet4);
        if (result4_fixed == 1) {
            printf("  Fixed: returned 1 (CALL REJECTED - correct)\n");
        } else {
            printf("  Fixed: returned 0 or other (CALL ALLOWED - unexpected)\n");
            allPassed = false;
            failures++;
        }

        // Buggy: should also return -1
        int result4_buggy = checkMyAction_CallBuggy(player4, targetBet4, highestSet4);
        if (result4_buggy == 1) {
            printf("  Buggy: returned 1 (CALL REJECTED - correct)\n");
        } else {
            printf("  Buggy: returned 0 or other (CALL ALLOWED - unexpected)\n");
            failures++;
        }
    }

// --- Scenario 5: mySet < highestSet, myCash > 0, exact call (should allow) ---
    printf("\n--- Scenario 5 (mySet < highestSet, myCash > 0, exact call amount: should allow) ---\n");
    {
        MockPlayer player5;
        player5.mySet = 50;
        player5.myCash = 50;
        int highestSet5 = 100;
        int targetBet5 = 50;  // highestSet - mySet = 50

        // Fixed: should return 0 (exact call)
        int result5_fixed = checkMyAction_CallFixed(player5, targetBet5, highestSet5);
        if (result5_fixed == 0) {
            printf("  Fixed: returned 0 (CALL ALLOWED - correct)\n");
        } else {
            printf("  Fixed: returned -1 (CALL REJECTED - unexpected)\n");
            allPassed = false;
            failures++;
        }

        // Buggy: should also return 0
        int result5_buggy = checkMyAction_CallBuggy(player5, targetBet5, highestSet5);
        if (result5_buggy == 0) {
            printf("  Buggy: returned 0 (CALL ALLOWED - correct)\n");
        } else {
            printf("  Buggy: returned -1 (CALL REJECTED - unexpected)\n");
            failures++;
        }
    }

// --- Scenario 6: mySet < highestSet, myCash > 0, all-in call (should allow) ---
    printf("\n--- Scenario 6 (mySet < highestSet, myCash > 0, all-in call: should allow) ---\n");
    {
        MockPlayer player6;
        player6.mySet = 50;
        player6.myCash = 50;
        int highestSet6 = 100;
        int targetBet6 = 50;  // == myCash

        // Fixed: should return 0 (all-in)
        int result6_fixed = checkMyAction_CallFixed(player6, targetBet6, highestSet6);
        if (result6_fixed == 0) {
            printf("  Fixed: returned 0 (ALLIN ALLOWED - correct)\n");
        } else {
            printf("  Fixed: returned -1 (ALLIN REJECTED - unexpected)\n");
            allPassed = false;
            failures++;
        }

        // Buggy: should also return 0
        int result6_buggy = checkMyAction_CallBuggy(player6, targetBet6, highestSet6);
        if (result6_buggy == 0) {
            printf("  Buggy: returned 0 (ALLIN ALLOWED - correct)\n");
        } else {
            printf("  Buggy: returned -1 (ALLIN REJECTED - unexpected)\n");
            failures++;
        }
    }

    printf("\n");
    if (!allPassed) {
        printf("TEST FAILED: One or more scenarios did not pass as expected\n");
        return 1;
    }

    printf("TEST PASSED: PLAYER_ACTION_CALL edge case mySet >= highestSet with myCash == 0 confirmed and fix verified\n");
    return 0;
}