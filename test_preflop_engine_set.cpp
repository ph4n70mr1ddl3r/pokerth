// Test for LocalPlayer::preflopEngine() mySet/myCash update bug.
//
// Bug: In LocalPlayer::preflopEngine(), the player's mySet and myCash are
// NEVER updated when an action (RAISE, CALL, BET, ALLIN) is chosen. The
// `raise` local variable is computed but discarded when the function returns.
// Compare with flopEngine() which directly modifies mySet, and preflopEngine3()
// which calls setMySet(). This means preflop bets do not affect the player's
// stack or the pot when EngineVersion config is disabled.
//
// This test replicates the preflopEngine action-selection logic with mock
// state and verifies that without the fix mySet/myCash remain unchanged
// (bug), while with the fix they are correctly updated (fix).
//
// Compile: g++ -O2 -std=c++23 -o test_preflop_engine_set test_preflop_engine_set.cpp && ./test_preflop_engine_set

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

struct MockPlayer {
    int mySet{0};
    int myCash{1000};
    int myNiveau[3]{0, 0, 0};
    double myOdds{0.0};
    int myButton{0};
    int mySBluff{50};
    bool mySBluffStatus{false};
    PlayerAction myAction{PLAYER_ACTION_NONE};
};

struct MockBeRo {
    int highestSet{0};
    int smallBlind{10};
    int getHighestSet() const { return highestSet; }
    int getSmallBlind() const { return smallBlind; }
};

// Buggy preflopEngine: does NOT update mySet/myCash after choosing an action.
static void preflopEngineBuggy(MockPlayer& p, const MockBeRo& ber) {
    int raise = 0;

    if(p.myOdds >= p.myNiveau[2]) {
        if(ber.getHighestSet() >= 12 * ber.getSmallBlind()) {
            if(p.myOdds >= p.myNiveau[2] + 8) {
                raise = p.myCash;
                p.myAction = PLAYER_ACTION_RAISE;
            } else {
                if(p.myCash - ber.getHighestSet() <= (p.myCash * 1) / 5) {
                    raise = p.myCash;
                    p.myAction = PLAYER_ACTION_RAISE;
                } else {
                    p.myAction = PLAYER_ACTION_CALL;
                }
            }
        } else {
            raise = ((static_cast<int>(p.myOdds) - p.myNiveau[2]) / 2) * 2 * ber.getSmallBlind();
            if(p.myCash / (2 * ber.getSmallBlind()) <= 6 || raise >= (p.myCash * 4) / 5) {
                raise = p.myCash;
            }
            p.myAction = PLAYER_ACTION_RAISE;
        }
    } else {
        if(p.myOdds >= p.myNiveau[0]) {
            if(p.myButton == 3 && p.mySet == ber.getHighestSet()) {
                p.myAction = PLAYER_ACTION_CHECK;
            } else {
                if(p.myCash - ber.getHighestSet() <= (p.myCash * 1) / 5) {
                    raise = p.myCash;
                    p.myAction = PLAYER_ACTION_RAISE;
                } else {
                    p.myAction = PLAYER_ACTION_CALL;
                }
            }
        } else {
            if(p.myButton == 3 && p.mySet == ber.getHighestSet()) {
                p.myAction = PLAYER_ACTION_CHECK;
            } else {
                p.myAction = PLAYER_ACTION_FOLD;
            }
        }
    }

    int bluffThreshold = INT_MAX;
    if(((p.mySBluff < bluffThreshold) && p.myOdds < p.myNiveau[2] &&
        ber.getHighestSet() == 2 * ber.getSmallBlind() && !p.mySBluffStatus) || p.mySBluffStatus) {
        p.mySBluffStatus = true;
        if(ber.getHighestSet() >= 4 * ber.getSmallBlind()) {
            if(p.myCash - ber.getHighestSet() <= (p.myCash * 1) / 6) {
                raise = p.myCash;
                p.myAction = PLAYER_ACTION_RAISE;
            } else {
                p.myAction = PLAYER_ACTION_CALL;
            }
        } else {
            raise = (p.mySBluff / 6) * ber.getSmallBlind();
            if(p.myCash / (2 * ber.getSmallBlind()) <= 6 || raise >= (p.myCash * 4) / 5) {
                raise = p.myCash;
            }
            p.myAction = PLAYER_ACTION_RAISE;
        }
    }

    if((ber.getHighestSet() >= 12 * ber.getSmallBlind() && p.myOdds < p.myNiveau[0]) ||
       (ber.getHighestSet() >= 20 * ber.getSmallBlind() && p.myOdds < p.myNiveau[2])) {
        p.myAction = PLAYER_ACTION_FOLD;
    }

    // BUG: mySet and myCash are NEVER updated here!
}

// Fixed preflopEngine: updates mySet and myCash after choosing an action.
static void preflopEngineFixed(MockPlayer& p, const MockBeRo& ber) {
    int raise = 0;

    if(p.myOdds >= p.myNiveau[2]) {
        if(ber.getHighestSet() >= 12 * ber.getSmallBlind()) {
            if(p.myOdds >= p.myNiveau[2] + 8) {
                raise = p.myCash;
                p.myAction = PLAYER_ACTION_RAISE;
            } else {
                if(p.myCash - ber.getHighestSet() <= (p.myCash * 1) / 5) {
                    raise = p.myCash;
                    p.myAction = PLAYER_ACTION_RAISE;
                } else {
                    p.myAction = PLAYER_ACTION_CALL;
                }
            }
        } else {
            raise = ((static_cast<int>(p.myOdds) - p.myNiveau[2]) / 2) * 2 * ber.getSmallBlind();
            if(p.myCash / (2 * ber.getSmallBlind()) <= 6 || raise >= (p.myCash * 4) / 5) {
                raise = p.myCash;
            }
            p.myAction = PLAYER_ACTION_RAISE;
        }
    } else {
        if(p.myOdds >= p.myNiveau[0]) {
            if(p.myButton == 3 && p.mySet == ber.getHighestSet()) {
                p.myAction = PLAYER_ACTION_CHECK;
            } else {
                if(p.myCash - ber.getHighestSet() <= (p.myCash * 1) / 5) {
                    raise = p.myCash;
                    p.myAction = PLAYER_ACTION_RAISE;
                } else {
                    p.myAction = PLAYER_ACTION_CALL;
                }
            }
        } else {
            if(p.myButton == 3 && p.mySet == ber.getHighestSet()) {
                p.myAction = PLAYER_ACTION_CHECK;
            } else {
                p.myAction = PLAYER_ACTION_FOLD;
            }
        }
    }

    int bluffThreshold = INT_MAX;
    if(((p.mySBluff < bluffThreshold) && p.myOdds < p.myNiveau[2] &&
        ber.getHighestSet() == 2 * ber.getSmallBlind() && !p.mySBluffStatus) || p.mySBluffStatus) {
        p.mySBluffStatus = true;
        if(ber.getHighestSet() >= 4 * ber.getSmallBlind()) {
            if(p.myCash - ber.getHighestSet() <= (p.myCash * 1) / 6) {
                raise = p.myCash;
                p.myAction = PLAYER_ACTION_RAISE;
            } else {
                p.myAction = PLAYER_ACTION_CALL;
            }
        } else {
            raise = (p.mySBluff / 6) * ber.getSmallBlind();
            if(p.myCash / (2 * ber.getSmallBlind()) <= 6 || raise >= (p.myCash * 4) / 5) {
                raise = p.myCash;
            }
            p.myAction = PLAYER_ACTION_RAISE;
        }
    }

    if((ber.getHighestSet() >= 12 * ber.getSmallBlind() && p.myOdds < p.myNiveau[0]) ||
       (ber.getHighestSet() >= 20 * ber.getSmallBlind() && p.myOdds < p.myNiveau[2])) {
        p.myAction = PLAYER_ACTION_FOLD;
    }

    // FIX: Update mySet and myCash based on the chosen action.
    switch(p.myAction) {
        case PLAYER_ACTION_CALL: {
            int toCall = ber.getHighestSet() - p.mySet;
            if(toCall > 0) {
                p.myCash -= toCall;
                p.mySet = ber.getHighestSet();
            }
        }
        break;
        case PLAYER_ACTION_RAISE: {
            int toCall = ber.getHighestSet() - p.mySet;
            int totalBet = toCall + raise;
            if(totalBet > 0) {
                if(totalBet > p.myCash) {
                    // All-in: can't cover full bet
                    p.mySet += p.myCash;
                    p.myCash = 0;
                } else {
                    p.myCash -= totalBet;
                    p.mySet = ber.getHighestSet() + raise;
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
            p.myCash -= p.mySet;
        }
        break;
        case PLAYER_ACTION_CHECK:
        case PLAYER_ACTION_FOLD:
        default:
            break;
    }

    if(p.myCash < 0) p.myCash = 0;
}

int main() {
    int failures = 0;

    // --- Scenario 1: Raise when no bet yet (highestSet = 0) ---
    // Player: mySet=0, myCash=1000, odds=80, niveau[2]=70 -> raises
    // raise = ((80-70)/2)*2*10 = 100
    // Expected fix: mySet = 100, myCash = 900
    {
        MockPlayer p{0, 1000, {0, 0, 70}, 80.0, 0, 50, false};
        MockBeRo ber{0, 10};

        int oldSet = p.mySet, oldCash = p.myCash;
        preflopEngineBuggy(p, ber);
        printf("Scenario 1 (Raise, no prior bet):\n");
        printf("  Buggy:   mySet=%d myCash=%d (expected unchanged %d,%d)\n",
               p.mySet, p.myCash, oldSet, oldCash);
        bool buggyNoChange = (p.mySet == oldSet && p.myCash == oldCash);
        printf("  Buggy no change: %s (bug confirmed)\n", buggyNoChange ? "PASS" : "FAIL");

        p.mySet = oldSet; p.myCash = oldCash; p.myAction = PLAYER_ACTION_NONE;
        preflopEngineFixed(p, ber);
        printf("  Fixed:   mySet=%d myCash=%d (expected 100,900)\n", p.mySet, p.myCash);
        bool fixedCorrect = (p.mySet == 100 && p.myCash == 900);
        printf("  Fixed correct: %s\n", fixedCorrect ? "PASS" : "FAIL");

        if(!buggyNoChange || !fixedCorrect) failures++;
    }

    // --- Scenario 2: Call when there is a moderate bet ---
    // Player: mySet=50, myCash=1000, highestSet=100, odds=50, niveau[0]=40 -> calls
    // toCall = 100 - 50 = 50
    // Expected fix: mySet = 100, myCash = 950
    {
        MockPlayer p{50, 1000, {40, 0, 70}, 50.0, 0, 50, false};
        MockBeRo ber{100, 10};

        int oldSet = p.mySet, oldCash = p.myCash;
        preflopEngineBuggy(p, ber);
        printf("\nScenario 2 (Call with existing bet):\n");
        printf("  Buggy:   mySet=%d myCash=%d (expected unchanged %d,%d)\n",
               p.mySet, p.myCash, oldSet, oldCash);
        bool buggyNoChange = (p.mySet == oldSet && p.myCash == oldCash);
        printf("  Buggy no change: %s (bug confirmed)\n", buggyNoChange ? "PASS" : "FAIL");

        p.mySet = oldSet; p.myCash = oldCash; p.myAction = PLAYER_ACTION_NONE;
        preflopEngineFixed(p, ber);
        printf("  Fixed:   mySet=%d myCash=%d (expected 100,950)\n", p.mySet, p.myCash);
        bool fixedCorrect = (p.mySet == 100 && p.myCash == 950);
        printf("  Fixed correct: %s\n", fixedCorrect ? "PASS" : "FAIL");

        if(!buggyNoChange || !fixedCorrect) failures++;
    }

    // --- Scenario 3: Raise with sufficient chips ---
    // Player: mySet=0, myCash=500, highestSet=100, odds=90, niveau[2]=70 -> raises
    // raise = ((90-70)/2)*2*10 = 200
    // toCall = 100, totalBet = 300
    // Expected fix: mySet = 300, myCash = 200
    {
        MockPlayer p{0, 500, {0, 0, 70}, 90.0, 0, 50, false};
        MockBeRo ber{100, 10};

        int oldSet = p.mySet, oldCash = p.myCash;
        preflopEngineBuggy(p, ber);
        printf("\nScenario 3 (Raise with sufficient chips):\n");
        printf("  Buggy:   mySet=%d myCash=%d (expected unchanged %d,%d)\n",
               p.mySet, p.myCash, oldSet, oldCash);
        bool buggyNoChange = (p.mySet == oldSet && p.myCash == oldCash);
        printf("  Buggy no change: %s (bug confirmed)\n", buggyNoChange ? "PASS" : "FAIL");

        p.mySet = oldSet; p.myCash = oldCash; p.myAction = PLAYER_ACTION_NONE;
        preflopEngineFixed(p, ber);
        printf("  Fixed:   mySet=%d myCash=%d (expected 300,200)\n", p.mySet, p.myCash);
        bool fixedCorrect = (p.mySet == 300 && p.myCash == 200);
        printf("  Fixed correct: %s\n", fixedCorrect ? "PASS" : "FAIL");

        if(!buggyNoChange || !fixedCorrect) failures++;
    }

    // --- Scenario 4: Check when already matched ---
    // Player: mySet=100, myCash=900, highestSet=100, button=3 -> checks
    // Expected fix: mySet=100, myCash=900 (unchanged)
    {
        MockPlayer p{100, 900, {0, 0, 70}, 30.0, 3, 50, false};
        MockBeRo ber{100, 10};

        int oldSet = p.mySet, oldCash = p.myCash;
        preflopEngineBuggy(p, ber);
        printf("\nScenario 4 (Check, already matched):\n");
        printf("  Buggy:   mySet=%d myCash=%d (expected unchanged %d,%d)\n",
               p.mySet, p.myCash, oldSet, oldCash);
        bool buggyNoChange = (p.mySet == oldSet && p.myCash == oldCash);
        printf("  Buggy no change: %s (bug confirmed)\n", buggyNoChange ? "PASS" : "FAIL");

        p.mySet = oldSet; p.myCash = oldCash; p.myAction = PLAYER_ACTION_NONE;
        preflopEngineFixed(p, ber);
        printf("  Fixed:   mySet=%d myCash=%d (expected 100,900)\n", p.mySet, p.myCash);
        bool fixedCorrect = (p.mySet == 100 && p.myCash == 900);
        printf("  Fixed correct: %s\n", fixedCorrect ? "PASS" : "FAIL");

        if(!buggyNoChange || !fixedCorrect) failures++;
    }

    // --- Scenario 5: Fold ---
    // Player: mySet=0, myCash=1000, highestSet=100, odds=10, niveau[0]=40 -> folds
    // Expected fix: mySet=0, myCash=1000 (unchanged)
    {
        MockPlayer p{0, 1000, {40, 0, 70}, 10.0, 0, 50, false};
        MockBeRo ber{100, 10};

        int oldSet = p.mySet, oldCash = p.myCash;
        preflopEngineBuggy(p, ber);
        printf("\nScenario 5 (Fold):\n");
        printf("  Buggy:   mySet=%d myCash=%d (expected unchanged %d,%d)\n",
               p.mySet, p.myCash, oldSet, oldCash);
        bool buggyNoChange = (p.mySet == oldSet && p.myCash == oldCash);
        printf("  Buggy no change: %s (bug confirmed)\n", buggyNoChange ? "PASS" : "FAIL");

        p.mySet = oldSet; p.myCash = oldCash; p.myAction = PLAYER_ACTION_NONE;
        preflopEngineFixed(p, ber);
        printf("  Fixed:   mySet=%d myCash=%d (expected 0,1000)\n", p.mySet, p.myCash);
        bool fixedCorrect = (p.mySet == 0 && p.myCash == 1000);
        printf("  Fixed correct: %s\n", fixedCorrect ? "PASS" : "FAIL");

        if(!buggyNoChange || !fixedCorrect) failures++;
    }

    // --- Scenario 6: All-in call (short stack) ---
    // Player: mySet=0, myCash=50, highestSet=100, odds=50, niveau[0]=40 -> calls all-in
    // toCall = 100, but myCash=50, so all-in with 50
    // Expected fix: mySet = 50, myCash = 0
    {
        MockPlayer p{0, 50, {40, 0, 70}, 50.0, 0, 50, false};
        MockBeRo ber{100, 10};

        int oldSet = p.mySet, oldCash = p.myCash;
        preflopEngineBuggy(p, ber);
        printf("\nScenario 6 (All-in call, short stack):\n");
        printf("  Buggy:   mySet=%d myCash=%d (expected unchanged %d,%d)\n",
               p.mySet, p.myCash, oldSet, oldCash);
        bool buggyNoChange = (p.mySet == oldSet && p.myCash == oldCash);
        printf("  Buggy no change: %s (bug confirmed)\n", buggyNoChange ? "PASS" : "FAIL");

        p.mySet = oldSet; p.myCash = oldCash; p.myAction = PLAYER_ACTION_NONE;
        preflopEngineFixed(p, ber);
        printf("  Fixed:   mySet=%d myCash=%d (expected 50,0)\n", p.mySet, p.myCash);
        bool fixedCorrect = (p.mySet == 50 && p.myCash == 0);
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
