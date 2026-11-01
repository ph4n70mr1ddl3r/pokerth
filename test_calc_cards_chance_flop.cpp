// Standalone regression test for CardsValue::calcCardsChance() GAME_STATE_FLOP enumeration.
//
// The flop case enumerates the turn and river cards from the unknown cards:
//   for(i=0; i<51; i++) {          // turn card (bound was i < 51)
//       if(i not in known) {
//           for(j=i+1; j<52; j++) { // river card
//               if(j not in known) {
//                   cards[5] = i;   // turn
//                   cards[6] = j;   // river
//
// Bug: the turn-card bound `i < 51` excludes card 51 from ever being the turn card.
// Card 51 can only appear as the river card (j=51). While turn and river are
// symmetric community cards (cardsValue() sorts the 7 cards internally, so board
// {turn=X,river=51} equals board {turn=51,river=X}), the correct and obvious bound
// for the turn card is `i < 52` so it ranges over all cards. This defensive fix
// prevents a latent off-by-one if the evaluator's symmetry assumption ever changes.
//
// This test replicates the enumeration and verifies it covers every unordered pair
// {turn,river} of unknown cards exactly once (count == C(47,2) with 5 known cards).

#include <cstdio>
#include <cstdlib>
#include <array>
#include <algorithm>

// Card: 0..51 = suit*13 + rank. 5 known cards (2 hole + 3 board), 47 unknown.
static const int known[5] = {0, 13, 26, 39, 2};
static bool isKnown(int c) {
    for (int i = 0; i < 5; i++) if (c == known[i]) return true;
    return false;
}

// Simplified evaluator: hand class proxy = number of distinct ranks.
// cardsValue() sorts internally, so turn/river order does not affect the class.
static int handClass(const int* cards) {
    std::array<int, 7> sorted;
    for (int i = 0; i < 7; i++) sorted[i] = cards[i];
    std::sort(sorted.begin(), sorted.end());
    int distinct = 1;
    for (int i = 1; i < 7; i++) if (sorted[i] != sorted[i-1]) distinct++;
    return distinct;
}

// Current (buggy) enumeration: turn card i < 51.
static void currentEnum(long long& countAll, long long classCount[10]) {
    countAll = 0;
    for (int i = 0; i < 10; i++) classCount[i] = 0;
    int cards[7];
    for (int i = 0; i < 5; i++) cards[i] = known[i];
    for (int i = 0; i < 51; i++) {
        if (isKnown(i)) continue;
        cards[5] = i;
        for (int j = i + 1; j < 52; j++) {
            if (isKnown(j)) continue;
            cards[6] = j;
            countAll++;
            classCount[handClass(cards) % 10]++;
        }
    }
}

// Fixed enumeration: turn card i < 52.
static void fixedEnum(long long& countAll, long long classCount[10]) {
    countAll = 0;
    for (int i = 0; i < 10; i++) classCount[i] = 0;
    int cards[7];
    for (int i = 0; i < 5; i++) cards[i] = known[i];
    for (int i = 0; i < 52; i++) {
        if (isKnown(i)) continue;
        cards[5] = i;
        for (int j = i + 1; j < 52; j++) {
            if (isKnown(j)) continue;
            cards[6] = j;
            countAll++;
            classCount[handClass(cards) % 10]++;
        }
    }
}

// Brute-force reference: enumerate every unordered pair of unknown cards directly.
static void bruteForce(long long& countAll, long long classCount[10]) {
    countAll = 0;
    for (int i = 0; i < 10; i++) classCount[i] = 0;
    int cards[7];
    for (int i = 0; i < 5; i++) cards[i] = known[i];
    int unknown[52];
    int nUnknown = 0;
    for (int c = 0; c < 52; c++) if (!isKnown(c)) unknown[nUnknown++] = c;
    for (int a = 0; a < nUnknown; a++) {
        for (int b = a + 1; b < nUnknown; b++) {
            cards[5] = unknown[a];
            cards[6] = unknown[b];
            countAll++;
            classCount[handClass(cards) % 10]++;
        }
    }
}
int main() {
    long long cAll = 0, fAll = 0, bAll = 0;
    long long cClass[10], fClass[10], bClass[10];

    currentEnum(cAll, cClass);
    fixedEnum(fAll, fClass);
    bruteForce(bAll, bClass);

    printf("calcCardsChance flop enumeration:\n");
    printf("  Current (i<51)   countAll = %lld\n", cAll);
    printf("  Fixed   (i<52)   countAll = %lld\n", fAll);
    printf("  Brute-force            countAll = %lld (expected C(47,2) = %d)\n", bAll, 47*46/2);

    bool currentCorrect = (cAll == bAll);
    bool fixedCorrect = (fAll == bAll);
    bool classesMatch = true;
    for (int i = 0; i < 10; i++) {
        if (cClass[i] != bClass[i] || fClass[i] != bClass[i]) classesMatch = false;
        printf("  class %d: current=%lld fixed=%lld brute=%lld %s\n", i,
               cClass[i], fClass[i], bClass[i],
               (cClass[i]==bClass[i] && fClass[i]==bClass[i]) ? "" : " <--- MISMATCH");
    }

    printf("\nCurrent (i<51) matches brute force: %s\n", currentCorrect ? "PASS" : "FAIL");
    printf("Fixed   (i<52) matches brute force: %s\n", fixedCorrect ? "PASS" : "FAIL");
    printf("Class distributions match: %s\n", classesMatch ? "PASS" : "FAIL");

    if (!currentCorrect || !fixedCorrect || !classesMatch) {
        printf("\nTEST FAILED\n");
        return 1;
    }

    printf("\nTEST PASSED: flop enumeration is exhaustive and correct\n");
    return 0;
}
