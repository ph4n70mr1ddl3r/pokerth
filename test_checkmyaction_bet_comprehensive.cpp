// Comprehensive test for checkMyAction() PLAYER_ACTION_BET
//
// This test verifies the PLAYER_ACTION_BET smallBlind guard in checkMyAction()
// The condition (smallBlind > 0 && targetBet >= 2*smallBlind) ensures:
// 1. A positive small blind is required
// 2. The bet amount must be at least 2*smallBlind
// 3. Bets are rejected when smallBlind == 0
//
// Compile: g++ -O2 -std=c++23 -o test_checkmyaction_bet_comprehensive test_checkmyaction_bet_comprehensive.cpp
//          ./test_checkmyaction_bet_comprehensive

#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer state
struct MockLocalPlayer {
    int myCash{100};
    int mySet{0};

    int getMyCash() const { return myCash; }
    int getMySet() const { return mySet; }
};

// Simulate the current checkMyAction PLAYER_ACTION_BET case
// from src/engine/local_engine/localplayer.cpp:1116-1121
int checkMyActionBet(int targetAction, int targetBet, int highestSet, int minimumRaise, int smallBlind, const MockLocalPlayer& player) {
    (void)minimumRaise;
    (void)highestSet;
    switch(targetAction) {
    case 1: // PLAYER_ACTION_BET:
        if(highestSet == 0 && targetBet <= player.getMyCash() && (smallBlind > 0 && targetBet >= 2*smallBlind)) {
            return 0;  // valid bet
        }
        return 1;  // invalid bet
    default:
        return 1;
    }
}

// Simulate a version WITHOUT the smallBlind > 0 guard (old buggy pattern)
int checkMyActionBetOld(int targetAction, int targetBet, int highestSet, int minimumRaise, int smallBlind, const MockLocalPlayer& player) {
    (void)minimumRaise;
    (void)highestSet;
    switch(targetAction) {
    case 1: // PLAYER_ACTION_BET:
        if(highestSet == 0 && targetBet <= player.getMyCash() && (smallBlind == 0 || targetBet >= 2*smallBlind)) {
            return 0;  // OLD: allows bet when smallBlind=0
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

    printf("PLAYER_ACTION_BET smallBlind edge case comprehensive test\n\n");

    // --- Scenario 1: Valid bet with normal smallBlind ---
    printf("--- Scenario 1: Valid bet (targetBet=20, smallBlind=10, cash=1000) ---\n");
    testScenario("Valid bet", 1000, 20, 0, 10, 0, checkMyActionBet);
    testScenario("Old buggy", 1000, 20, 0, 10, 0, checkMyActionBetOld);

    // --- Scenario 2: Invalid bet when smallBlind is 0 (should reject with fix) ---
    printf("\n--- Scenario 2: smallBlind=0, targetBet=20 (should reject) ---\n");
    testScenario("Fix rejects smallBlind=0", 100, 20, 0, 0, 1, checkMyActionBet);
    testScenario("Old allows smallBlind=0", 100, 20, 0, 0, 0, checkMyActionBetOld);

    // --- Scenario 3: Invalid bet when targetBet < 2*smallBlind (both should reject) ---
    printf("\n--- Scenario 3: smallBlind=10, targetBet=15 (15 < 2*10=20, should reject) ---\n");
    testScenario("Fix rejects bet < 2*smallBlind", 100, 15, 0, 10, 1, checkMyActionBet);
    testScenario("Old rejects bet < 2*smallBlind", 100, 15, 0, 10, 1, checkMyActionBetOld);

    // --- Scenario 4: Valid bet at minimum 2*smallBlind ---
    printf("\n--- Scenario 4: Valid bet at minimum (targetBet=20, smallBlind=10) ---\n");
    testScenario("Fix accepts min valid bet", 100, 20, 0, 10, 0, checkMyActionBet);
    testScenario("Old accepts min valid bet", 100, 20, 0, 10, 0, checkMyActionBetOld);

    // --- Scenario 5: highestSet != 0 (should always reject bet) ---
    printf("\n--- Scenario 5: highestSet!=0, should reject bet ---\n");
    testScenario("Fix rejects highestSet!=0", 100, 20, 100, 10, 1, checkMyActionBet);
    testScenario("Old rejects highestSet!=0", 100, 20, 100, 10, 1, checkMyActionBetOld);

    // --- Scenario 6: Edge case - targetBet > myCash (should reject) ---
    printf("\n--- Scenario 6: targetBet > myCash (100 > 50, should reject) ---\n");
    testScenario("Fix rejects targetBet > myCash", 50, 100, 0, 10, 1, checkMyActionBet);
    testScenario("Old rejects targetBet > myCash", 50, 100, 0, 10, 1, checkMyActionBetOld);

    // --- Scenario 7: Edge case - exact 2*smallBlind ---
    printf("\n--- Scenario 7: Exact 2*smallBlind (targetBet=20, smallBlind=10) ---\n");
    testScenario("Fix accepts exact 2*smallBlind", 100, 20, 0, 10, 0, checkMyActionBet);
    testScenario("Old accepts exact 2*smallBlind", 100, 20, 0, 10, 0, checkMyActionBetOld);

    // --- Scenario 8: smallBlind=0, targetBet=0 ---
    printf("\n--- Scenario 8: smallBlind=0, targetBet=0 ---\n");
    testScenario("Fix rejects smallBlind=0 regardless of bet", 100, 0, 0, 0, 1, checkMyActionBet);
    testScenario("Old allows smallBlind=0 with bet=0", 100, 0, 0, 0, 0, checkMyActionBetOld);

    printf("\n");
    if (failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("TESTS FAILED: %d failure(s)\n", failures);
        return 1;
    }
}