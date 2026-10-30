// Test for LocalBoard::determinePlayerNeedToShowCards() show-list completeness bug.
//
// Bug: In LocalBoard::determinePlayerNeedToShowCards(), the level-iteration loop
// compares each active non-folded player against the existing level groups. Two
// branches fall through without adding the player or breaking:
//   - player value > level value, but not the last level  (continue searching)
//   - player value < level value, but player bet <= level bet (continue searching)
// When a player's hand value falls strictly between two existing level groups AND
// their bet amount does not exceed the higher group's bet, both branches fall
// through for every level, the inner loop ends without a break, and the player is
// silently omitted from playerNeedToShowCards. An active, non-folded player at
// showdown can thus have their cards never revealed.
//
// Fix: Track whether the player was added during the level iteration; if not,
// add them to the show list after the loop so every active non-folded player is
// accounted for at showdown.
//
// This test replicates both strategies with mock players and verifies that a
// player with an intermediate hand value is omitted by the buggy logic but
// included by the fixed logic, while all previously-included players remain.

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <list>
#include <vector>

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
    int uniqueId;
    int cardsValue;
    int roundStartCash;
    int cash;
    PlayerAction action;

    MockPlayer(int id, int value, int rsc, int c, PlayerAction a)
        : uniqueId(id), cardsValue(value), roundStartCash(rsc), cash(c), action(a) {}

    int getMyUniqueID() const { return uniqueId; }
    int getMyCardsValueInt() const { return cardsValue; }
    int getMyRoundStartCash() const { return roundStartCash; }
    int getMyCash() const { return cash; }
    PlayerAction getMyAction() const { return action; }
    int getMySet() const { return std::max(0, roundStartCash - cash); }
};

// Replicates LocalBoard::determinePlayerNeedToShowCards() non-all-in path.
// lastActionPlayer is the first element of players.
static void determinePlayerNeedToShowCards(const std::list<MockPlayer>& players,
                                           std::list<unsigned>& showList, bool applyFix) {
    showList.clear();
    if (players.empty()) return;

    auto it_c = players.begin();
    const MockPlayer& lastAction = *it_c;

    // level list: (cardsValue, setAmount)
    struct Level { int first; int second; };
    std::list<Level> level;

    level.push_back({lastAction.cardsValue, std::max(0, lastAction.roundStartCash - lastAction.cash)});
    showList.push_back(static_cast<unsigned>(lastAction.uniqueId));

    auto level_it = level.begin();
    auto next_level_it = level.begin();

    ++it_c;
    for (unsigned i = 0; i < players.size(); i++) {
        if (it_c == players.end()) it_c = players.begin();
        const MockPlayer& p = *it_c;

        if (p.getMyAction() != PLAYER_ACTION_FOLD) {
            bool playerAdded = false;
            for (level_it = level.begin(); level_it != level.end(); ++level_it) {
                if (p.getMyCardsValueInt() > (*level_it).first) {
                    next_level_it = level_it;
                    ++next_level_it;
                    if (next_level_it == level.end()) {
                        showList.push_back(static_cast<unsigned>(p.getMyUniqueID()));
                        playerAdded = true;
                        Level tmp;
                        tmp.first = p.getMyCardsValueInt();
                        tmp.second = std::max(0, p.getMyRoundStartCash() - p.getMyCash());
                        level.push_back(tmp);
                        break;
                    }
                } else {
                    if (p.getMyCardsValueInt() == (*level_it).first) {
                        next_level_it = level_it;
                        ++next_level_it;
                        int pset = std::max(0, p.getMyRoundStartCash() - p.getMyCash());
                        if (next_level_it == level.end() || pset > (*next_level_it).second) {
                            showList.push_back(static_cast<unsigned>(p.getMyUniqueID()));
                            playerAdded = true;
                            if (pset > (*level_it).second) {
                                (*level_it).second = pset;
                            }
                        }
                        break;
                    } else {
                        int pset = std::max(0, p.getMyRoundStartCash() - p.getMyCash());
                        if (pset > (*level_it).second) {
                            showList.push_back(static_cast<unsigned>(p.getMyUniqueID()));
                            playerAdded = true;
                            Level tmp;
                            tmp.first = p.getMyCardsValueInt();
                            tmp.second = pset;
                            level.insert(level_it, tmp);
                            break;
                        }
                    }
                }
            }
            if (applyFix && !playerAdded) {
                showList.push_back(static_cast<unsigned>(p.getMyUniqueID()));
            }
        }
        ++it_c;
    }

    showList.sort();
    showList.unique();
}

int main() {
    int failures = 0;

    // --- Scenario: intermediate-value player omitted by buggy logic ---
    // P0 (last action): value 50, set 50
    // P1: value 200, set 200
    // P2: value 150 (between P0 and P1), set 100 (<= P1's level bet 200) -> omitted
    // P3: value 20 (below all), set 5 -> omitted too
    {
        std::list<MockPlayer> players;
        players.push_back(MockPlayer(10, 50, 100, 50, PLAYER_ACTION_CALL));   // last action
        players.push_back(MockPlayer(20, 200, 200, 0,  PLAYER_ACTION_CALL));
        players.push_back(MockPlayer(30, 150, 150, 50, PLAYER_ACTION_CALL));
        players.push_back(MockPlayer(40, 20,  50,  45, PLAYER_ACTION_CALL));

        std::list<unsigned> buggy, fixed;
        determinePlayerNeedToShowCards(players, buggy, false);
        determinePlayerNeedToShowCards(players, fixed, true);

        printf("Scenario (intermediate-value player omitted):\n");
        printf("  Buggy show list: ");
        for (unsigned id : buggy) printf("%u ", id);
        printf("\n");
        printf("  Fixed show list: ");
        for (unsigned id : fixed) printf("%u ", id);
        printf("\n");

        bool buggyOmitsP2 = (std::find(buggy.begin(), buggy.end(),30u) == buggy.end());
        bool buggyOmitsP3 = (std::find(buggy.begin(), buggy.end(),40u) == buggy.end());
        bool fixedIncludesAll = (fixed.size() == players.size());
        bool fixedHasP2 = (std::find(fixed.begin(), fixed.end(),30u) != fixed.end());
        bool fixedHasP3 = (std::find(fixed.begin(), fixed.end(),40u) != fixed.end());
        bool keepsP0P1 = (std::find(buggy.begin(), buggy.end(),10u) != buggy.end() && std::find(buggy.begin(), buggy.end(),20u) != buggy.end());

        printf("  Buggy omits P2(150): %s\n", buggyOmitsP2 ? "PASS (bug confirmed)" : "FAIL");
        printf("  Buggy omits P3(20):  %s\n", buggyOmitsP3 ? "PASS (bug confirmed)" : "FAIL");
        printf("  Fixed includes all active players: %s\n", fixedIncludesAll ? "PASS" : "FAIL");
        printf("  Fixed keeps P0/P1: %s\n", keepsP0P1 ? "PASS" : "FAIL");

        if (!buggyOmitsP2 || !buggyOmitsP3 || !fixedIncludesAll || !fixedHasP2 || !fixedHasP3 || !keepsP0P1) {
            printf("  FAILED!\n");
            failures++;
        }
    }

    // --- Scenario: folded players must never be added ---
    {
        std::list<MockPlayer> players;
        players.push_back(MockPlayer(10, 100, 100, 0, PLAYER_ACTION_CALL)); // last action
        players.push_back(MockPlayer(20, 90,  100, 0, PLAYER_ACTION_FOLD));  // folded -> skip
        players.push_back(MockPlayer(30, 80,  100, 0, PLAYER_ACTION_CALL));

        std::list<unsigned> buggy, fixed;
        determinePlayerNeedToShowCards(players, buggy, false);
        determinePlayerNeedToShowCards(players, fixed, true);

        printf("\nScenario (folded player skipped):\n");
        printf("  Buggy: ");
        for (unsigned id : buggy) printf("%u ", id);
        printf("\n  Fixed: ");
        for (unsigned id : fixed) printf("%u ", id);
        printf("\n");

        bool buggySkipsFolded = (std::find(buggy.begin(), buggy.end(),20u) == buggy.end());
        bool fixedSkipsFolded = (std::find(fixed.begin(), fixed.end(),20u) == fixed.end());
        bool fixedIncludesActive = (fixed.size() == 2);

        printf("  Buggy skips folded: %s\n", buggySkipsFolded ? "PASS" : "FAIL");
        printf("  Fixed skips folded: %s\n", fixedSkipsFolded ? "PASS" : "FAIL");
        printf("  Fixed includes both active: %s\n", fixedIncludesActive ? "PASS" : "FAIL");

        if (!buggySkipsFolded || !fixedSkipsFolded || !fixedIncludesActive) {
            printf("  FAILED!\n");
            failures++;
        }
    }

    // --- Scenario: all-in condition path shows everyone (unchanged by fix) ---
    // This is the separate allInCondition branch; our fix only touches the
    // non-all-in iteration, so behavior there is preserved. Here we just check
    // the non-all-in iteration doesn't break with equal values.
    {
        std::list<MockPlayer> players;
        players.push_back(MockPlayer(10, 100, 100, 0, PLAYER_ACTION_CALL));
        players.push_back(MockPlayer(20, 100, 100, 0, PLAYER_ACTION_CALL)); // equal value
        players.push_back(MockPlayer(30, 100, 100, 0, PLAYER_ACTION_CALL)); // equal value

        std::list<unsigned> buggy, fixed;
        determinePlayerNeedToShowCards(players, buggy, false);
        determinePlayerNeedToShowCards(players, fixed, true);

        printf("\nScenario (equal values):\n");
        printf("  Buggy count: %zu  Fixed count: %zu\n", buggy.size(), fixed.size());
        bool fixedIncludesAll = (fixed.size() == players.size());
        printf("  Fixed includes all active: %s\n", fixedIncludesAll ? "PASS" : "FAIL");
        if (!fixedIncludesAll) { failures++; printf("  FAILED!\n"); }
    }

    if (failures > 0) {
        printf("\nTEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("\nTEST PASSED: determinePlayerNeedToShowCards bug confirmed and fix verified\n");
    return 0;
}
