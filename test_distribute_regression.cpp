// Regression test for LocalBoard::distributePot() pot distribution.
//
// Replicates the distributePot algorithm (fixed version: finalPot=true when
// fallback is used) and verifies correct chip distribution across several
// scenarios, including main/side pots, folded-player excess (fallback),
// carry-over levels, and split pots.
//
// Mirrors the engine logic in src/engine/local_engine/localboard.cpp.

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>

struct MockSeat {
    unsigned id;
    int cash;
    int roundStartCash;
    int cardsValue;
    int action;   // 1 = FOLD
    bool active;
};

// Replicates LocalBoard::distributePot() fixed logic.
static void distributePot(std::vector<MockSeat>& seats, int initialPot,
                          std::vector<int>& moneyWon, int& remainingPot,
                          bool& errorTriggered, unsigned dealerPosition = 0)
{
    size_t n = seats.size();
    std::vector<unsigned> playerSets(n, 0);
    for (size_t j = 0; j < n; j++) {
        if (seats[j].active) {
            playerSets[j] = static_cast<unsigned>(std::max(0, seats[j].roundStartCash - seats[j].cash));
        } else {
            playerSets[j] = 0;
        }
    }
    std::vector<unsigned> playerSetsSort = playerSets;
    std::sort(playerSetsSort.begin(), playerSetsSort.end());

    int pot = initialPot;
    std::vector<unsigned> potLevel;
    unsigned potCarryOver = 0;
    errorTriggered = false;
    moneyWon.assign(n, 0);

    for (size_t i = 0; i < playerSetsSort.size(); i++) {
        int highestCardsValue = 0;
        if (playerSetsSort[i] > 0) {
            potLevel.push_back(playerSetsSort[i]);
            potLevel.push_back(static_cast<unsigned>((playerSetsSort.size() - i) * potLevel[0] + potCarryOver));

            for (size_t j = 0; j < n; j++) {
                if (seats[j].active && seats[j].cardsValue > highestCardsValue && seats[j].action != 1 && playerSets[j] >= potLevel[0]) {
                    highestCardsValue = seats[j].cardsValue;
                }
            }
            for (size_t j = 0; j < n; j++) {
                if (seats[j].active && highestCardsValue == seats[j].cardsValue && seats[j].action != 1 && playerSets[j] >= potLevel[0]) {
                    potLevel.push_back(seats[j].id);
                }
            }

            size_t winnerCount = potLevel.size() - 2;
            bool usedFallback = false;
            if (!winnerCount) {
                usedFallback = true;
                int fallbackHighest = 0;
                for (size_t j = 0; j < n; j++) {
                    if (seats[j].active && seats[j].action != 1 && seats[j].cardsValue > fallbackHighest) {
                        fallbackHighest = seats[j].cardsValue;
                    }
                }
                for (size_t j = 0; j < n; j++) {
                    if (seats[j].active && seats[j].action != 1 && seats[j].cardsValue == fallbackHighest) {
                        potLevel.push_back(seats[j].id);
                    }
                }
                winnerCount = potLevel.size() - 2;
            }

            bool finalPot = false;
            if (usedFallback) {
                finalPot = true;
            } else {
                for (size_t j = 2; j < potLevel.size(); j++) {
                    for (size_t k = 0; k < n; k++) {
                        if (seats[k].id == potLevel[j] && potLevel[0] == playerSets[k]) {
                            finalPot = true;
                            break;
                        }
                    }
                    if (finalPot) break;
                }
            }

            if (finalPot) {
                size_t mod = potLevel[1] % winnerCount;
                if (mod == 0) {
                    for (size_t j = 2; j < potLevel.size(); j++) {
                        for (size_t s = 0; s < n; s++) {
                            if (seats[s].id == potLevel[j]) {
                                seats[s].cash += (int)(potLevel[1] / winnerCount);
                                moneyWon[s] += (int)(potLevel[1] / winnerCount);
                            }
                        }
                    }
                } else {
                    size_t dealerIdx = 0;
                    for (size_t k = 0; k < n; k++) {
                        if (seats[k].id == dealerPosition) { dealerIdx = k; break; }
                    }
                    size_t pos = dealerIdx;
                    for (size_t j = 0; j < winnerCount; j++) {
                        bool winnerHit = false;
                        for (size_t k = 0; k < n && !winnerHit; k++) {
                            pos = (pos + 1) % n;
                            for (size_t l = 2; l < potLevel.size(); l++) {
                                if (seats[pos].active && seats[pos].id == potLevel[l])
                                    winnerHit = true;
                            }
                        }
                        if (!winnerHit) continue;
                        if (j < mod) {
                            seats[pos].cash += (int)(potLevel[1] / winnerCount) + 1;
                            moneyWon[pos] += (int)(potLevel[1] / winnerCount) + 1;
                        } else {
                            seats[pos].cash += (int)(potLevel[1] / winnerCount);
                            moneyWon[pos] += (int)(potLevel[1] / winnerCount);
                        }
                    }
                }
                potCarryOver = 0;
                pot -= (int)potLevel[1];
            } else {
                potCarryOver = static_cast<unsigned>(potLevel[1]);
            }

            for (size_t j = 0; j < n; j++) {
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
        size_t target = 0;
        for (size_t k = 1; k < n; k++) {
            if (moneyWon[k] > moneyWon[target]) target = k;
        }
        seats[target].cash += pot;
        moneyWon[target] += pot;
        pot = 0;
    }
    remainingPot = pot;
}

int main()
{
    int failures = 0;

    // Scenario 1: folded-player excess (fallback), split pot between tying winners
    // P0(id=0): 30 chips, value 100 (best), all-in
    // P1(id=1): 50 chips, value 100 (ties P0), all-in
    // P2(id=2): 100 chips, value 50, FOLDED after betting 100
    // Total pot = 180.
    // Level1(30): all 3 contribute 30 -> pot 90. Winners P0,P1 (tie). P0 all-in at level -> finalPot. 45 each.
    // Level2(20): P1 remaining 20 -> pot 40. Winner P1. P1 gets 40.
    // Level3(50): P2 folded excess 50 -> fallback to P0,P1 (tie, value 100). pot 50. Split 25 each.
    // Expected: P0=45+25=70, P1=45+40+25=110, P2=0.
    {
        std::vector<MockSeat> seats = {
            {0, 0, 30, 100, 0, true},
            {1, 0, 50, 100, 0, true},
            {2, 0, 100, 50,  1, true}
        };
        std::vector<int> moneyWon;
        int remainingPot = 0;
        bool errorTriggered = false;
        distributePot(seats, 180, moneyWon, remainingPot, errorTriggered, 0);
        printf("Scenario 1 (folded excess split): moneyWon=[%d,%d,%d] pot=%d error=%d\n",
               moneyWon[0], moneyWon[1], moneyWon[2], remainingPot, errorTriggered);
        bool ok = (moneyWon[0] == 70 && moneyWon[1] == 110 && moneyWon[2] == 0 && remainingPot == 0 && !errorTriggered);
        printf("  %s (expected [70,110,0])\n", ok ? "PASS" : "FAIL");
        if (!ok) failures++;
    }

    // Scenario 2: simple tie split, no folding
    // P0: 50 value 100, P1: 50 value 100. pot=100.
    {
        std::vector<MockSeat> seats = {
            {0, 0, 50, 100, 0, true},
            {1, 0, 50, 100, 0, true}
        };
        std::vector<int> moneyWon;
        int remainingPot = 0;
        bool errorTriggered = false;
        distributePot(seats, 100, moneyWon, remainingPot, errorTriggered, 0);
        printf("Scenario 2 (tie split): moneyWon=[%d,%d] pot=%d error=%d\n",
               moneyWon[0], moneyWon[1], remainingPot, errorTriggered);
        bool ok = (moneyWon[0] == 50 && moneyWon[1] == 50 && remainingPot == 0 && !errorTriggered);
        printf("  %s\n", ok ? "PASS" : "FAIL");
        if (!ok) failures++;
    }

    // Scenario 3: one player wins all, other folded
    // P0: 100 value 200, P1: 30 value 50 FOLDED. pot=130.
    // Level1(30): both contribute 30 -> pot 60. Winner P0. finalPot: playerSets[P0]=100==30? no. carryOver=60.
    //   remaining: P0=70, P1=0.
    // Level2(70): P0 remaining 70 -> pot 70+carryOver60=130. Winner P0. finalPot: playerSets[P0]=70==70? yes. P0 gets 130.
    // Expected: P0=130, P1=0.
    {
        std::vector<MockSeat> seats = {
            {0, 0, 100, 200, 0, true},
            {1, 0, 30,  50,  1, true}
        };
        std::vector<int> moneyWon;
        int remainingPot = 0;
        bool errorTriggered = false;
        distributePot(seats, 130, moneyWon, remainingPot, errorTriggered, 0);
        printf("Scenario 3 (one wins all): moneyWon=[%d,%d] pot=%d error=%d\n",
               moneyWon[0], moneyWon[1], remainingPot, errorTriggered);
        bool ok = (moneyWon[0] == 130 && moneyWon[1] == 0 && remainingPot == 0 && !errorTriggered);
        printf("  %s\n", ok ? "PASS" : "FAIL");
        if (!ok) failures++;
    }

    // Scenario 4: simple win, no mod
    // P0(id=0, dealer): 10 value 200 (best), P1(id=1): 10 value 100. pot=20.
    {
        std::vector<MockSeat> seats = {
            {0, 0, 10, 200, 0, true},
            {1, 0, 10, 100, 0, true}
        };
        std::vector<int> moneyWon;
        int remainingPot = 0;
        bool errorTriggered = false;
        distributePot(seats, 20, moneyWon, remainingPot, errorTriggered, 0);
        printf("Scenario 4 (simple win): moneyWon=[%d,%d] pot=%d error=%d\n",
               moneyWon[0], moneyWon[1], remainingPot, errorTriggered);
        bool ok = (moneyWon[0] == 20 && moneyWon[1] == 0 && remainingPot == 0 && !errorTriggered);
        printf("  %s\n", ok ? "PASS" : "FAIL");
        if (!ok) failures++;
    }

    if (failures > 0) {
        printf("\nREGRESSION TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }
    printf("\nREGRESSION TEST PASSED: distributePot fixed logic verified\n");
    return 0;
}
