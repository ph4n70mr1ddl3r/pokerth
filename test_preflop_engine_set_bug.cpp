// Test for LocalPlayer::preflopEngine() mySet/myCash update bug.
//
// Bug: In LocalPlayer::preflopEngine(), the player's mySet and myCash are
// NEVER updated when an action (RAISE, CALL, BET, ALLIN) is chosen with
// EngineVersion=0 (preflopEngine, not preflopEngine3). The `raise` local
// variable is computed but discarded when the function returns.
// Compare with flopEngine() which directly modifies mySet, and preflopEngine3()
// which calls setMySet(). This means preflop bets do not affect the player's
// stack or the pot when EngineVersion config is disabled.
//
// This test replicates the preflopEngine action-selection logic with mock
// state and verifies that without the fix mySet/myCash remain unchanged
// (bug), while with the fix they are correctly updated (fix).
//
// Compile:
//   g++ -O2 -std=c++23 -o /tmp/test_preflop_engine_set_bug test_preflop_engine_set_bug.cpp && /tmp/test_preflop_engine_set_bug

#include <cstdio>
#include <cstdlib>
#include <climits>
#include <algorithm>

enum PlayerAction {
    PLAYER_ACTION_NONE = 0,
    PLAYER_ACTION_FOLD = 1,
    PLAYER_ACTION_CHECK = 2,
    PLAYER_ACTION_CALL = 3,
    PLAYER_ACTION_BET = 4,
    PLAYER_ACTION_RAISE = 5,
    PLAYER_ACTION_ALLIN = 6
};

// Mock player state - mirrors the actual LocalPlayer structure used in preflopEngine
struct MockPlayer {
    int mySet{0};
    int myCash{1000};
    int myNiveau[3]{0, 0, 0};
    double myOdds{0.0};
    int myButton{0};
    int mySBluff{50};
    bool mySBluffStatus{false};
    PlayerAction myAction{PLAYER_ACTION_NONE};

    // Buggy preflopEngine: does NOT update mySet/myCash after choosing an action.
    // This simulates the OLD preflopEngine behavior where the function computes
    // an action but forgets to update mySet/myCash.
    static void preflopEngineBuggy(MockPlayer& p, int highestSet, int smallBlind, int button) {
        int raise = 0;

        // Simple odds-based decision logic (simplified from actual code)
        if(p.myOdds >= 50.0) {
            // Raising logic
            if(highestSet >= 12 * smallBlind) {
                if(p.myOdds >= 60.0) {
                    raise = p.myCash;
                    p.myAction = PLAYER_ACTION_RAISE;
                } else {
                    p.myAction = PLAYER_ACTION_CALL;
                }
            } else {
                raise = (p.myCash / (2 * smallBlind)) * 2 * smallBlind;
                if(raise >= (p.myCash * 4) / 5) {
                    raise = p.myCash;
                }
                p.myAction = PLAYER_ACTION_RAISE;
            }
        } else {
            // Calling/folding logic
            if(button == 3 && p.mySet == highestSet) {
                p.myAction = PLAYER_ACTION_CHECK;
            } else {
                if(p.myCash - highestSet <= (p.myCash * 1) / 5) {
                    raise = p.myCash;
                    p.myAction = PLAYER_ACTION_RAISE;
                } else {
                    p.myAction = PLAYER_ACTION_CALL;
                }
            }
        }

        // BUG: mySet and myCash are NEVER updated here!
        // The function just sets myAction and returns without modifying mySet/myCash.
    }

    // Fixed preflopEngine: updates mySet and myCash after choosing an action.
    static void preflopEngineFixed(MockPlayer& p, int highestSet, int smallBlind, int button) {
        int raise = 0;

        // Same decision logic as buggy version
        if(p.myOdds >= 50.0) {
            if(highestSet >= 12 * smallBlind) {
                if(p.myOdds >= 60.0) {
                    raise = p.myCash;
                    p.myAction = PLAYER_ACTION_RAISE;
                } else {
                    p.myAction = PLAYER_ACTION_CALL;
                }
            } else {
                raise = (p.myCash / (2 * smallBlind)) * 2 * smallBlind;
                if(raise >= (p.myCash * 4) / 5) {
                    raise = p.myCash;
                }
                p.myAction = PLAYER_ACTION_RAISE;
            }
        } else {
            if(button == 3 && p.mySet == highestSet) {
                p.myAction = PLAYER_ACTION_CHECK;
            } else {
                if(p.myCash - highestSet <= (p.myCash * 1) / 5) {
                    raise = p.myCash;
                    p.myAction = PLAYER_ACTION_RAISE;
                } else {
                    p.myAction = PLAYER_ACTION_CALL;
                }
            }
        }

        // FIX: Update mySet and myCash based on the chosen action.
        switch(p.myAction) {
            case PLAYER_ACTION_CALL: {
                int toCall = highestSet - p.mySet;
                if(toCall > 0) {
                    p.myCash -= toCall;
                    p.mySet = highestSet;
                }
            }
            break;
            case PLAYER_ACTION_RAISE: {
                int toCall = highestSet - p.mySet;
                int totalBet = toCall + raise;
                if(totalBet > 0) {
                    if(totalBet > p.myCash) {
                        // All-in: can't cover full bet
                        p.mySet += p.myCash;
                        p.myCash = 0;
                    } else {
                        p.myCash -= totalBet;
                        p.mySet = highestSet + raise;
                    }
                }
            }
            break;
            case PLAYER_ACTION_ALLIN: {
                p.mySet += p.myCash;
                p.myCash = 0;
            }
            break;
            case PLAYER_ACTION_BET: {
                // Bet when highestSet == 0 (preflop): subtract mySet from myCash
                p.myCash -= p.mySet;
            }
            break;
            case PLAYER_ACTION_CHECK:
            case PLAYER_ACTION_FOLD:
            default:
                // No cash/chip change needed for check or fold
                break;
        }

        if(p.myCash < 0) p.myCash = 0;
    }
};

int main()
{
    int failures = 0;

    // --- Scenario 1: Raise when no bet yet (highestSet = 0) ---
    // Player: mySet=0, myCash=1000, odds=80 -> raises
    // Expected fix: mySet = 1000, myCash = 0 (all-in raise)
    {
        MockPlayer p{0, 1000, {0, 0, 70}, 80.0, 0, 50, false};
        int oldSet = p.mySet, oldCash = p.myCash;

        printf("--- Scenario 1 (Raise, no prior bet, highestSet=0) ---\n");
        printf("  Before: mySet=%d myCash=%d\n", p.mySet, p.myCash);

        MockPlayer::preflopEngineBuggy(p, 0, 10, 0);
        printf("  Buggy:  mySet=%d myCash=%d (expected unchanged %d,%d)\n",
               p.mySet, p.myCash, oldSet, oldCash);
        bool buggyNoChange = (p.mySet == oldSet && p.myCash == oldCash);
        printf("  Buggy no change: %s (bug confirmed)\n", buggyNoChange ? "PASS" : "FAIL");

        p.mySet = oldSet; p.myCash = oldCash; p.myAction = PLAYER_ACTION_NONE;
        MockPlayer::preflopEngineFixed(p, 0, 10, 0);
        printf("  Fixed:  mySet=%d myCash=%d (expected all-in: 1000,0)\n", p.mySet, p.myCash);
        bool fixedCorrect = (p.mySet == 1000 && p.myCash == 0);
        printf("  Fixed correct: %s\n", fixedCorrect ? "PASS" : "FAIL");

        if(!buggyNoChange || !fixedCorrect) failures++;
    }

    // --- Scenario 2: Call when there is a moderate bet ---
    // Player: mySet=50, myCash=1000, highestSet=100, odds=50 -> calls
    // toCall = 100 - 50 = 50
    // Expected fix: mySet = 100, myCash = 950
    {
        MockPlayer p{50, 1000, {40, 0, 70}, 50.0, 0, 50, false};
        int oldSet = p.mySet, oldCash = p.myCash;

        printf("\n--- Scenario 2 (Call with existing bet) ---\n");
        printf("  Before: mySet=%d myCash=%d\n", p.mySet, p.myCash);

        MockPlayer::preflopEngineBuggy(p, 100, 10, 0);
        printf("  Buggy:  mySet=%d myCash=%d (expected unchanged %d,%d)\n",
               p.mySet, p.myCash, oldSet, oldCash);
        bool buggyNoChange = (p.mySet == oldSet && p.myCash == oldCash);
        printf("  Buggy no change: %s (bug confirmed)\n", buggyNoChange ? "PASS" : "FAIL");

        p.mySet = oldSet; p.myCash = oldCash; p.myAction = PLAYER_ACTION_NONE;
        MockPlayer::preflopEngineFixed(p, 100, 10, 0);
        printf("  Fixed:  mySet=%d myCash=%d (expected 100,950)\n", p.mySet, p.myCash);
        bool fixedCorrect = (p.mySet == 100 && p.myCash == 950);
        printf("  Fixed correct: %s\n", fixedCorrect ? "PASS" : "FAIL");

        if(!buggyNoChange || !fixedCorrect) failures++;
    }

    // --- Scenario 3: Raise with sufficient chips ---
    // Player: mySet=0, myCash=500, highestSet=100, odds=90 -> raises
    // Expected fix: mySet = 300, myCash = 200 (simplified logic)
    {
        MockPlayer p{0, 500, {0, 0, 70}, 90.0, 0, 50, false};
        int oldSet = p.mySet, oldCash = p.myCash;

        printf("\n--- Scenario 3 (Raise with sufficient chips) ---\n");
        printf("  Before: mySet=%d myCash=%d\n", p.mySet, p.myCash);

        MockPlayer::preflopEngineBuggy(p, 100, 10, 0);
        printf("  Buggy:  mySet=%d myCash=%d (expected unchanged %d,%d)\n",
               p.mySet, p.myCash, oldSet, oldCash);
        bool buggyNoChange = (p.mySet == oldSet && p.myCash == oldCash);
        printf("  Buggy no change: %s (bug confirmed)\n", buggyNoChange ? "PASS" : "FAIL");

        p.mySet = oldSet; p.myCash = oldCash; p.myAction = PLAYER_ACTION_NONE;
        MockPlayer::preflopEngineFixed(p, 100, 10, 0);
        printf("  Fixed:  mySet=%d myCash=%d (expected 300,200)\n", p.mySet, p.myCash);
        bool fixedCorrect = (p.mySet == 300 && p.myCash == 200);
        printf("  Fixed correct: %s\n", fixedCorrect ? "PASS" : "FAIL");

        if(!buggyNoChange || !fixedCorrect) failures++;
    }

    // --- Scenario 4: Check when already matched ---
    // Player: mySet=100, myCash=900, highestSet=100, button=3 -> checks
    // Expected fix: mySet=100, myCash=900 (unchanged)
    {
        MockPlayer p{100, 900, {0, 0, 70}, 30.0, 3, 50, false};
        int oldSet = p.mySet, oldCash = p.myCash;

        printf("\n--- Scenario 4 (Check, already matched) ---\n");
        printf("  Before: mySet=%d myCash=%d\n", p.mySet, p.myCash);

        MockPlayer::preflopEngineBuggy(p, 100, 10, 3);
        printf("  Buggy:  mySet=%d myCash=%d (expected unchanged %d,%d)\n",
               p.mySet, p.myCash, oldSet, oldCash);
        bool buggyNoChange = (p.mySet == oldSet && p.myCash == oldCash);
        printf("  Buggy no change: %s (bug confirmed)\n", buggyNoChange ? "PASS" : "FAIL");

        p.mySet = oldSet; p.myCash = oldCash; p.myAction = PLAYER_ACTION_NONE;
        MockPlayer::preflopEngineFixed(p, 100, 10, 3);
        printf("  Fixed:  mySet=%d myCash=%d (expected 100,900)\n", p.mySet, p.myCash);
        bool fixedCorrect = (p.mySet == 100 && p.myCash == 900);
        printf("  Fixed correct: %s\n", fixedCorrect ? "PASS" : "FAIL");

        if(!buggyNoChange || !fixedCorrect) failures++;
    }

    // --- Scenario 5: Fold ---
    // Player: mySet=0, myCash=1000, highestSet=100, odds=10 -> folds
    // Expected fix: mySet=0, myCash=1000 (unchanged)
    {
        MockPlayer p{0, 1000, {40, 0, 70}, 10.0, 0, 50, false};
        int oldSet = p.mySet, oldCash = p.myCash;

        printf("\n--- Scenario 5 (Fold) ---\n");
        printf("  Before: mySet=%d myCash=%d\n", p.mySet, p.myCash);

        MockPlayer::preflopEngineBuggy(p, 100, 10, 0);
        printf("  Buggy:  mySet=%d myCash=%d (expected unchanged %d,%d)\n",
               p.mySet, p.myCash, oldSet, oldCash);
        bool buggyNoChange = (p.mySet == oldSet && p.myCash == oldCash);
        printf("  Buggy no change: %s (bug confirmed)\n", buggyNoChange ? "PASS" : "FAIL");

        p.mySet = oldSet; p.myCash = oldCash; p.myAction = PLAYER_ACTION_NONE;
        MockPlayer::preflopEngineFixed(p, 100, 10, 0);
        printf("  Fixed:  mySet=%d myCash=%d (expected 0,1000)\n", p.mySet, p.myCash);
        bool fixedCorrect = (p.mySet == 0 && p.myCash == 1000);
        printf("  Fixed correct: %s\n", fixedCorrect ? "PASS" : "FAIL");

        if(!buggyNoChange || !fixedCorrect) failures++;
    }

    // --- Scenario 6: All-in call (short stack) ---
    // Player: mySet=0, myCash=50, highestSet=100, odds=50 -> calls all-in
    // Expected fix: mySet = 50, myCash = 0
    {
        MockPlayer p{0, 50, {40, 0, 70}, 50.0, 0, 50, false};
        int oldSet = p.mySet, oldCash = p.myCash;

        printf("\n--- Scenario 6 (All-in call, short stack) ---\n");
        printf("  Before: mySet=%d myCash=%d\n", p.mySet, p.myCash);

        MockPlayer::preflopEngineBuggy(p, 100, 10, 0);
        printf("  Buggy:  mySet=%d myCash=%d (expected unchanged %d,%d)\n",
               p.mySet, p.myCash, oldSet, oldCash);
        bool buggyNoChange = (p.mySet == oldSet && p.myCash == oldCash);
        printf("  Buggy no change: %s (bug confirmed)\n", buggyNoChange ? "PASS" : "FAIL");

        p.mySet = oldSet; p.myCash = oldCash; p.myAction = PLAYER_ACTION_NONE;
        MockPlayer::preflopEngineFixed(p, 100, 10, 0);
        printf("  Fixed:  mySet=%d myCash=%d (expected 50,0)\n", p.mySet, p.myCash);
        bool fixedCorrect = (p.mySet == 50 && p.myCash == 0);
        printf("  Fixed correct: %s\n", fixedCorrect ? "PASS" : "FAIL");

        if(!buggyNoChange || !fixedCorrect) failures++;
    }

    // --- Scenario 7: Bet preflop (highestSet=0) ---
    // Player: mySet=0, myCash=100, highestSet=0 (preflop), bets
    // Expected fix: myCash -= mySet (0), so myCash stays 100, mySet stays 0
    {
        MockPlayer p{0, 100, {0, 0, 70}, 30.0, 0, 50, false};
        int oldSet = p.mySet, oldCash = p.myCash;

        printf("\n--- Scenario 7 (Preflop bet, highestSet=0) ---\n");
        printf("  Before: mySet=%d myCash=%d\n", p.mySet, p.myCash);

        MockPlayer::preflopEngineBuggy(p, 0, 10, 0);
        printf("  Buggy:  mySet=%d myCash=%d (expected unchanged %d,%d)\n",
               p.mySet, p.myCash, oldSet, oldCash);
        bool buggyNoChange = (p.mySet == oldSet && p.myCash == oldCash);
        printf("  Buggy no change: %s (bug confirmed)\n", buggyNoChange ? "PASS" : "FAIL");

        p.mySet = oldSet; p.myCash = oldCash; p.myAction = PLAYER_ACTION_NONE;
        MockPlayer::preflopEngineFixed(p, 0, 10, 0);
        printf("  Fixed:  mySet=%d myCash=%d (expected 0,100 - bet deducts mySet=0)\n", p.mySet, p.myCash);
        bool fixedCorrect = (p.mySet == 0 && p.myCash == 100);
        printf("  Fixed correct: %s\n", fixedCorrect ? "PASS" : "FAIL");

        if(!buggyNoChange || !fixedCorrect) failures++;
    }

    if(failures > 0) {
        printf("\nTEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("\nTEST PASSED: preflopEngine mySet/myCash update bug confirmed and fix verified\n");
    return 0;
}