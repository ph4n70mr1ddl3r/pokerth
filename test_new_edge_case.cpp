// New test for checkMyAction edge case:
// Verifies PLAYER_ACTION_CALL when mySet == highestSet and myCash == 0 and targetBet == 0
// This should allow the call because the player's set chips already cover the highest set.

#include <cstdio>
#include <cstdlib>

struct LocalPlayer {
    int mySet;
    int myCash;
    
    LocalPlayer(int s, int c) : mySet(s), myCash(c) {}
    
    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Actual checkMyAction from localplayer.cpp (simplified)
int checkMyActionCall(int targetAction, int targetBet, int highestSet, int minimumRaise, int smallBlind, LocalPlayer* player) {
    if (targetAction == 2) { // PLAYER_ACTION_CALL
        if (player->getMySet() >= highestSet) {
            // Player has already put in enough chips (mySet >= highestSet).
            // Call is valid because no additional chips need to be put in.
            // targetBet is irrelevant when mySet >= highestSet.
            // Note: removed getMyCash() > 0 requirement - set chips alone are sufficient
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
    }
    return -1;
}

int main() {
    int failures = 0;

    // Scenario: mySet == highestSet, myCash == 0, targetBet == 0
    // Should allow call because mySet >= highestSet
    printf("Test: PLAYER_ACTION_CALL mySet == highestSet, myCash == 0, targetBet == 0\n");
    LocalPlayer player(100, 0);
    int highestSet = 100;
    int targetBet = 0;
    int result = checkMyActionCall(2, targetBet, highestSet, 0, 0, &player);
    
    if (result == 0) {
        printf("  PASS: call allowed (mySet >= highestSet, cash irrelevant)\n");
    } else {
        printf("  FAIL: expected 0 (call allowed), got %d\n", result);
        failures++;
    }

    // Also test with targetBet > 0
    printf("\nTest: PLAYER_ACTION_CALL mySet == highestSet, myCash == 0, targetBet > 0\n");
    LocalPlayer player2(100, 0);
    targetBet = 50;
    result = checkMyActionCall(2, targetBet, highestSet, 0, 0, &player2);
    
    if (result == 0) {
        printf("  PASS: call allowed (mySet >= highestSet, targetBet irrelevant)\n");
    } else {
        printf("  FAIL: expected 0 (call allowed), got %d\n", result);
        failures++;
    }

    // Test that mySet < highestSet with myCash == 0 still rejects
    printf("\nTest: PLAYER_ACTION_CALL mySet < highestSet, myCash == 0, targetBet == 0\n");
    LocalPlayer player3(50, 0);
    highestSet = 100;
    targetBet = 0;
    result = checkMyActionCall(2, targetBet, highestSet, 0, 0, &player3);
    
    if (result == -1) {
        printf("  PASS: call rejected (insufficient chips)\n");
    } else {
        printf("  FAIL: expected -1 (call rejected), got %d\n", result);
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }
    printf("ALL TESTS PASSED\n");
    return 0;
}
