// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_ALLIN bug when myCash == 0.
//
// Bug: In localplayer.cpp line 1103, the check `targetBet == getMyCash() && getMyCash() > 0`
// requires positive cash even when the player's set already covers the highest set.
// The all-in should be valid when mySet >= highestSet because no additional chips
// need to be put in, similar to the PLAYER_ACTION_CALL fix.
//
// Fix: Add handling for the case where mySet >= highestSet, allowing all-in even when
// myCash == 0, since the set chips alone should determine whether all-in is valid.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_allin_bug_when_no_cash test_allin_bug_when_no_cash.cpp && /tmp/test_allin_bug_when_no_cash

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

// Simulated code from localplayer.cpp line 1102-1107
// Old (buggy) code pattern:
int checkMyAction_Allin_Buggy(int targetAction, int targetBet, int highestSet, LocalPlayer* player) {
    if (targetAction == 5) { // PLAYER_ACTION_ALLIN
        if (targetBet == player->getMyCash() && player->getMyCash() > 0) {
            // Player is going all-in with their cash.
            // But this requires positive cash, which is incorrect when mySet >= highestSet.
            return 0;
        }
    }
    return -1; // invalid
}

// Fixed code pattern:
int checkMyAction_Allin_Fixed(int targetAction, int targetBet, int highestSet, LocalPlayer* player) {
    if (targetAction == 5) { // PLAYER_ACTION_ALLIN
        if (player->getMySet() >= highestSet) {
            // Player has already put in enough chips (mySet >= highestSet).
            // All-in is valid because no additional chips need to be put in.
            // myCash is irrelevant when mySet >= highestSet.
            return 0;
        }
        else if (targetBet == player->getMyCash() && player->getMyCash() > 0) {
            // Player is going all-in with their cash.
            return 0;
        }
    }
    return -1; // invalid
}

int main()
{
    int failures = 0;
    bool allPassed = true;

    printf("Testing PLAYER_ACTION_ALLIN bug when myCash == 0 but mySet >= highestSet\n\n");

// --- Scenario 1: mySet >= highestSet but myCash == 0 (THE BUG) ---
    printf("--- Scenario 1 (mySet >= highestSet, myCash == 0: should allow all-in) ---\n");
    {
        LocalPlayer player1(100, 0);  // mySet=100, myCash=0
        int highestSet1 = 80;

        // Buggy: should return -1 (all-in rejected because myCash > 0 check fails)
        // This confirms the bug: myCash > 0 is unnecessarily required
        int result1_buggy = checkMyAction_Allin_Buggy(5, 0, highestSet1, &player1);
        if (result1_buggy == -1) {
            printf("  Buggy: returned -1 (ALLIN REJECTED - BUG CONFIRMED: myCash > 0 unnecessarily required)\n");
        } else {
            printf("  Buggy: returned 0 (ALLIN ALLOWED - unexpected, bug not present)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should return 0 (all-in allowed because mySet >= highestSet)
        int result1_fixed = checkMyAction_Allin_Fixed(5, 0, highestSet1, &player1);
        if (result1_fixed == 0) {
            printf("  Fixed: returned 0 (ALLIN ALLOWED - correct)\n");
        } else {
            printf("  Fixed: returned -1 (ALLIN REJECTED - BUG: fix incorrect)\n");
            allPassed = false;
            failures++;
        }
    }

// --- Scenario 2: mySet >= highestSet with positive myCash (should still work) ---
    printf("\n--- Scenario 2 (mySet >= highestSet, myCash > 0: should allow all-in) ---\n");
    {
        LocalPlayer player2(100, 50);  // mySet=100, myCash=50
        int highestSet2 = 80;

        // Buggy: should return 0 (cash > 0, so condition passes)
        int result2_buggy = checkMyAction_Allin_Buggy(5, 50, highestSet2, &player2);
        if (result2_buggy == 0) {
            printf("  Buggy: returned 0 (ALLIN ALLOWED - correct)\n");
        } else {
            printf("  Buggy: returned -1 (ALLIN REJECTED - unexpected)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should also return 0
        int result2_fixed = checkMyAction_Allin_Fixed(5, 50, highestSet2, &player2);
        if (result2_fixed == 0) {
            printf("  Fixed: returned 0 (ALLIN ALLOWED - correct)\n");
        } else {
            printf("  Fixed: returned -1 (ALLIN REJECTED - unexpected)\n");
            allPassed = false;
            failures++;
        }
    }

// --- Scenario 3: mySet < highestSet with myCash == 0 (all-in should be rejected) ---
    printf("\n--- Scenario 3 (mySet < highestSet, myCash == 0: should reject all-in) ---\n");
    {
        LocalPlayer player3(50, 0);  // mySet=50, myCash=0, highestSet=80

        // Buggy: should return -1 (all-in rejected - not enough chips)
        int result3_buggy = checkMyAction_Allin_Buggy(5, 0, 80, &player3);
        if (result3_buggy == -1) {
            printf("  Buggy: returned -1 (ALLIN REJECTED - correct)\n");
        } else {
            printf("  Buggy: returned 0 (ALLIN ALLOWED - unexpected)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should also return -1 (all-in rejected - not enough chips)
        int result3_fixed = checkMyAction_Allin_Fixed(5, 0, 80, &player3);
        if (result3_fixed == -1) {
            printf("  Fixed: returned -1 (ALLIN REJECTED - correct)\n");
        } else {
            printf("  Fixed: returned 0 (ALLIN ALLOWED - unexpected)\n");
            allPassed = false;
            failures++;
        }
    }

// --- Scenario 4: mySet < highestSet with myCash > 0, targetBet == myCash (all-in allowed) ---
    printf("\n--- Scenario 4 (mySet < highestSet, myCash > 0, targetBet == myCash: should allow all-in) ---\n");
    {
        LocalPlayer player4(50, 100);  // mySet=50, myCash=100, highestSet=80
        int highestSet4 = 80;
        int targetBet4 = 100;

        // Buggy: should return 0 (cash > 0 and targetBet == myCash)
        int result4_buggy = checkMyAction_Allin_Buggy(5, targetBet4, highestSet4, &player4);
        if (result4_buggy == 0) {
            printf("  Buggy: returned 0 (ALLIN ALLOWED - correct)\n");
        } else {
            printf("  Buggy: returned -1 (ALLIN REJECTED - unexpected)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should also return 0
        int result4_fixed = checkMyAction_Allin_Fixed(5, targetBet4, highestSet4, &player4);
        if (result4_fixed == 0) {
            printf("  Fixed: returned 0 (ALLIN ALLOWED - correct)\n");
        } else {
            printf("  Fixed: returned -1 (ALLIN REJECTED - unexpected)\n");
            allPassed = false;
            failures++;
        }
    }

// --- Scenario 5: mySet < highestSet with myCash > 0 but targetBet != myCash (all-in rejected) ---
    printf("\n--- Scenario 5 (mySet < highestSet, myCash > 0, targetBet != myCash: should reject all-in) ---\n");
    {
        LocalPlayer player5(50, 100);  // mySet=50, myCash=100, highestSet=80
        int highestSet5 = 80;
        int targetBet5 = 50;  // not equal to myCash

        // Buggy: should return -1 (all-in rejected - targetBet != myCash)
        int result5_buggy = checkMyAction_Allin_Buggy(5, targetBet5, highestSet5, &player5);
        if (result5_buggy == -1) {
            printf("  Buggy: returned -1 (ALLIN REJECTED - correct)\n");
        } else {
            printf("  Buggy: returned 0 (ALLIN ALLOWED - unexpected)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should also return -1 (all-in rejected)
        int result5_fixed = checkMyAction_Allin_Fixed(5, targetBet5, highestSet5, &player5);
        if (result5_fixed == -1) {
            printf("  Fixed: returned -1 (ALLIN REJECTED - correct)\n");
        } else {
            printf("  Fixed: returned 0 (ALLIN ALLOWED - unexpected)\n");
            allPassed = false;
            failures++;
        }
    }

    printf("\n");
    if (!allPassed) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: PLAYER_ACTION_ALLIN bug confirmed and fix verified\n");
    return 0;
}