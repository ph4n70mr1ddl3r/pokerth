// Test for calcMyOdds() turn-case river card enumeration bias.
//
// Bug: In LocalPlayer::calcMyOdds() case 2 (turn), the river card was
// enumerated with `for(k=j+1; k<52; k++)`, forcing the river card index
// to always be higher than both opponent hole cards. For each set of 3
// unknown cards only 1 of 3 possible river-card assignments was tested,
// biasing the odds calculation.
//
// Fix: Enumerate the river card independently with `for(k=0; k<52; k++)`
// excluding k==i and k==j, so all 3 river-card assignments per card set
// are tested equally.
//
// This test replicates both enumeration strategies with a simplified
// hand evaluator and verifies they produce different odds (proving the
// bias existed) and that the fixed strategy enumerates each card set 3x.

#include <cstdio>
#include <cstdlib>
#include <array>

// Simplified card representation: 0..51 = suit*13 + rank
// Hand strength proxy: sum of card indices (higher = better). Using the
// full card index (not just rank) makes suit matter, so the river-card
// assignment (shared vs opponent-only) changes hand strengths differently.

static int cardStrength(int card) { return card; }

// Simplified 7-card evaluator: best 5-card strength sum.
// Not a real poker evaluator, just needs to order hands consistently and
// be sensitive to which specific cards are shared vs opponent-only.
static int evalHand(const int* cards, int n) {
    std::array<int, 7> sorted;
    for (int i = 0; i < n; i++) sorted[i] = cards[i];
    // simple insertion sort descending by strength
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

// Known cards for a turn scenario: 2 hole + 4 board = 6 known cards.
// Leaves 46 unknown cards: opponent gets 2, river gets 1.
static const int myHole[2] = {0, 13};          // two aces (rank 0 in each suit)
static const int board[4] = {1, 14, 27, 40};    // four kings-ish cards
static const int knownCount = 6;

// Returns true if card is one of the known cards.
static bool isKnown(int c) {
    for (int i = 0; i < 2; i++) if (c == myHole[i]) return true;
    for (int i = 0; i < 4; i++) if (c == board[i]) return true;
    return false;
}

// Buggy enumeration: river card k always > opponent cards i, j (k from j+1).
static void buggyEnumeration(int& countAll, int& countMy) {
    countAll = 0;
    countMy = 0;
    int myCards[7] = {myHole[0], myHole[1], board[0], board[1], board[2], board[3], 0};
    int oppCards[7] = {board[0], board[1], board[2], board[3], 0, 0, 0};

    for (int i = 0; i < 52; i++) {
        if (isKnown(i)) continue;
        for (int j = i + 1; j < 52; j++) {
            if (isKnown(j)) continue;
            for (int k = j + 1; k < 52; k++) {
                if (isKnown(k)) continue;

                countAll++;
                oppCards[0] = i;
                oppCards[1] = j;
                oppCards[6] = k;
                myCards[6] = k;

                if (evalHand(myCards, 7) >= evalHand(oppCards, 7)) countMy++;
            }
        }
    }
}

// Fixed enumeration: river card k independent (k from 0, excluding i, j).
static void fixedEnumeration(int& countAll, int& countMy) {
    countAll = 0;
    countMy = 0;
    int myCards[7] = {myHole[0], myHole[1], board[0], board[1], board[2], board[3], 0};
    int oppCards[7] = {board[0], board[1], board[2], board[3], 0, 0, 0};

    for (int i = 0; i < 52; i++) {
        if (isKnown(i)) continue;
        for (int j = i + 1; j < 52; j++) {
            if (isKnown(j)) continue;
            for (int k = 0; k < 52; k++) {
                if (isKnown(k)) continue;
                if (k == i || k == j) continue;

                countAll++;
                oppCards[0] = i;
                oppCards[1] = j;
                oppCards[6] = k;
                myCards[6] = k;

                if (evalHand(myCards, 7) >= evalHand(oppCards, 7)) countMy++;
            }
        }
    }
}

int main() {
    int bAll = 0, bMy = 0;
    int fAll = 0, fMy = 0;

    buggyEnumeration(bAll, bMy);
    fixedEnumeration(fAll, fMy);

    double bOdds = (bAll > 0) ? 100.0 * bMy / bAll : 0.0;
    double fOdds = (fAll > 0) ? 100.0 * fMy / fAll : 0.0;

    printf("Buggy enumeration:\n");
    printf("  countAll = %d (expected C(46,3) = 15180)\n", bAll);
    printf("  countMy  = %d\n", bMy);
    printf("  odds     = %.4f%%\n", bOdds);
    printf("\nFixed enumeration:\n");
    printf("  countAll = %d (expected 46*C(45,2) = 45540)\n", fAll);
    printf("  countMy  = %d\n", fMy);
    printf("  odds     = %.4f%%\n", fOdds);
    printf("\n");

    bool countAllCorrect = (bAll == 15180) && (fAll == 45540);
    bool oddsDiffer = (bOdds != fOdds);
    bool fixedIsTriple = (fAll == 3 * bAll);

    printf("Buggy countAll == C(46,3): %s\n", (bAll == 15180) ? "PASS" : "FAIL");
    printf("Fixed countAll == 46*C(45,2): %s\n", (fAll == 45540) ? "PASS" : "FAIL");
    printf("Fixed countAll == 3 * buggy countAll: %s\n", fixedIsTriple ? "PASS" : "FAIL");
    printf("Odds differ between buggy and fixed: %s\n", oddsDiffer ? "PASS (bias confirmed)" : "FAIL");

    if (!countAllCorrect || !oddsDiffer || !fixedIsTriple) {
        printf("\nTEST FAILED\n");
        return 1;
    }

    printf("\nTEST PASSED: enumeration bias confirmed and fix verified\n");
    return 0;
}
