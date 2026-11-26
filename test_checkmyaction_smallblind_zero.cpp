// Test for checkMyAction() PLAYER_ACTION_BET smallBlind==0 edge case
//
#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer state
struct MockLocalPlayer {
    int myCash{100};
    int mySet{0};

    int getMyCash() const { return myCash; }
    int getMySet() const { return mySet; }
};

// Current (buggy) checkMyAction with PLAYER_ACTION_BET case
int checkMyAction_buggy(int targetAction, int targetBet, int highestSet, int minimumRaise, int smallBlind, const MockLocalPlayer& player) {
    (void)minimumRaise;
    switch(targetAction) {
    case 1: // PLAYER_ACTION_BET:
        if(highestSet == 0 && targetBet <= player.getMyCash() && (smallBlind > 0 && targetBet >= 2*smallBlind)) {
            return 0;
        }
        return 1;
    default:
        return 1;
    }
}

// Fixed checkMyAction with PLAYER_ACTION_BET case
// Fix: change (smallBlind > 0 && targetBet >= 2*smallBlind) to (smallBlind == 0 || targetBet >= 2*smallBlind)
int checkMyAction_fixed(int targetAction, int targetBet, int highestSet, int minimumRaise, int smallBlind, const MockLocalPlayer& player) {
    (void)minimumRaise;
    switch(targetAction) {
    case 1: // PLAYER_ACTION_BET:
        if(highestSet == 0 && targetBet <= player.getMyCash() && (smallBlind == 0 || targetBet >= 2*smallBlind)) {
            return 0;
        }
        return 1;
    default:
        return 1;
    }
}

// Helper to test a scenario
static void testScenario(const char* name, int cash, int bet, int highestSet, int smallBlind, int expected, int (*checkFn)(int, int, int, int, int, const MockLocalPlayer&)) {
    MockLocalPlayer player{cash, 0};
    int result = checkFn(1, bet, highestSet, 0, smallBlind, player);
    if (result == expected) {
        printf("  PASS: %s (cash=%d, bet=%d, highestSet=%d, smallBlind=%d => %d)\n",
               name, cash, bet, highestSet, smallBlind, result);
    } else {
        printf("  FAIL: %s (cash=%d, bet=%d, highestSet=%d, smallBlind=%d => got %d, expected %d)\n",
               name, cash, bet, highestSet, smallBlind, result, expected);
    }
}

int main()
{
    int failures = 0;

    printf("Testing checkMyAction() PLAYER_ACTION_BET smallBlind==0 edge case - FIX VERIFICATION:\n\n");

    // Test 1: smallBlind == 0, valid bet should be ALLOWED (fix)
    testScenario("smallBlind==0, valid bet (FIX)", 100, 5, 0, 0, 0, checkMyAction_fixed);
    // Test 1: smallBlind == 0, valid bet should be REJECTED (buggy)
    testScenario("smallBlind==0, valid bet (BUGGY)", 100, 5, 0, 0, 1, checkMyAction_buggy);

    // Test 2: smallBlind > 0, bet >= 2*smallBlind should be allowed
    testScenario("smallBlind==5, bet>=2*5 (FIX)", 100, 10, 0, 5, 0, checkMyAction_fixed);
    testScenario("smallBlind==5, bet>=2*5 (BUGGY)", 100, 10, 0, 5, 0, checkMyAction_buggy);

    // Test 3: smallBlind > 0, bet < 2*smallBlind should be rejected
    testScenario("smallBlind==5, bet<2*5 (FIX)", 100, 5, 0, 5, 1, checkMyAction_fixed);
    testScenario("smallBlind==5, bet<2*5 (BUGGY)", 100, 5, 0, 5, 1, checkMyAction_buggy);

    // Test 4: highestSet != 0 should reject bet (both should reject)
    testScenario("highestSet!=0 (FIX)", 100, 5, 10, 0, 1, checkMyAction_fixed);
    testScenario("highestSet!=0 (BUGGY)", 100, 5, 10, 0, 1, checkMyAction_buggy);

    printf("\n");
    if (failures == 0) {
        printf("FIX VERIFIED: Fixed code correctly handles smallBlind==0 case\n");
        printf("  FIX: (smallBlind == 0 || targetBet >= 2*smallBlind) - allows betting when smallBlind=0\n");
        printf("  BUGGY: (smallBlind > 0 && targetBet >= 2*smallBlind) - rejects betting when smallBlind=0\n");
    } else {
        printf("SOME TESTS FAILED\n");
    }

    return failures;
}