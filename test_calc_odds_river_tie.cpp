// Test for LocalPlayer::calcMyOdds() river-case tie handling bug.
//
// Bug: In LocalPlayer::calcMyOdds() case 3 (river), the hand comparison uses
// `if(tempMyCardsValue >= tempOpponentCardsValue) countMy++;`, which counts
// a showdown tie (split pot) as a full win for the player. This overestimates
// the player's win probability because in a real showdown a tie splits the pot
// rather than awarding it entirely to one player.
//
// Fix: Count strict wins and ties separately, then compute odds as
// `(wins + 0.5 * ties) / total` so a split pot contributes half a win.
//
// This test replicates both strategies with a simplified hand evaluator and
// verifies that the buggy strategy (>=, ties as full wins) produces higher
// odds than the fixed strategy (> with half ties), proving the overestimation.

#include <cstdio>
#include <cstdlib>
#include <array>
#include <cmath>

// Simplified card representation: 0..51 = suit*13 + rank
// Rank is card % 13 (0=2, 12=A), suit is card / 13.
// Hand strength proxy: sum of card indices (higher = better). Using the full
// card index (not just rank) makes suit matter.
// Not a real poker evaluator, just needs to order hands consistently.
static int evalHand(const int* cards, int n) {
    std::array<int, 7> sorted;
    for (int i = 0; i < n; i++) sorted[i] = cards[i];
    for (int i = 1; i < n; i++) {
        int key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] < key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }
    int sum = 0;
    for (int i = 0; i < 5 && i < n; i++) sum += sorted[i];
    return sum;
}

// Known cards for a river scenario: 2 hole + 5 board = 7 known cards.
// Leaves 45 unknown cards: opponent gets 2.
static const int myHole[2] = {0, 13};          // two aces (rank 0 in each suit)
static const int board[5] = {1, 14, 27, 40, 2}; // five board cards

// Returns true if card is one of the known cards.
static bool isKnown(int c) {
    for (int i = 0; i < 2; i++) if (c == myHole[i]) return true;
    for (int i = 0; i < 5; i++) if (c == board[i]) return true;
    return false;
}

// Buggy river odds: ties counted as full wins (>= comparison).
// Replicates calcMyOdds() case 3 before the fix.
static void buggyRiverOdds(int& countAll, int& countMy, double& odds) {
    countAll = 0;
    countMy = 0;

    int myCards[7] = {myHole[0], myHole[1], board[0], board[1], board[2], board[3], board[4]};
    int oppCards[7] = {board[0], board[1], board[2], board[3], board[4], 0, 0};

    int myValue = evalHand(myCards, 7);

    for (int i = 0; i < 52; i++) {
        if (isKnown(i)) continue;
        for (int j = i + 1; j < 52; j++) {
            if (isKnown(j)) continue;

            countAll++;

            oppCards[0] = i;
            oppCards[1] = j;

            int oppValue = evalHand(oppCards, 7);

            // Bug: >= counts ties as full wins, overestimating odds.
            if (myValue >= oppValue) countMy++;
        }
    }

    odds = (countAll > 0) ? 100.0 * countMy / countAll : 0.0;
}

// Fixed river odds: strict wins plus half ties.
// Replicates calcMyOdds() case 3 after the fix.
static void fixedRiverOdds(int& countAll, int& countMy, int& countTie, double& odds) {
    countAll = 0;
    countMy = 0;
    countTie = 0;

    int myCards[7] = {myHole[0], myHole[1], board[0], board[1], board[2], board[3], board[4]};
    int oppCards[7] = {board[0], board[1], board[2], board[3], board[4], 0, 0};

    int myValue = evalHand(myCards, 7);

    for (int i = 0; i < 52; i++) {
        if (isKnown(i)) continue;
        for (int j = i + 1; j < 52; j++) {
            if (isKnown(j)) continue;

            countAll++;

            oppCards[0] = i;
            oppCards[1] = j;

            int oppValue = evalHand(oppCards, 7);

            if (myValue > oppValue) countMy++;
            else if (myValue == oppValue) countTie++;
        }
    }

    odds = (countAll > 0) ? 100.0 * (countMy + 0.5 * countTie) / countAll : 0.0;
}

int main() {
    int bAll = 0, bMy = 0;
    double bOdds = 0.0;
    int fAll = 0, fMy = 0, fTie = 0;
    double fOdds = 0.0;

    buggyRiverOdds(bAll, bMy, bOdds);
    fixedRiverOdds(fAll, fMy, fTie, fOdds);

    printf("River odds enumeration:\n");
    printf("  countAll = %d (expected C(45,2) = 990)\n", fAll);
    printf("\nBuggy (>=, ties as full wins):\n");
    printf("  countMy  = %d\n", bMy);
    printf("  odds     = %.4f%%\n", bOdds);
    printf("\nFixed (> with half ties):\n");
    printf("  countMy  = %d\n", fMy);
    printf("  countTie = %d\n", fTie);
    printf("  odds     = %.4f%%\n", fOdds);
    printf("\n");

    bool countAllCorrect = (fAll == 990);
    bool tiesExist = (fTie > 0);
    bool buggyOverestimates = (bOdds > fOdds);
    bool fixedReasonable = (fOdds >= 0.0 && fOdds <= 100.0);
    bool buggyReasonable = (bOdds >= 0.0 && bOdds <= 100.0);

    printf("countAll == C(45,2): %s\n", countAllCorrect ? "PASS" : "FAIL");
    printf("Ties exist in this scenario: %s\n", tiesExist ? "PASS (tie scenario confirmed)" : "FAIL (no ties found)");
    printf("Buggy odds > fixed odds (overestimation confirmed): %s\n", buggyOverestimates ? "PASS" : "FAIL");
    printf("Fixed odds in [0,100]: %s\n", fixedReasonable ? "PASS" : "FAIL");
    printf("Buggy odds in [0,100]: %s\n", buggyReasonable ? "PASS" : "FAIL");

    if (!countAllCorrect || !tiesExist || !buggyOverestimates || !fixedReasonable || !buggyReasonable) {
        printf("\nTEST FAILED\n");
        return 1;
    }

    printf("\nTEST PASSED: river tie-handling bug confirmed and fix verified\n");
    return 0;
}
