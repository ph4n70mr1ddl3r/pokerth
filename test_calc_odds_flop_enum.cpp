// Test for LocalPlayer::calcMyOdds() flop-case turn/river enumeration double-counting.
//
// Bug: In LocalPlayer::calcMyOdds() case 1 (flop), the turn card `i` and river
// card `j` are enumerated as ORDERED pairs:
//
//     for(i=0; i<52; i++) {          // turn
//         ...
//         for(j=0; j<52; j++) {      // river (starts at 0, not i+1)
//             if(... && j != i) {
//
// Because turn and river are shared community cards, the board {turn=A,river=B}
// is identical to {turn=B,river=A} (cardsValue() sorts the 7 cards internally,
// so board order does not affect the result). Enumerating both orders counts
// every unique board TWICE, so `countAll` (and `countMy`/`countTie`) is 2x the
// true number of unique combinations. The win-odds ratio is unchanged
// ((2*countMy + 0.5*2*countTie)/(2*countAll) == same), but the enumeration
// count is wrong and the work is doubled.
//
// Fix: Enumerate the river card only for j > i, so each unordered board is
// counted exactly once, matching the correct turn-case (case 2) pattern.
//
// This test replicates both strategies with a simplified hand evaluator and
// verifies the buggy strategy counts each board twice while the fixed strategy
// counts each board once; odds are identical in both (no probability bias).

#include <cstdio>
#include <cstdlib>
#include <array>

// Simplified card representation: 0..51 = suit*13 + rank
// Hand strength proxy: sum of card indices (higher = better). Using the full
// card index (not just rank) makes suit matter, so the turn/river-card
// assignment changes hand strengths differently. Not a real poker evaluator,
// just needs to order hands consistently and be order-insensitive (sorts).
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
static const int board[3] = {1, 14, 27};       // three kings-ish cards
static const int knownCount = 5;

// Returns true if card is one of the known cards.
static bool isKnown(int c) {
    for (int i = 0; i < 2; i++) if (c == myHole[i]) return true;
    for (int i = 0; i < 3; i++) if (c == board[i]) return true;
    return false;
}

// Buggy enumeration: turn i and river j are independent (both 0..51, j != i),
// so each unordered board {turn,river} is counted twice ((A,B) and (B,A)).
static void buggyEnumeration(long long& countAll, long long& countMy, long long& countTie, double& odds) {
    countAll = 0; countMy = 0; countTie = 0;

    int myCards[7] = {myHole[0], myHole[1], board[0], board[1], board[2], 0, 0};
    int oppCards[7] = {board[0], board[1], board[2], 0, 0, 0, 0};

    for (int turn = 0; turn < 52; turn++) {
        if (isKnown(turn)) continue;
        myCards[5] = turn;
        oppCards[5] = turn;

        for (int river = 0; river < 52; river++) {
            if (isKnown(river) || river == turn) continue;
            myCards[6] = river;
            oppCards[6] = river;

            for (int o1 = 0; o1 < 52; o1++) {
                if (isKnown(o1) || o1 == turn || o1 == river) continue;
                oppCards[0] = o1;

                for (int o2 = o1 + 1; o2 < 52; o2++) {
                    if (isKnown(o2) || o2 == turn || o2 == river || o2 == o1) continue;
                    oppCards[1] = o2;

                    countAll++;

                    int myValue = evalHand(myCards, 7);
                    int oppValue = evalHand(oppCards, 7);

                    if (myValue > oppValue) countMy++;
                    else if (myValue == oppValue) countTie++;
                }
            }
        }
    }

    odds = (countAll > 0) ? 100.0 * (countMy + 0.5 * countTie) / (countAll * 1.0) : 0.0;
}

// Fixed enumeration: river j only for j > turn i, so each unordered board is
// counted exactly once.
static void fixedEnumeration(long long& countAll, long long& countMy, long long& countTie, double& odds) {
    countAll = 0; countMy = 0; countTie = 0;

    int myCards[7] = {myHole[0], myHole[1], board[0], board[1], board[2], 0, 0};
    int oppCards[7] = {board[0], board[1], board[2], 0, 0, 0, 0};

    for (int turn = 0; turn < 52; turn++) {
        if (isKnown(turn)) continue;
        myCards[5] = turn;
        oppCards[5] = turn;

        for (int river = turn + 1; river < 52; river++) {
            if (isKnown(river)) continue;
            myCards[6] = river;
            oppCards[6] = river;

            for (int o1 = 0; o1 < 52; o1++) {
                if (isKnown(o1) || o1 == turn || o1 == river) continue;
                oppCards[0] = o1;

                for (int o2 = o1 + 1; o2 < 52; o2++) {
                    if (isKnown(o2) || o2 == turn || o2 == river || o2 == o1) continue;
                    oppCards[1] = o2;

                    countAll++;

                    int myValue = evalHand(myCards, 7);
                    int oppValue = evalHand(oppCards, 7);

                    if (myValue > oppValue) countMy++;
                    else if (myValue == oppValue) countTie++;
                }
            }
        }
    }

    odds = (countAll > 0) ? 100.0 * (countMy + 0.5 * countTie) / (countAll * 1.0) : 0.0;
}

int main() {
    long long bAll = 0, bMy = 0, bTie = 0;
    double bOdds = 0.0;
    long long fAll = 0, fMy = 0, fTie = 0;
    double fOdds = 0.0;

    buggyEnumeration(bAll, bMy, bTie, bOdds);
    fixedEnumeration(fAll, fMy, fTie, fOdds);

    // 47 unknown cards; turn+river unordered = C(47,2), opponent holes = C(45,2)
    const long long expectedFixed = 1081LL * 990LL; // 1,070,190
    const long long expectedBuggy = 2 * expectedFixed; // 2,138,580

    printf("Flop enumeration (5 known cards, 47 unknown):\n");
    printf("\nBuggy (turn/river ordered, each board counted twice):\n");
    printf("  countAll = %lld (expected %lld)\n", bAll, expectedBuggy);
    printf("  countMy  = %lld\n", bMy);
    printf("  countTie = %lld\n", bTie);
    printf("  odds     = %.4f%%\n", bOdds);
    printf("\nFixed (turn/river unordered, each board once):\n");
    printf("  countAll = %lld (expected %lld)\n", fAll, expectedFixed);
    printf("  countMy  = %lld\n", fMy);
    printf("  countTie = %lld\n", fTie);
    printf("  odds     = %.4f%%\n", fOdds);
    printf("\n");

    bool buggyIsDouble = (bAll == expectedBuggy);
    bool fixedIsCorrect = (fAll == expectedFixed);
    bool buggyIsDoubleFixed = (bAll == 2 * fAll);
    bool oddsIdentical = (bOdds == fOdds);

    printf("Buggy countAll == 2 * C(47,2) * C(45,2): %s\n", buggyIsDouble ? "PASS (double count confirmed)" : "FAIL");
    printf("Fixed countAll == C(47,2) * C(45,2): %s\n", fixedIsCorrect ? "PASS" : "FAIL");
    printf("Buggy countAll == 2 * fixed countAll: %s\n", buggyIsDoubleFixed ? "PASS" : "FAIL");
    printf("Odds identical (no probability bias): %s\n", oddsIdentical ? "PASS" : "FAIL");

    if (!buggyIsDouble || !fixedIsCorrect || !buggyIsDoubleFixed || !oddsIdentical) {
        printf("\nTEST FAILED\n");
        return 1;
    }

    printf("\nTEST PASSED: flop turn/river double-counting bug confirmed\n");
    return 0;
}
