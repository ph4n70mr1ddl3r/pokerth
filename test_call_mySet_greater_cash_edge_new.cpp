// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_CALL edge case
// when mySet > highestSet but myCash == 0 and targetBet > 0.
//
// Bug hunt: Verify that when mySet > highestSet and myCash == 0,
// a PLAYER_ACTION_CALL is still valid because the player's set chips
// already cover the highest set. The targetBet is irrelevant in this case.
//
// This test verifies the fix: when mySet >= highestSet, the call should
// be allowed regardless of myCash and targetBet values.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_call_mySet_greater_cash_edge_new test_call_mySet_greater_cash_edge_new.cpp && /tmp/test_call_mySet_greater_cash_edge_new

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

// Mock LocalException for test purposes
struct LocalException : public std::runtime_error {
    LocalException(const char* file, int line, int err, const char* msg = "")
        : std::runtime_error(msg) {}
};

// Helper macro for the test
#define LocalException(file, line, err, msg) LocalException(__FILE__, __LINE__, 0, msg)

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

// Fixed PLAYER_ACTION_CALL check: handles mySet >= highestSet case
static int checkMyActionCallFixed(int targetAction, int targetBet, int highestSet, LocalPlayer* player) {
    if (targetAction == 1) { // PLAYER_ACTION_CALL
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
    return -1; // invalid
}

// Old (buggy) code pattern (for comparison - would reject call when myCash == 0)
static int checkMyActionCallBuggy(int targetAction, int targetBet, int highestSet, LocalPlayer* player) {
    if (targetAction == 1) { // PLAYER_ACTION_CALL
        if (player->getMySet() >= highestSet && player->getMyCash() > 0) {
            // Buggy: requires positive cash even when mySet >= highestSet
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
    return -1; // invalid
}

int main() {
    int failures = 0;

    printf("Testing PLAYER_ACTION_CALL edge case: mySet > highestSet, myCash == 0, targetBet > 0\n\n");

// --- Scenario 1: mySet > highestSet, myCash == 0, targetBet > 0 (THE EDGE CASE) ---
    printf("--- Scenario 1 (mySet > highestSet, myCash == 0, targetBet > 0: should allow call) ---\n");
    {
        LocalPlayer player1(100, 0);  // mySet=100, myCash=0
        int highestSet1 = 80;
        int targetBet1 = 20;  // > myCash (0)

        // Buggy: should NOT return 0 (BUG: requires cash > 0 unnecessarily)
        // With buggy code, the first if fails because myCash > 0 is false,
        // and the else if also fails because myCash > 0 is false.
        // So the call is incorrectly rejected.
        int result1_buggy = checkMyActionCallBuggy(1, targetBet1, highestSet1, &player1);

        // Fixed: should return 0 (call allowed because mySet >= highestSet)
        int result1_fixed = checkMyActionCallFixed(1, targetBet1, highestSet1, &player1);

        if (result1_buggy == 0) {
            printf("  Buggy: returned 0 (CALL ALLOWED - unexpected, bug not present)\n");
        } else {
            printf("  Buggy: returned -1 (CALL REJECTED - BUG CONFIRMED: myCash > 0 unnecessarily required)\n");
        }

        if (result1_fixed == 0) {
            printf("  Fixed: returned 0 (CALL ALLOWED - correct)\n");
        } else {
            printf("  Fixed: returned -1 (CALL REJECTED - BUG: fix incorrect)\n");
            failures++;
        }
    }

// --- Scenario 2: mySet > highestSet with positive myCash (should still work) ---
    printf("\n--- Scenario 2 (mySet > highestSet, myCash > 0: should allow call) ---\n");
    {
        LocalPlayer player2(100, 50);  // mySet=100, myCash=50
        int highestSet2 = 80;
        int targetBet2 = 20;

        // Buggy: should return 0 (cash > 0, so condition passes)
        int result2_buggy = checkMyActionCallBuggy(1, targetBet2, highestSet2, &player2);

        // Fixed: should also return 0
        int result2_fixed = checkMyActionCallFixed(1, targetBet2, highestSet2, &player2);

        if (result2_buggy == 0) {
            printf("  Buggy: returned 0 (CALL ALLOWED - correct)\n");
        } else {
            printf("  Buggy: returned -1 (CALL REJECTED - unexpected)\n");
            failures++;
        }

        if (result2_fixed == 0) {
            printf("  Fixed: returned 0 (CALL ALLOWED - correct)\n");
        } else {
            printf("  Fixed: returned -1 (CALL REJECTED - unexpected)\n");
            failures++;
        }
    }

// --- Scenario 3: mySet < highestSet with myCash == 0 (call should be rejected) ---
    printf("\n--- Scenario 3 (mySet < highestSet, myCash == 0: should reject call) ---\n");
    {
        LocalPlayer player3(50, 0);  // mySet=50, myCash=0, highestSet=80
        int highestSet3 = 80;
        int targetBet3 = 0;

        // Buggy: should return -1 (call rejected)
        int result3_buggy = checkMyActionCallBuggy(1, targetBet3, highestSet3, &player3);

        // Fixed: should also return -1 (call rejected - not enough chips)
        int result3_fixed = checkMyActionCallFixed(1, targetBet3, highestSet3, &player3);

        if (result3_buggy == -1) {
            printf("  Buggy: returned -1 (CALL REJECTED - correct)\n");
        } else {
            printf("  Buggy: returned 0 (CALL ALLOWED - unexpected)\n");
            failures++;
        }

        if (result3_fixed == -1) {
            printf("  Fixed: returned -1 (CALL REJECTED - correct)\n");
        } else {
            printf("  Fixed: returned 0 (CALL ALLOWED - unexpected)\n");
            failures++;
        }
    }

// --- Scenario 4: mySet < highestSet with myCash > 0 but insufficient total (should reject) ---
    printf("\n--- Scenario 4 (mySet < highestSet, myCash > 0 but insufficient: should reject call) ---\n");
    {
        LocalPlayer player4(50, 30);  // mySet=50, myCash=30, highestSet=100
        int highestSet4 = 100;
        int targetBet4 = 0;

        // Buggy: should return -1 (call rejected - not enough total chips)
        int result4_buggy = checkMyActionCallBuggy(1, targetBet4, highestSet4, &player4);

        // Fixed: should also return -1 (call rejected - not enough total chips)
        int result4_fixed = checkMyActionCallFixed(1, targetBet4, highestSet4, &player4);

        if (result4_buggy == -1) {
            printf("  Buggy: returned -1 (CALL REJECTED - correct)\n");
        } else {
            printf("  Buggy: returned 0 (CALL ALLOWED - unexpected)\n");
            failures++;
        }

        if (result4_fixed == -1) {
            printf("  Fixed: returned -1 (CALL REJECTED - correct)\n");
        } else {
            printf("  Fixed: returned 0 (CALL ALLOWED - unexpected)\n");
            failures++;
        }
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: PLAYER_ACTION_CALL edge case mySet > highestSet with myCash == 0 and targetBet > 0 confirmed and fix verified\n");
    return 0;
}