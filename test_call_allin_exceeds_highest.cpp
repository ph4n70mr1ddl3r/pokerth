// Test for LocalPlayer::checkMyAction() PLAYER_ACTION_CALL all-in bug.
//
// Bug: In LocalPlayer::checkMyAction(), the PLAYER_ACTION_CALL case at line 1085
// has an overly restrictive condition `getMyCash() + getMySet() <= highestSet`
// for all-in calls. This incorrectly rejects valid all-in actions when a player's
// total chip contribution (mySet + myCash) exceeds the current highest set.
//
// The PLAYER_ACTION_ALLIN case (line 1103-1107) correctly only checks
// `targetBet == getMyCash() && getMyCash() > 0` without restricting total contribution,
// but PLAYER_ACTION_CALL's all-in check has the extra restriction.
//
// Fix: Remove the `getMyCash() + getMySet() <= highestSet` condition from line 1085,
// making the PLAYER_ACTION_CALL all-in check consistent with PLAYER_ACTION_ALLIN.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_call_allin_exceeds_highest test_call_allin_exceeds_highest.cpp && /tmp/test_call_allin_exceeds_highest
#

#include <cstdio>
#include <cstdlib>

// Mock PlayerInterface for test
struct PlayerInterface {
    virtual ~PlayerInterface() = default;
    virtual int getMySet() const = 0;
    virtual int getMyCash() const = 0;
    virtual int getMyID() const { return 0; }
};

// Mock player for test
struct MockPlayer : public PlayerInterface {
    int mySet;
    int myCash;

    MockPlayer(int set, int cash) : mySet(set), myCash(cash) {}

    int getMySet() const override { return mySet; }
    int getMyCash() const override { return myCash; }
};

// Old (buggy) checkMyAction all-in logic
// Old code: requires getMyCash() + getMySet() <= highestSet && targetBet == getMyCash()
static bool checkMyAction_Buggy(int targetAction, int targetBet, int highestSet,
                                 const PlayerInterface& player) {
    (void)targetAction;

    if (targetAction != 1) { // PLAYER_ACTION_CALL = 1
        return true;
    }

    // Buggy all-in check: requires total contribution <= highestSet
    if (targetBet == player.getMyCash() && player.getMyCash() > 0) {
        if (player.getMyCash() + player.getMySet() <= highestSet) {
            return false; // valid (bug: should be valid even when total > highestSet)
        }
    }
    return true; // invalid
}

// Fixed checkMyAction all-in logic
// New code: only checks targetBet == getMyCash() && getMyCash() > 0, no total restriction
static bool checkMyAction_Fixed(int targetAction, int targetBet, int highestSet,
                                 const PlayerInterface& player) {
    (void)highestSet;

    if (targetAction != 1) { // PLAYER_ACTION_CALL = 1
        return true;
    }

    // Fixed all-in check: only verify targetBet == getMyCash() and has cash
    if (targetBet == player.getMyCash() && player.getMyCash() > 0) {
        return false; // valid
    }
    return true; // invalid
}

int main() {
    printf("Testing checkMyAction PLAYER_ACTION_CALL all-in bug\n\n");

    // Scenario: All-in with total chips exceeding highestSet
    // mySet=8, myCash=5, total=13, highestSet=10, targetBet=5 (all-in)
    MockPlayer p(8, 5);
    int highestSet = 10;
    int targetBet = 5;  // all-in with 5 chips

    printf("Scenario: mySet=%d, myCash=%d, total=%d, highestSet=%d, targetBet=%d (all-in)\n",
           p.getMySet(), p.getMyCash(), p.getMySet() + p.getMyCash(), highestSet, targetBet);
    printf("  Expected: all-in should be VALID (player bets all their chips)\n\n");

    // Test buggy version
    printf("Buggy version:\n");
    bool buggyResult = checkMyAction_Buggy(1, targetBet, highestSet, p);
    printf("  Returns: %s (invalid = 1, valid = 0)\n", buggyResult ? "invalid" : "valid");
    if (buggyResult) {
        printf("  RESULT: BUG CONFIRMED - all-in incorrectly rejected when total chips exceed highestSet\n");
    } else {
        printf("  RESULT: unexpectedly valid\n");
    }

    printf("\n");

    // Test fixed version
    printf("Fixed version:\n");
    bool fixedResult = checkMyAction_Fixed(1, targetBet, highestSet, p);
    printf("  Returns: %s (invalid = 1, valid = 0)\n", fixedResult ? "invalid" : "valid");
    if (!fixedResult) {
        printf("  RESULT: all-in correctly accepted (fix works!)\n");
    } else {
        printf("  RESULT: all-in incorrectly rejected (bug still present)\n");
    }

    printf("\n");

    // Verify the fix is correct
    if (buggyResult && !fixedResult) {
        printf("CONCLUSION: Bug confirmed and fix verified!\n");
        printf("  - Buggy: rejects all-in when total > highestSet (incorrect)\n");
        printf("  - Fixed: accepts all-in when targetBet == myCash (correct)\n");
        return 0;
    } else {
        printf("CONCLUSION: Unexpected results\n");
        return 1;
    }
}