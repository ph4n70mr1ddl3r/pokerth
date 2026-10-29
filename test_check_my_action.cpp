// Test for LocalPlayer::checkMyAction() raise and all-in call validation bug.
//
// Bug 1: In LocalPlayer::checkMyAction() under PLAYER_ACTION_RAISE, the check was:
//   `if(highestSet > 0 && targetBet >= minimumRaise && targetBet <= getMyCash())`
// Because targetBet represents the relative bet (additional chips from cash),
// targetBet must satisfy `getMySet() + targetBet >= highestSet + minimumRaise`
// (i.e. cover calling highestSet plus at least minimumRaise).
// Checking only `targetBet >= minimumRaise` allowed:
//   - A pure CALL to be accepted as a RAISE when mySet == 0 and targetBet == minimumRaise == highestSet.
//   - Illegal UNDER-RAISES (raising by as little as 1 chip above highestSet).
//   - Blind under-raises where mySet > 0 was ignored.
//
// Bug 2: Under PLAYER_ACTION_CALL, all-in call was:
//   `if(getMyCash() + getMySet() <= highestSet) return 0;`
// It failed to check `targetBet == getMyCash()`, allowing a player to call all-in
// with less than their available cash stack.
//
// Fix:
//   - In PLAYER_ACTION_RAISE: require
//     `static_cast<long long>(getMySet()) + targetBet >= static_cast<long long>(highestSet) + minimumRaise`
//   - In PLAYER_ACTION_CALL (all-in): require `targetBet == getMyCash()`.

#include <cstdio>
#include <cstdlib>
#include <cassert>

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

    int checkMyAction(int targetAction, int targetBet, int highestSet, int minimumRaise, int smallBlind, bool applyFix) const
    {
        switch (targetAction) {
        case PLAYER_ACTION_FOLD: {
            return 0;
        }
        case PLAYER_ACTION_CHECK: {
            if (mySet == highestSet) {
                return 0;
            }
        }
        break;
        case PLAYER_ACTION_CALL: {
            if (mySet < highestSet && targetBet <= myCash) {
                // not all in
                if (myCash + mySet >= highestSet && targetBet == highestSet - mySet) {
                    return 0;
                }
                // all in
                if (applyFix) {
                    if (myCash + mySet <= highestSet && targetBet == myCash) {
                        return 0;
                    }
                } else {
                    if (myCash + mySet <= highestSet) {
                        return 0;
                    }
                }
            }
        }
        break;
        case PLAYER_ACTION_BET: {
            if (highestSet == 0 && targetBet <= myCash && targetBet >= 2 * smallBlind) {
                return 0;
            }
        }
        break;
        case PLAYER_ACTION_RAISE: {
            if (applyFix) {
                if (highestSet > 0 &&
                    static_cast<long long>(mySet) + targetBet >= static_cast<long long>(highestSet) + minimumRaise &&
                    targetBet <= myCash) {
                    return 0;
                }
            } else {
                if (highestSet > 0 && targetBet >= minimumRaise && targetBet <= myCash) {
                    return 0;
                }
            }
        }
        break;
        case PLAYER_ACTION_ALLIN: {
            if (targetBet == myCash) {
                return 0;
            }
        }
        break;
        default:
            break;
        }
        return 1;
    }
};

int main()
{
    int failures = 0;

    // --- Scenario 1: Call disguised as Raise ---
    // highestSet = 100, minimumRaise = 100. Player has mySet = 0, myCash = 500.
    // targetBet = 100 is just calling 100.
    {
        MockPlayer p{0, 500};
        int buggy = p.checkMyAction(PLAYER_ACTION_RAISE, 100, 100, 100, 10, false);
        int fixed = p.checkMyAction(PLAYER_ACTION_RAISE, 100, 100, 100, 10, true);
        printf("Scenario 1 (Call disguised as Raise):\n");
        printf("  Buggy: %d (expected 0, bug allows call as raise)\n", buggy);
        printf("  Fixed: %d (expected 1, rejected)\n", fixed);
        if (buggy != 0 || fixed != 1) {
            printf("  FAILED!\n");
            failures++;
        }
    }

    // --- Scenario 2: Under-raise by 1 chip ---
    // highestSet = 100, minimumRaise = 100. Player has mySet = 0, myCash = 500.
    // targetBet = 101 raises by 1 chip above highestSet instead of minRaise 100.
    {
        MockPlayer p{0, 500};
        int buggy = p.checkMyAction(PLAYER_ACTION_RAISE, 101, 100, 100, 10, false);
        int fixed = p.checkMyAction(PLAYER_ACTION_RAISE, 101, 100, 100, 10, true);
        printf("Scenario 2 (Under-raise by 1 chip: targetBet=101):\n");
        printf("  Buggy: %d (expected 0, bug allows illegal under-raise)\n", buggy);
        printf("  Fixed: %d (expected 1, rejected)\n", fixed);
        if (buggy != 0 || fixed != 1) {
            printf("  FAILED!\n");
            failures++;
        }
    }

    // --- Scenario 3: Under-raise by 1 chip below min-raise (199 vs 200) ---
    {
        MockPlayer p{0, 500};
        int buggy = p.checkMyAction(PLAYER_ACTION_RAISE, 199, 100, 100, 10, false);
        int fixed = p.checkMyAction(PLAYER_ACTION_RAISE, 199, 100, 100, 10, true);
        printf("Scenario 3 (Under-raise targetBet=199 when min required is 200):\n");
        printf("  Buggy: %d (expected 0, bug allows under-raise)\n", buggy);
        printf("  Fixed: %d (expected 1, rejected)\n", fixed);
        if (buggy != 0 || fixed != 1) {
            printf("  FAILED!\n");
            failures++;
        }
    }

    // --- Scenario 4: Small Blind under-raise ---
    // SB has mySet = 50. BB is 100 (highestSet = 100, minimumRaise = 100).
    // Min-raise total set must be 200, so targetBet must be at least 150.
    // SB sends targetBet = 100 (total set becomes 150, under-raising by 50).
    {
        MockPlayer p{50, 500};
        int buggy = p.checkMyAction(PLAYER_ACTION_RAISE, 100, 100, 100, 25, false);
        int fixed = p.checkMyAction(PLAYER_ACTION_RAISE, 100, 100, 100, 25, true);
        printf("Scenario 4 (SB under-raise: mySet=50, targetBet=100):\n");
        printf("  Buggy: %d (expected 0, bug allows under-raise)\n", buggy);
        printf("  Fixed: %d (expected 1, rejected)\n", fixed);
        if (buggy != 0 || fixed != 1) {
            printf("  FAILED!\n");
            failures++;
        }
    }

    // --- Scenario 5: Re-raise on flop (call disguised as re-raise) ---
    // P1 bet 100 (mySet=100), P2 raised to 300 (highestSet=300, minimumRaise=200).
    // P1 sends targetBet = 200 (total set becomes 300, which is just a CALL).
    {
        MockPlayer p{100, 1000};
        int buggy = p.checkMyAction(PLAYER_ACTION_RAISE, 200, 300, 200, 10, false);
        int fixed = p.checkMyAction(PLAYER_ACTION_RAISE, 200, 300, 200, 10, true);
        printf("Scenario 5 (Re-raise with call amount: mySet=100, highestSet=300, minRaise=200, targetBet=200):\n");
        printf("  Buggy: %d (expected 0, bug allows call as re-raise)\n", buggy);
        printf("  Fixed: %d (expected 1, rejected)\n", fixed);
        if (buggy != 0 || fixed != 1) {
            printf("  FAILED!\n");
            failures++;
        }
    }

    // --- Scenario 6: Partial stack all-in call ---
    // highestSet = 500. Player has myCash = 100, mySet = 0.
    // Player sends targetBet = 50 (hiding 50 chips when calling all-in).
    {
        MockPlayer p{0, 100};
        int buggy = p.checkMyAction(PLAYER_ACTION_CALL, 50, 500, 100, 10, false);
        int fixed = p.checkMyAction(PLAYER_ACTION_CALL, 50, 500, 100, 10, true);
        printf("Scenario 6 (Partial stack all-in call: myCash=100, targetBet=50):\n");
        printf("  Buggy: %d (expected 0, bug allows partial stack all-in)\n", buggy);
        printf("  Fixed: %d (expected 1, rejected)\n", fixed);
        if (buggy != 0 || fixed != 1) {
            printf("  FAILED!\n");
            failures++;
        }
    }

    // --- Scenario 7: Valid actions must pass under fixed logic ---
    {
        MockPlayer p1{0, 500};
        // Valid exact min-raise (100 call + 100 raise = 200)
        assert(p1.checkMyAction(PLAYER_ACTION_RAISE, 200, 100, 100, 10, true) == 0);
        // Valid large raise
        assert(p1.checkMyAction(PLAYER_ACTION_RAISE, 350, 100, 100, 10, true) == 0);

        // Valid SB min-raise (mySet=50, targetBet=150 -> 200 total)
        MockPlayer pSb{50, 500};
        assert(pSb.checkMyAction(PLAYER_ACTION_RAISE, 150, 100, 100, 25, true) == 0);

        // Valid re-raise (mySet=100, highestSet=300, minRaise=200 -> need 400)
        MockPlayer pRe{100, 1000};
        assert(pRe.checkMyAction(PLAYER_ACTION_RAISE, 400, 300, 200, 10, true) == 0);

        // Valid call
        assert(p1.checkMyAction(PLAYER_ACTION_CALL, 100, 100, 100, 10, true) == 0);

        // Valid all-in call
        MockPlayer pShort{0, 50};
        assert(pShort.checkMyAction(PLAYER_ACTION_CALL, 50, 100, 100, 10, true) == 0);

        // Valid check
        MockPlayer pCheck{100, 500};
        assert(pCheck.checkMyAction(PLAYER_ACTION_CHECK, 0, 100, 100, 10, true) == 0);

        // Invalid check
        assert(p1.checkMyAction(PLAYER_ACTION_CHECK, 0, 100, 100, 10, true) == 1);

        // Valid fold
        assert(p1.checkMyAction(PLAYER_ACTION_FOLD, 0, 100, 100, 10, true) == 0);

        // Valid bet
        assert(p1.checkMyAction(PLAYER_ACTION_BET, 20, 0, 0, 10, true) == 0);

        // Valid all-in
        assert(p1.checkMyAction(PLAYER_ACTION_ALLIN, 500, 100, 100, 10, true) == 0);

        printf("\nScenario 7 (All valid actions pass under fixed logic): PASS\n");
    }

    if (failures > 0) {
        printf("\nTEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("\nTEST PASSED: checkMyAction bugs confirmed and fix verified across all scenarios\n");
    return 0;
}
