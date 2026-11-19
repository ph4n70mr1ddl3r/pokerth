// Regression test: checkMyAction() PLAYER_ACTION_CALL mySet >= highestSet with myCash == 0 edge case
//
// Verifies that when mySet >= highestSet and myCash == 0, the call is correctly
// accepted because the player's set chips already cover the highest bet, and no
// additional chips need to be put in.
//
// This test verifies the bug: in localplayer.cpp line 1092, the check
// `getMySet() >= highestSet && myCash > 0` requires positive cash even when
// the player's set already covers the highest set. The call should be valid
// because no additional chips need to be put in.
//
// Bug: The check `getMySet() >= highestSet && myCash > 0` incorrectly rejects
// calls when mySet >= highestSet but myCash == 0, even though no additional
// chips need to be put in.
//
// Fix: Remove the myCash > 0 requirement from the first condition, since set
// chips alone should determine whether a call is valid when mySet >= highestSet.
//
// Compile:
//   g++ -O2 -std=c++23 -o /tmp/test_checkmyaction_cashguard_bug test_checkmyaction_cashguard_bug.cpp
//   /tmp/test_checkmyaction_cashguard_bug

#include <cstdio>
#include <cstdlib>

// Mock PlayerInterface: minimal subset needed for the checkMyAction test.
struct MockPlayer {
    int mySet;
    int myCash;

    MockPlayer(int s, int c) : mySet(s), myCash(c) {}

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// BUGGY checkMyAction: requires myCash > 0 when mySet >= highestSet
// This is the buggy code found in localplayer.cpp line 1092
int checkMyActionCallBuggy(int targetAction, int targetBet, int highestSet, int minimumRaise, int smallBlind, MockPlayer* player) {
    if (targetAction == 2) { // PLAYER_ACTION_CALL
        if (player->getMySet() >= highestSet && player->getMyCash() > 0) {
            // BUG: requires myCash > 0 even though set chips cover highestSet
            // Player has already put in enough chips (mySet >= highestSet).
            // Call is valid because no additional chips need to be put in.
            // BUG: targetBet is irrelevant when mySet >= highestSet and player has cash.
            return 0;
        }
        else if(player->getMyCash() > 0 && targetBet <= player->getMyCash()) {
            // not all in
            if(player->getMyCash() + player->getMySet() >= highestSet && targetBet == highestSet - player->getMySet()) {
                return 0;
            }
            // all in
            if(targetBet == player->getMyCash()) {
                return 0;
            }
        }
        else if(player->getMyCash() <= 0) {
            // No cash to call with
            return 1;
        }
    }
    return 1;
}

// FIXED checkMyAction: does NOT require myCash > 0 when mySet >= highestSet
// The fix: remove the myCash > 0 requirement from the first condition
int checkMyActionCallFixed(int targetAction, int targetBet, int highestSet, int minimumRaise, int smallBlind, MockPlayer* player) {
    if (targetAction == 2) { // PLAYER_ACTION_CALL
        if (player->getMySet() >= highestSet) {
            // Player has already put in enough chips (mySet >= highestSet).
            // Call is valid because no additional chips need to be put in.
            // myCash > 0 is NOT required when set chips cover highestSet.
            // FIX: removed the myCash > 0 requirement
            return 0;
        }
        else if(player->getMyCash() > 0 && targetBet <= player->getMyCash()) {
            // not all in
            if(player->getMyCash() + player->getMySet() >= highestSet && targetBet == highestSet - player->getMySet()) {
                return 0;
            }
            // all in
            if(targetBet == player->getMyCash()) {
                return 0;
            }
        }
        else if(player->getMyCash() <= 0) {
            // No cash to call with
            return 1;
        }
    }
    return 1;
}

int main()
{
    int failures = 0;

    printf("Regression test: checkMyAction mySet>=highestSet myCash==0 edge case\n\n");
    
    // Scenario 1: mySet >= highestSet, myCash == 0, targetBet == 0
    // Expected: call should be ALLOWED (0) since no additional chips needed
    printf("Scenario 1: mySet >= highestSet, myCash == 0, targetBet == 0\n");
    MockPlayer player1(100, 0);
    int highestSet1 = 100;
    int targetBet1 = 0;
    int result_buggy1 = checkMyActionCallBuggy(2, targetBet1, highestSet1, 0, 0, &player1);
    int result_fixed1 = checkMyActionCallFixed(2, targetBet1, highestSet1, 0, 0, &player1);
    
    if (result_fixed1 == 0) {
        printf("  PASS: Fixed code accepts call (myCash == 0, mySet >= highestSet)\n");
    } else {
        printf("  FAIL: Fixed code should accept call, got %d\n", result_fixed1);
        failures++;
    }
    
    if (result_buggy1 == 1) {
        printf("  BUG CONFIRMED: Buggy code rejects call even though mySet >= highestSet\n");
        printf("  (myCash > 0 check unnecessarily rejects valid call)\n");
    } else {
        printf("  INFO: Buggy code also accepts call\n");
    }

    // Scenario 2: mySet >= highestSet, myCash > 0, targetBet == 0 (should still work)
    printf("\nScenario 2: mySet >= highestSet, myCash > 0, targetBet == 0\n");
    MockPlayer player2(100, 50);
    int highestSet2 = 100;
    int targetBet2 = 0;
    int result_buggy2 = checkMyActionCallBuggy(2, targetBet2, highestSet2, 0, 0, &player2);
    int result_fixed2 = checkMyActionCallFixed(2, targetBet2, highestSet2, 0, 0, &player2);
    
    if (result_fixed2 == 0) {
        printf("  PASS: Fixed code accepts call (myCash > 0, mySet >= highestSet)\n");
    } else {
        printf("  FAIL: Fixed code should accept call, got %d\n", result_fixed2);
        failures++;
    }
    
    if (result_buggy2 == 0) {
        printf("  INFO: Buggy code also accepts call (myCash > 0, mySet >= highestSet)\n");
    }

    // Scenario 3: mySet < highestSet, myCash == 0, targetBet == 0 (should reject)
    printf("\nScenario 3: mySet < highestSet, myCash == 0, targetBet == 0\n");
    MockPlayer player3(50, 0);
    int highestSet3 = 100;
    int targetBet3 = 0;
    int result_buggy3 = checkMyActionCallBuggy(2, targetBet3, highestSet3, 0, 0, &player3);
    int result_fixed3 = checkMyActionCallFixed(2, targetBet3, highestSet3, 0, 0, &player3);
    
    if (result_fixed3 == 1) {
        printf("  PASS: Fixed code rejects call (mySet < highestSet, myCash == 0)\n");
    } else {
        printf("  FAIL: Fixed code should reject call, got %d\n", result_fixed3);
        failures++;
    }
    
    if (result_buggy3 == 1) {
        printf("  INFO: Buggy code also rejects call\n");
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }
    printf("REGRESSION TEST PASSED: checkMyAction mySet>=highestSet myCash==0 edge case verified\n");
    return 0;
}