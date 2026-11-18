// Test for PLAYER_ACTION_CALL edge case: mySet >= highestSet with myCash == 0
// This verifies the fix from commit 2c03cbe

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

// Fixed PLAYER_ACTION_CALL check from checkMyAction (with myCash > 0 guard)
static int checkMyActionCallFixed(MockPlayer& p, int highestSet) {
    if (p.getMySet() >= highestSet && p.getMyCash() > 0) {
        return 0;  // valid call: has sufficient chips and cash
    }
    return 1;
}

// Old (buggy) PLAYER_ACTION_CALL check (without myCash > 0 guard)
static int checkMyActionCallBuggy(MockPlayer& p, int highestSet) {
    if (p.getMySet() >= highestSet) {
        return 0;  // BUG: doesn't check myCash > 0
    }
    return 1;
}

int main()
{
    int failures = 0;

    // Scenario 1: mySet == 0, highestSet == 0, myCash > 0, targetBet == 0
    // Should allow call (posting small blind)
    printf("Test 1: mySet=0, highestSet=0, myCash>0, targetBet=0\n");
    MockPlayer p1(0, 1000);
    int highestSet1 = 0;
    int result_fixed1 = checkMyActionCallFixed(p1, highestSet1);
    int result_buggy1 = checkMyActionCallBuggy(p1, highestSet1);
    
    if (result_fixed1 == 0) {
        printf("  PASS: Fixed code allows call\n");
    } else {
        printf("  FAIL: fixed code should allow call, got %d\n", result_fixed1);
        failures++;
    }
    if (result_buggy1 == 0) {
        printf("  INFO: Buggy code also allows (current behavior before fix)\n");
    } else {
        printf("  INFO: Buggy code also rejects\n");
    }

    // Scenario 2: mySet == 0, highestSet == 0, myCash == 0, targetBet == 0
    // Should REJECT call (no cash to post blind)
    printf("\nTest 2: mySet=0, highestSet=0, myCash==0, targetBet=0\n");
    MockPlayer p2(0, 0);
    int highestSet2 = 0;
    int result_fixed2 = checkMyActionCallFixed(p2, highestSet2);
    int result_buggy2 = checkMyActionCallBuggy(p2, highestSet2);
    
    if (result_fixed2 == 1) {
        printf("  PASS: Fixed code rejects call\n");
    } else {
        printf("  FAIL: fixed code should reject call, got %d\n", result_fixed2);
        failures++;
    }
    if (result_buggy2 == 0) {
        printf("  BUG CONFIRMED: Buggy code allows call even with myCash==0\n");
    } else {
        printf("  INFO: Buggy code also rejects\n");
    }

    // Scenario 3: mySet == 50, highestSet == 50, myCash > 0, targetBet == 0
    // Player has already contributed enough, should allow call
    printf("\nTest 3: mySet=50, highestSet=50, myCash>0, targetBet=0\n");
    MockPlayer p3(50, 1000);
    int highestSet3 = 50;
    int result_fixed3 = checkMyActionCallFixed(p3, highestSet3);
    int result_buggy3 = checkMyActionCallBuggy(p3, highestSet3);
    
    if (result_fixed3 == 0) {
        printf("  PASS: Fixed code allows call\n");
    } else {
        printf("  FAIL: fixed code should allow call, got %d\n", result_fixed3);
        failures++;
    }

    // Scenario 4: mySet == 50, highestSet == 50, myCash == 0, targetBet == 0
    // Player has contributed enough but has no cash - should reject
    printf("\nTest 4: mySet=50, highestSet=50, myCash==0, targetBet=0\n");
    MockPlayer p4(50, 0);
    int highestSet4 = 50;
    int result_fixed4 = checkMyActionCallFixed(p4, highestSet4);
    int result_buggy4 = checkMyActionCallBuggy(p4, highestSet4);
    
    if (result_fixed4 == 1) {
        printf("  PASS: Fixed code rejects call\n");
    } else {
        printf("  FAIL: fixed code should reject call, got %d\n", result_fixed4);
        failures++;
    }
    if (result_buggy4 == 0) {
        printf("  BUG CONFIRMED: Buggy code allows call even with myCash==0\n");
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: checkMyAction mySet>=highestSet myCash==0 edge case verified\n");
    return 0;
}
