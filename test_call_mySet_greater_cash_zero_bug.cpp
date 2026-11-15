// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_CALL bug when mySet >= highestSet but myCash == 0.
//
// Bug: In localplayer.cpp line 1095, the check `getMyCash() > 0` requires positive cash even when
// the player's set already covers the highest set. The call should be valid when mySet >= highestSet
// because no additional chips need to be put in, similar to the PLAYER_ACTION_CHECK fix.
//
// Fix: Allow call when mySet >= highestSet regardless of myCash value, since the set chips alone
// should determine whether call is valid.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_call_mySet_greater_cash_zero_bug test_call_mySet_greater_cash_zero_bug.cpp && /tmp/test_call_mySet_greater_cash_zero_bug

#include <cstdio>
#include <cstdlib>
#include <cstdint>
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

// Current (buggy) PLAYER_ACTION_CALL handling from the switch block
// Simulates the old checkMyAction logic for PLAYER_ACTION_CALL
int checkMyAction_Call_Buggy(int targetAction, int targetBet, int highestSet, LocalPlayer* player) {
    if (targetAction == 1) { // PLAYER_ACTION_CALL
        if (player->getMySet() >= highestSet) {
            // Player has already put in enough chips (mySet >= highestSet).
            // Bug: requires getMyCash() > 0 even though no additional chips needed.
            if (player->getMyCash() > 0 && targetBet <= player->getMyCash()) {
                return 0;
            }
            return 1;
        }
        else if (player->getMyCash() > 0 && targetBet <= player->getMyCash()) {
            // not all in
            if (player->getMyCash() + player->getMySet() >= highestSet && targetBet == highestSet - player->getMySet()) {
                return 0;
            }
            // all in
            if (targetBet == player->getMyCash()) {
                return 0;
            }
        }
        else if (player->getMyCash() <= 0) {
            // No cash to call with
            return 1;
        }
        // Invalid call case
        return 1;
    }
    return -1; // invalid
}

// Fixed PLAYER_ACTION_CALL handling
int checkMyAction_Call_Fixed(int targetAction, int targetBet, int highestSet, LocalPlayer* player) {
    if (targetAction == 1) { // PLAYER_ACTION_CALL
        if (player->getMySet() >= highestSet) {
            // Player has already put in enough chips (mySet >= highestSet).
            // Call is valid regardless of myCash since no additional chips need to be put in.
            // targetBet must still be checked but cash is irrelevant when mySet >= highestSet.
            if (targetBet <= player->getMyCash() + player->getMySet() - highestSet + player->getMyCash()) {
                // Simplified: just check targetBet is reasonable when mySet >= highestSet
                // The key fix: no require getMyCash() > 0
                return 0;
            }
            return 1;
        }
        else if (player->getMyCash() > 0 && targetBet <= player->getMyCash()) {
            // not all in
            if (player->getMyCash() + player->getMySet() >= highestSet && targetBet == highestSet - player->getMySet()) {
                return 0;
            }
            // all in
            if (targetBet == player->getMyCash()) {
                return 0;
            }
        }
        else if (player->getMyCash() <= 0) {
            // No cash to call with
            return 1;
        }
        // Invalid call case
        return 1;
    }
    return -1; // invalid
}

int main()
{
    int failures = 0;
    bool allPassed = true;

    printf("Testing PLAYER_ACTION_CALL bug when mySet >= highestSet but myCash == 0\n\n");

// --- Scenario 1: mySet >= highestSet but myCash == 0 (THE BUG) ---
    printf("--- Scenario 1 (mySet >= highestSet, myCash == 0: should allow call) ---\n");
    {
        LocalPlayer player1(100, 0);  // mySet=100, myCash=0
        int highestSet1 = 80;

        // Buggy: should return -1 (call rejected because myCash > 0 check fails)
        // This confirms the bug: myCash > 0 is unnecessarily required when mySet >= highestSet
        int result1_buggy = checkMyAction_Call_Buggy(1, 0, highestSet1, &player1);
        if (result1_buggy == -1) {
            printf("  Buggy: returned -1 (CALL REJECTED - BUG CONFIRMED: myCash > 0 unnecessarily required)\n");
        } else {
            printf("  Buggy: returned 0 (CALL ALLOWED - unexpected, bug not present)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should return 0 (call allowed because mySet >= highestSet)
        int result1_fixed = checkMyAction_Call_Fixed(1, 0, highestSet1, &player1);
        if (result1_fixed == 0) {
            printf("  Fixed: returned 0 (CALL ALLOWED - correct)\n");
        } else {
            printf("  Fixed: returned -1 (CALL REJECTED - BUG: fix incorrect)\n");
            allPassed = false;
            failures++;
        }
    }

// --- Scenario 2: mySet >= highestSet with positive myCash (should still work) ---
    printf("\n--- Scenario 2 (mySet >= highestSet, myCash > 0: should allow call) ---\n");
    {
        LocalPlayer player2(100, 50);  // mySet=100, myCash=50
        int highestSet2 = 80;

        // Buggy: should return 0 (cash > 0, so condition passes)
        int result2_buggy = checkMyAction_Call_Buggy(1, 0, highestSet2, &player2);
        if (result2_buggy == 0) {
            printf("  Buggy: returned 0 (CALL ALLOWED - correct)\n");
        } else {
            printf("  Buggy: returned -1 (CALL REJECTED - unexpected)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should also return 0
        int result2_fixed = checkMyAction_Call_Fixed(1, 0, highestSet2, &player2);
        if (result2_fixed == 0) {
            printf("  Fixed: returned 0 (CALL ALLOWED - correct)\n");
        } else {
            printf("  Fixed: returned -1 (CALL REJECTED - unexpected)\n");
            allPassed = false;
            failures++;
        }
    }

// --- Scenario 3: mySet < highestSet with myCash == 0 (call should be rejected) ---
    printf("\n--- Scenario 3 (mySet < highestSet, myCash == 0: should reject call) ---\n");
    {
        LocalPlayer player3(50, 0);  // mySet=50, myCash=0, highestSet=80

        // Buggy: should return -1 (call rejected - no cash)
        int result3_buggy = checkMyAction_Call_Buggy(1, 0, 80, &player3);
        if (result3_buggy == -1) {
            printf("  Buggy: returned -1 (CALL REJECTED - correct)\n");
        } else {
            printf("  Buggy: returned 0 (CALL ALLOWED - unexpected)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should also return -1 (call rejected - no cash)
        int result3_fixed = checkMyAction_Call_Fixed(1, 0, 80, &player3);
        if (result3_fixed == -1) {
            printf("  Fixed: returned -1 (CALL REJECTED - correct)\n");
        } else {
            printf("  Fixed: returned 0 (CALL ALLOWED - unexpected)\n");
            allPassed = false;
            failures++;
        }
    }

// --- Scenario 4: mySet < highestSet with myCash > 0, targetBet == myCash (call allowed) ---
    printf("\n--- Scenario 4 (mySet < highestSet, myCash > 0, targetBet == myCash: should allow call) ---\n");
    {
        LocalPlayer player4(50, 100);  // mySet=50, myCash=100, highestSet=80
        int highestSet4 = 80;

        // Buggy: should return 0 (cash > 0 and targetBet == myCash)
        int result4_buggy = checkMyAction_Call_Buggy(1, 100, highestSet4, &player4);
        if (result4_buggy == 0) {
            printf("  Buggy: returned 0 (CALL ALLOWED - correct)\n");
        } else {
            printf("  Buggy: returned -1 (CALL REJECTED - unexpected)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should also return 0
        int result4_fixed = checkMyAction_Call_Fixed(1, 100, highestSet4, &player4);
        if (result4_fixed == 0) {
            printf("  Fixed: returned 0 (CALL ALLOWED - correct)\n");
        } else {
            printf("  Fixed: returned -1 (CALL REJECTED - unexpected)\n");
            allPassed = false;
            failures++;
        }
    }

// --- Scenario 5: mySet < highestSet with myCash > 0 but targetBet > myCash (call rejected) ---
    printf("\n--- Scenario 5 (mySet < highestSet, myCash > 0, targetBet > myCash: should reject call) ---\n");
    {
        LocalPlayer player5(50, 100);  // mySet=50, myCash=100, highestSet=80
        int highestSet5 = 80;
        int targetBet5 = 150;  // > myCash

        // Buggy: should return -1 (call rejected - targetBet > myCash)
        int result5_buggy = checkMyAction_Call_Buggy(1, targetBet5, highestSet5, &player5);
        if (result5_buggy == -1) {
            printf("  Buggy: returned -1 (CALL REJECTED - correct)\n");
        } else {
            printf("  Buggy: returned 0 (CALL ALLOWED - unexpected)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should also return -1 (call rejected)
        int result5_fixed = checkMyAction_Call_Fixed(1, targetBet5, highestSet5, &player5);
        if (result5_fixed == -1) {
            printf("  Fixed: returned -1 (CALL REJECTED - correct)\n");
        } else {
            printf("  Fixed: returned 0 (CALL ALLOWED - unexpected)\n");
            allPassed = false;
            failures++;
        }
    }

    printf("\n");
    if (!allPassed) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: PLAYER_ACTION_CALL bug confirmed and fix verified\n");
    return 0;
}