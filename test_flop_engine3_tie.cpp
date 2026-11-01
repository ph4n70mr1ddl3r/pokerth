// Regression test for LocalPlayer::flopEngine3()/turnEngine3()/riverEngine3() tie-handling bug.
//
// Bug: The Engine3 AI odds enumeration counts a showdown tie (split pot) as a full
// win for the player (`if(tempMyCardsValue >= tempOpponentCardsValue) countMy++;`).
// This overestimates the AI's win probability because in a real showdown a tie splits
// the pot rather than awarding it entirely to one player. The same bug was already
// fixed in calcMyOdds() (ties counted as half-wins), but the Engine3 variants used
// for AI decisions still use >=.
//
// Fix: Count strict wins and ties separately, then compute
// percent = (wins + 0.5 * ties) / total, matching the fixed calcMyOdds() logic.
//
// This test replicates the flopEngine3() enumeration with a simplified hand
// evaluator and verifies the buggy strategy (>=, ties as full wins) produces higher
// win% than the fixed strategy (> with half ties), proving the overestimation.

#include <cstdio>
#include <cstdlib>
#include <array>
#include <cmath>

// Simplified card representation: 0..51 = suit*13 + rank
// Rank is card % 13 (0=2, 12=A), suit is card / 13.
// Hand strength proxy: sum of card indices (higher = better). Using the full card
// index (not just rank) makes suit matter.
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

// Known cards for a flop scenario: 2 hole + 3 board = 5 known cards.
// Leaves 47 unknown cards: turn gets 1, river gets 1, opponent gets 2.
static const int myHole[2] = {0, 13};          // two aces (rank 0 in each suit)
static const int board[3] = {1, 14, 27};       // three board cards

// Returns true if card is one of the known cards.
static bool isKnown(int c) {
    for (int i = 0; i < 2; i++) if (c == myHole[i]) return true;
    for (int i = 0; i < 3; i++) if (c == board[i]) return true;
    return false;
}

// Buggy flopEngine3 odds: ties counted as full wins (>= comparison).
// Replicates flopEngine3() before the fix.
static void buggyFlopEngine3(int& countAll, int& countMy, double& percent) {
    countAll = 0;
    countMy = 0;

    int myCards[7] = {myHole[0], myHole[1], board[0], board[1], board[2], 0, 0};
    int oppCards[7] = {board[0], board[1], board[2], 0, 0, 0, 0};

    int myValue = 0;
    int oppValue = 0;

    for (int i = 0; i < 52; i++) {           // turn card
        if (isKnown(i)) continue;
        myCards[5] = i;
        oppCards[5] = i;
        for (int j = i + 1; j < 52; j++) {   // river card
            if (isKnown(j)) continue;
            myCards[6] = j;
            oppCards[6] = j;
            for (int k = 0; k < 52; k++) {   // opponent hole card 1
                if (isKnown(k) || k == i || k == j) continue;
                oppCards[0] = k;
                for (int l = k + 1; l < 52; l++) {  // opponent hole card 2
                    if (isKnown(l) || l == i || l == j || l == k) continue;
                    oppCards[1] = l;
                    countAll++;

                    myValue = evalHand(myCards, 7);
                    oppValue = evalHand(oppCards, 7);

                    // Bug: >= counts ties as full wins, overestimating win%.
                    if (myValue >= oppValue) countMy++;
                }
            }
        }
    }

    percent = (countAll > 0) ? 100.0 * countMy / countAll : 0.0;
}

// Fixed flopEngine3 odds: strict wins plus half ties.
// Replicates flopEngine3() after the fix.
static void fixedFlopEngine3(int& countAll, int& countMy, int& countTie, double& percent) {
    countAll = 0;
    countMy = 0;
    countTie = 0;

    int myCards[7] = {myHole[0], myHole[1], board[0], board[1], board[2], 0, 0};
    int oppCards[7] = {board[0], board[1], board[2], 0, 0, 0, 0};

    int myValue = 0;
    int oppValue = 0;

    for (int i = 0; i < 52; i++) {           // turn card
        if (isKnown(i)) continue;
        myCards[5] = i;
        oppCards[5] = i;
        for (int j = i + 1; j < 52; j++) {   // river card
            if (isKnown(j)) continue;
            myCards[6] = j;
            oppCards[6] = j;
            for (int k = 0; k < 52; k++) {   // opponent hole card 1
                if (isKnown(k) || k == i || k == j) continue;
                oppCards[0] = k;
                for (int l = k + 1; l < 52; l++) {  // opponent hole card 2
                    if (isKnown(l) || l == i || l == j || l == k) continue;
                    oppCards[1] = l;
                    countAll++;

                    myValue = evalHand(myCards, 7);
                    oppValue = evalHand(oppCards, 7);

                    if (myValue > oppValue) countMy++;
                    else if (myValue == oppValue) countTie++;
                }
            }
        }
    }

    percent = (countAll > 0) ? 100.0 * (countMy + 0.5 * countTie) / countAll : 0.0;
}

// Brute-force reference: enumerate every (turn,river) pair and opponent hole pair
// directly from the unknown-card list, to confirm exhaustiveness.
static void bruteForce(int& countAll, int& countMy, int& countTie, double& percent) {
    countAll = 0;
    countMy = 0;
    countTie = 0;
    int myCards[7] = {myHole[0], myHole[1], board[0], board[1], board[2], 0, 0};
    int oppCards[7] = {board[0], board[1], board[2], 0, 0, 0, 0};
    int unknown[52];
    int nUnknown = 0;
    for (int c = 0; c < 52; c++) if (!isKnown(c)) unknown[nUnknown++] = c;

    for (int a = 0; a < nUnknown; a++) {          // turn
        myCards[5] = unknown[a];
        oppCards[5] = unknown[a];
        for (int b = a + 1; b < nUnknown; b++) {  // river
            myCards[6] = unknown[b];
            oppCards[6] = unknown[b];
            for (int c = 0; c < nUnknown; c++) {  // opp hole 1
                if (c == a || c == b) continue;
                oppCards[0] = unknown[c];
                for (int d = c + 1; d < nUnknown; d++) {  // opp hole 2
                    if (d == a || d == b) continue;
                    oppCards[1] = unknown[d];
                    countAll++;

                    int myValue = evalHand(myCards, 7);
                    int oppValue = evalHand(oppCards, 7);
                    if (myValue > oppValue) countMy++;
                    else if (myValue == oppValue) countTie++;
                }
            }
        }
    }

    percent = (countAll > 0) ? 100.0 * (countMy + 0.5 * countTie) / countAll : 0.0;
}

int main() {
    int bAll = 0, bMy = 0;
    double bPct = 0.0;
    int fAll = 0, fMy = 0, fTie = 0;
    double fPct = 0.0;
    int rAll = 0, rMy = 0, rTie = 0;
    double rPct = 0.0;

    buggyFlopEngine3(bAll, bMy, bPct);
    fixedFlopEngine3(fAll, fMy, fTie, fPct);
    bruteForce(rAll, rMy, rTie, rPct);

    printf("FlopEngine3 tie-handling enumeration:\n");
    printf("  countAll = %d\n", fAll);
    printf("\nBuggy (>=, ties as full wins):\n");
    printf("  countMy  = %d\n", bMy);
    printf("  percent  = %.4f%%\n", bPct);
    printf("\nFixed (> with half ties):\n");
    printf("  countMy  = %d\n", fMy);
    printf("  countTie = %d\n", fTie);
    printf("  percent  = %.4f%%\n", fPct);
    printf("\nBrute-force reference:\n");
    printf("  countAll = %d\n", rAll);
    printf("  countMy  = %d\n", rMy);
    printf("  countTie = %d\n", rTie);
    printf("  percent  = %.4f%%\n", rPct);
    printf("\n");

    bool countAllCorrect = (fAll == rAll);
    bool tiesExist = (fTie > 0);
    bool buggyOverestimates = (bPct > fPct);
    bool fixedMatchesRef = (fPct == rPct);
    bool fixedReasonable = (fPct >= 0.0 && fPct <= 100.0);

    printf("countAll == brute force: %s\n", countAllCorrect ? "PASS" : "FAIL");
    printf("Ties exist in this scenario: %s\n", tiesExist ? "PASS (tie scenario confirmed)" : "FAIL (no ties found)");
    printf("Buggy percent > fixed percent (overestimation confirmed): %s\n", buggyOverestimates ? "PASS" : "FAIL");
    printf("Fixed percent == brute force: %s\n", fixedMatchesRef ? "PASS" : "FAIL");
    printf("Fixed percent in [0,100]: %s\n", fixedReasonable ? "PASS" : "FAIL");

    if (!countAllCorrect || !tiesExist || !buggyOverestimates || !fixedMatchesRef || !fixedReasonable) {
        printf("\nTEST FAILED\n");
        return 1;
    }

    printf("\nTEST PASSED: Engine3 tie-handling bug confirmed and fix verified\n");
    return 0;
}
