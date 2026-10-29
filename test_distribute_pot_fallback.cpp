// Test for LocalBoard::distributePot() fallback side-pot level distribution bug.
//
// Bug: When a player folds after contributing more chips than any remaining
// all-in player, their excess contribution forms a pot level with no active
// non-folded contributors (winnerCount == 0). A fallback was added to award
// the level to the best non-folded hand(s) among active players.
// However, finalPot was still checked with `potLevel[0] == playerSets[k]`
// which is always false for fallback winners because their chips were already
// exhausted at lower levels. Consequently, finalPot was never set to true,
// the level sum was never distributed, potCarryOver was stranded, a
// "distributePot-ERROR: Pot = ..." was logged, and emergency redistribution
// dumped the entire unallocated pot to winners.front() (the main-pot winner),
// distorting payouts and breaking split pots.
//
// Fix: When fallback is used (!winnerCount), set finalPot = true so the
// pot level sum is properly distributed to the fallback winners immediately.

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <list>
#include <algorithm>
#include <memory>

struct MockPlayer {
    unsigned uniqueId;
    int cash;
    int roundStartCash;
    int lastMoneyWon;
    int cardsValue;
    bool active;
    int action; // 1 = FOLD
};

struct PotResult {
    bool errorTriggered;
    int remainingPot;
    std::vector<int> moneyWon;
};

static PotResult runPotDistribution(std::vector<MockPlayer> seats, int initialPot, bool applyFix)
{
    int pot = initialPot;
    std::vector<unsigned> playerSets;
    for (const auto& p : seats) {
        if (p.active) {
            playerSets.push_back(static_cast<unsigned>(std::max(0, p.roundStartCash - p.cash)));
        } else {
            playerSets.push_back(0);
        }
    }

    std::vector<unsigned> playerSetsSort = playerSets;
    std::sort(playerSetsSort.begin(), playerSetsSort.end());

    std::vector<unsigned> potLevel;
    std::list<unsigned> winners;
    unsigned potCarryOver = 0;
    bool errorTriggered = false;

    for (size_t i = 0; i < playerSetsSort.size(); i++) {
        int highestCardsValue = 0;
        if (playerSetsSort[i] > 0) {
            potLevel.push_back(playerSetsSort[i]);
            potLevel.push_back(static_cast<unsigned>((playerSetsSort.size() - i) * potLevel[0] + potCarryOver));

            for (size_t j = 0; j < seats.size(); j++) {
                if (seats[j].active && seats[j].cardsValue > highestCardsValue && seats[j].action != 1 && playerSets[j] >= potLevel[0]) {
                    highestCardsValue = seats[j].cardsValue;
                }
            }
            for (size_t j = 0; j < seats.size(); j++) {
                if (seats[j].active && highestCardsValue == seats[j].cardsValue && seats[j].action != 1 && playerSets[j] >= potLevel[0]) {
                    potLevel.push_back(seats[j].uniqueId);
                }
            }

            size_t winnerCount = potLevel.size() - 2;
            bool usedFallback = false;
            if (!winnerCount) {
                usedFallback = true;
                int fallbackHighest = 0;
                for (size_t j = 0; j < seats.size(); j++) {
                    if (seats[j].active && seats[j].action != 1 && seats[j].cardsValue > fallbackHighest) {
                        fallbackHighest = seats[j].cardsValue;
                    }
                }
                for (size_t j = 0; j < seats.size(); j++) {
                    if (seats[j].active && seats[j].action != 1 && seats[j].cardsValue == fallbackHighest) {
                        potLevel.push_back(seats[j].uniqueId);
                    }
                }
                winnerCount = potLevel.size() - 2;
            }

            bool finalPot = false;
            if (applyFix && usedFallback) {
                finalPot = true;
            } else {
                for (size_t j = 2; j < potLevel.size(); j++) {
                    for (size_t k = 0; k < seats.size(); k++) {
                        if (seats[k].uniqueId == potLevel[j] && potLevel[0] == playerSets[k]) {
                            finalPot = true;
                            break;
                        }
                    }
                    if (finalPot) break;
                }
            }

            if (finalPot) {
                for (size_t j = 2; j < potLevel.size(); j++) {
                    for (auto& p : seats) {
                        if (p.uniqueId == potLevel[j]) {
                            p.cash += potLevel[1] / winnerCount;
                            p.lastMoneyWon += potLevel[1] / winnerCount;
                            winners.push_back(p.uniqueId);
                        }
                    }
                }
                potCarryOver = 0;
                pot -= static_cast<int>(potLevel[1]);
            } else {
                potCarryOver = static_cast<unsigned>(potLevel[1]);
            }

            for (size_t j = 0; j < playerSets.size(); j++) {
                if (playerSets[j] > 0) {
                    playerSets[j] -= std::min(playerSets[j], potLevel[0]);
                }
            }
            playerSetsSort = playerSets;
            std::sort(playerSetsSort.begin(), playerSetsSort.end());
            potLevel.clear();
        }
    }

    if (pot > 0) {
        errorTriggered = true;
        if (!winners.empty()) {
            unsigned firstWinnerId = winners.front();
            for (auto& p : seats) {
                if (p.uniqueId == firstWinnerId) {
                    p.cash += pot;
                    p.lastMoneyWon += pot;
                    pot = 0;
                    break;
                }
            }
        }
    }

    PotResult res;
    res.errorTriggered = errorTriggered;
    res.remainingPot = pot;
    for (const auto& p : seats) {
        res.moneyWon.push_back(p.lastMoneyWon);
    }
    return res;
}

int main()
{
    // Test Scenario:
    // P1 (id 1): all-in for 50 chips. Hand strength: 100 (ties with P2)
    // P2 (id 2): all-in for 50 chips. Hand strength: 100 (ties with P1)
    // P3 (id 3): bet 100 chips, then FOLDED.
    // Total pot = 200 chips.
    //
    // Fair expected payout:
    // Main pot (50*3 = 150): split equally -> P1: 75, P2: 75.
    // Excess from P3's fold (50*1 = 50): fallback splits equally -> P1: 25, P2: 25.
    // Total: P1: 100, P2: 100, P3: 0.
    std::vector<MockPlayer> seats = {
        { 1, 0, 50, 0, 100, true, 0 },
        { 2, 0, 50, 0, 100, true, 0 },
        { 3, 0, 100, 0, 0, true, 1 }
    };

    PotResult buggyRes = runPotDistribution(seats, 200, false);
    PotResult fixedRes = runPotDistribution(seats, 200, true);

    printf("Buggy Pot Distribution:\n");
    printf("  errorTriggered = %s\n", buggyRes.errorTriggered ? "true (BUG CONFIRMED)" : "false");
    printf("  P1 won = %d, P2 won = %d, P3 won = %d\n", buggyRes.moneyWon[0], buggyRes.moneyWon[1], buggyRes.moneyWon[2]);

    printf("\nFixed Pot Distribution:\n");
    printf("  errorTriggered = %s\n", fixedRes.errorTriggered ? "true" : "false");
    printf("  P1 won = %d, P2 won = %d, P3 won = %d\n", fixedRes.moneyWon[0], fixedRes.moneyWon[1], fixedRes.moneyWon[2]);

    bool buggyFailed = buggyRes.errorTriggered && (buggyRes.moneyWon[0] != buggyRes.moneyWon[1]);
    bool fixedPassed = !fixedRes.errorTriggered && (fixedRes.moneyWon[0] == 100) && (fixedRes.moneyWon[1] == 100) && (fixedRes.moneyWon[2] == 0);

    printf("\nBuggy behaviour confirmed (unfair split + error): %s\n", buggyFailed ? "PASS" : "FAIL");
    printf("Fixed behaviour confirmed (equal split 100/100, no error): %s\n", fixedPassed ? "PASS" : "FAIL");

    if (!buggyFailed || !fixedPassed) {
        printf("\nTEST FAILED\n");
        return 1;
    }

    printf("\nTEST PASSED: fallback pot distribution bug confirmed and fix verified\n");
    return 0;
}
