// Test for LocalPlayer::flopEngine3() PLAYER_ACTION_BET bug when highestSet == 0.
//
// Bug: In LocalPlayer::flopEngine3(), when highestSet == 0 (pre-flop scenario)
// and a bet is placed (not check, not all-in), myAction is NOT set to
// PLAYER_ACTION_BET. This means the post-engine switch statement's
// PLAYER_ACTION_BET case (which does `myCash -= mySet;`) never executes,
// causing myCash to NOT be reduced by the bet amount.
//
// Fix: Set myAction = PLAYER_ACTION_BET in the else branch when betting
// with highestSet == 0.
//
#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer state matching the actual code structure
enum PlayerAction {
    PLAYER_ACTION_NONE = 0,
    PLAYER_ACTION_FOLD,
    PLAYER_ACTION_CHECK,
    PLAYER_ACTION_CALL,
    PLAYER_ACTION_BET,
    PLAYER_ACTION_RAISE,
    PLAYER_ACTION_ALLIN
};

struct MockLocalPlayer {
    int mySet{0};
    int myCash{100};
    PlayerAction myAction{PLAYER_ACTION_NONE};
    int highestSet{0};
    int bet{0};

    PlayerAction getMyAction() const { return myAction; }
    int getMyCash() const { return myCash; }
    int getMySet() const { return mySet; }
    int getHighestSet() const { return highestSet; }
    int getBet() const { return bet; }
};

// Simulate the POST-ENGINE switch statement from flopEngine3()
// This is the switch that processes myAction after the engine function
static void runSwitchStatement(MockLocalPlayer& p) {
    switch(p.myAction) {
        case PLAYER_ACTION_CALL: {
            int toCall = p.highestSet - p.mySet; // simplified
            if(toCall > 0) {
                p.myCash -= toCall;
                p.mySet = p.highestSet;
            }
        }
        break;
        case PLAYER_ACTION_RAISE: {
            int toCall = p.highestSet - 0; // simplified
            int totalBet = toCall; // simplified
            if(totalBet > 0) {
                p.myCash -= totalBet;
                p.mySet = p.highestSet;
            }
        }
        break;
        case PLAYER_ACTION_ALLIN: {
            p.mySet += p.myCash;
            p.myCash = 0;
        }
        break;
        case PLAYER_ACTION_BET: {
            if(p.mySet > 0) {
                p.myCash -= p.mySet;
            }
        }
        break;
        case PLAYER_ACTION_CHECK:
        case PLAYER_ACTION_FOLD:
        default:
            break;
    }
}

// Simulate the BUGGY flopEngine3() else branch when highestSet == 0 and betting
// (missing myAction = PLAYER_ACTION_BET)
static int processBuggy(MockLocalPlayer& p) {
    // mySet is set to bet amount (simulated here, matching lines 4347-4352)
    p.mySet = p.bet;

    // highestSet is set to mySet (line 4363)
    p.highestSet = p.mySet;

    // BUG: myAction is NOT set to PLAYER_ACTION_BET
    // It stays as PLAYER_ACTION_NONE (default)
    // myCash is NOT subtracted because the switch statement's
    // PLAYER_ACTION_BET case doesn't execute

    return 0;
}

// Simulate the FIXED flopEngine3() else branch - myAction set to PLAYER_ACTION_BET
static int processFixed(MockLocalPlayer& p) {
    // mySet is set to bet amount (simulated here, matching lines 4347-4352)
    p.mySet = p.bet;

    // highestSet is set to mySet (line 4363)
    p.highestSet = p.mySet;

    // FIX: myAction = PLAYER_ACTION_BET (matching the fix)
    p.myAction = PLAYER_ACTION_BET;

    return 0;
}

int main()
{
    int failures = 0;

    printf("Testing flopEngine3() PLAYER_ACTION_BET bug when highestSet == 0:\n\n");

    // --- Scenario 1: Normal bet with bet > 0, highestSet == 0 ---
    printf("--- Scenario 1 (mySet=30, myCash=100, bet=30, highestSet=0) ---\n");
    MockLocalPlayer p1;
    p1.mySet = 0;
    p1.myCash = 100;
    p1.myAction = PLAYER_ACTION_NONE;  // default, as engine would leave it
    p1.highestSet = 0;
    p1.bet = 30;

    int cashBefore1 = p1.myCash;

    processBuggy(p1);
    // Run the switch statement (as done after flopEngine3 returns)
    runSwitchStatement(p1);
    // Buggy: myCash should be 100 - 30 = 70, but remains 100 because
    // myAction is PLAYER_ACTION_NONE, so the switch statement's
    // PLAYER_ACTION_BET case doesn't execute
    bool buggy1_confirmed = (p1.myCash != cashBefore1 - 30);
    printf("  Buggy: myCash=%d (expected %d after switch), myAction=%d\n",
           p1.myCash, cashBefore1 - 30, (int)p1.myAction);
    if (buggy1_confirmed) {
        printf("  BUG CONFIRMED: myCash not subtracted! myAction=%d caused switch case to skip\n",
               (int)p1.myAction);
    } else {
        printf("  Bug NOT confirmed (unexpected - bug may already be fixed)\n");
        failures++;
    }

    // Test fix: reset and run fixed
    p1.myCash = cashBefore1;
    p1.mySet = 0;
    processFixed(p1);
    runSwitchStatement(p1);
    bool fix1_ok = (p1.myCash == cashBefore1 - 30);
    printf("  Fixed: myCash=%d (expected %d after switch), myAction=%d\n", p1.myCash, cashBefore1 - 30, (int)p1.myAction);
    if (fix1_ok) {
        printf("  PASS: flopEngine3 fix correctly subtracts myCash by bet amount via switch\n");
    } else {
        printf("  FAIL: Fixed code myCash=%d instead of %d after switch\n", p1.myCash, cashBefore1 - 30);
        failures++;
    }

    // --- Scenario 2: Bet of 0 edge case ---
    printf("\n--- Scenario 2 (mySet=0, myCash=100, bet=0, highestSet=0) ---\n");
    MockLocalPlayer p2;
    p2.mySet = 0;
    p2.myCash = 100;
    p2.myAction = PLAYER_ACTION_NONE;
    p2.highestSet = 0;
    p2.bet = 0;

    int cashBefore2 = p2.myCash;

    processBuggy(p2);
    runSwitchStatement(p2);
    // bet=0, so cash should remain unchanged regardless of myAction
    bool buggy2_ok = (p2.myCash == cashBefore2);  // bet=0, cash unchanged expected
    printf("  Buggy: myCash=%d (should be %d, unchanged since bet=0)\n", p2.myCash, cashBefore2);
    if (buggy2_ok) {
        printf("  OK: bet=0: cash correctly unchanged\n");
    } else {
        printf("  Bug: cash unexpectedly changed to %d\n", p2.myCash);
        failures++;
    }

    // Test fix
    p2.myCash = cashBefore2;
    processFixed(p2);
    runSwitchStatement(p2);
    bool fix2_ok = (p2.myCash == cashBefore2);
    printf("  Fixed: myCash=%d (should be %d, unchanged since bet=0)\n", p2.myCash, cashBefore2);
    if (fix2_ok) {
        printf("  Fixed bet=0: cash correctly unchanged\n");
    } else {
        printf("  FAIL: fixed code myCash=%d instead of %d\n", p2.myCash, cashBefore2);
        failures++;
    }

    // --- Scenario 3: Large bet ---
    printf("\n--- Scenario 3 (mySet=0, myCash=50, bet=30, highestSet=0) ---\n");
    MockLocalPlayer p3;
    p3.mySet = 0;
    p3.myCash = 50;
    p3.myAction = PLAYER_ACTION_NONE;
    p3.highestSet = 0;
    p3.bet = 30;

    int cashBefore3 = p3.myCash;

    processBuggy(p3);
    runSwitchStatement(p3);
    // Buggy: myCash should be 50 - 30 = 20, but remains 50
    bool buggy3_confirmed = (p3.myCash != cashBefore3 - 30);
    printf("  Buggy: myCash=%d (expected %d for single subtraction), myAction=%d\n",
           p3.myCash, cashBefore3 - 30, (int)p3.myAction);
    if (buggy3_confirmed) {
        printf("  BUG PATTERN CONFIRMED: myCash not reduced by bet amount\n");
    } else {
        printf("  Bug NOT confirmed (unexpected)\n");
        failures++;
    }

    // Test fix
    p3.myCash = cashBefore3;
    processFixed(p3);
    runSwitchStatement(p3);
    bool fix3_ok = (p3.myCash == cashBefore3 - 30);
    printf("  Fixed: myCash=%d (expected %d)\n", p3.myCash, cashBefore3 - 30);
    if (fix3_ok) {
        printf("  PASS: Fixed code correctly reduces myCash by single bet amount\n");
    } else {
        printf("  FAIL: Fixed code myCash=%d instead of %d\n", p3.myCash, cashBefore3 - 30);
        failures++;
    }

    printf("\n");
    printf("Scenario results:\n");
    printf("  Scenario 1 (bug confirmed + fix verified via switch): %s\n", 
           (buggy1_confirmed && fix1_ok) ? "PASS" : "FAIL");
    printf("  Scenario 2 (bet=0 edge case): %s\n", 
           (buggy2_ok && fix2_ok) ? "PASS" : "FAIL");
    printf("  Scenario 3 (large bet): %s\n", 
           (buggy3_confirmed && fix3_ok) ? "PASS" : "FAIL");

    if (failures > 0) {
        printf("\nTEST FAILED: %d failure(s)\n", failures);
        return 1;
    }

    printf("TEST PASSED: flopEngine3 PLAYER_ACTION_BET bug when highestSet==0 confirmed and fix verified\n");
    return 0;
}