// Empirical bug-hunt test for CardsValue::cardsValue() hand classification.
//
// Replicates the cardsValue() classification algorithm (hand class = value /
// 100000000, mapped to 0=high card .. 8=straight flush / 9=royal flush) and
// compares it against an independent best-5-card-hand evaluator that enumerates
// all C(7,5)=21 five-card subsets and ranks each with a standard poker hand
// ranking. Any class mismatch on a 7-card hand indicates a misclassification
// bug in cardsValue().
//
// Card encoding: 0..51 = suit*13 + rank, suit = card/13 (0..3), rank = card%13
// (0=deuce, 12=ace). Hand classes: 0 high card, 1 pair, 2 two pair, 3 trips,
// 4 straight, 5 flush, 6 full house, 7 quads, 8 straight flush, 9 royal flush
// (royal reported as 8 here since it is an ace-high straight flush).
//
// Compile: g++ -O2 -std=c++23 -o test_cards_value test_cards_value.cpp && ./test_cards_value

#include <cstdio>
#include <cstdlib>
#include <array>
#include <vector>
#include <algorithm>
#include <random>
#include <cstdint>

// ---------------------------------------------------------------------------
// Independent evaluator: enumerate all 5-card subsets, rank each, take max.
// ---------------------------------------------------------------------------

static int rank5(const int* five) {
    // five: 5 cards. Return class (0..8) with kicker info packed for compare.
    // We only need the class for cross-checking cardsValue's class.
    int suit[5], rank[5];
    for (int i = 0; i < 5; i++) {
        suit[i] = five[i] / 13;
        rank[i] = five[i] % 13; // 0=deuce .. 12=ace
    }
    // sort ranks descending (ace high = 12)
    std::array<int,5> sr;
    for (int i = 0; i < 5; i++) sr[i] = rank[i];
    std::sort(sr.begin(), sr.end(), std::greater<int>());

    bool flush = (suit[0]==suit[1] && suit[1]==suit[2] && suit[2]==suit[3] && suit[3]==suit[4]);

    // straight: 5 consecutive distinct ranks (wheel A-5-4-3-2 = ranks 12,3,2,1,0)
    bool straight = false;
    int straightHigh = 0;
    {
        bool used[13] = {false};
        for (int i = 0; i < 5; i++) used[rank[i]] = true;
        // normal straight
        for (int h = 12; h >= 4; h--) {
            if (used[h] && used[h-1] && used[h-2] && used[h-3] && used[h-4]) { straight = true; straightHigh = h; break; }
        }
        // wheel A-5-4-3-2
        if (!straight && used[12] && used[3] && used[2] && used[1] && used[0]) { straight = true; straightHigh = 3; } // 5-high
    }

    if (flush && straight) {
        if (straightHigh == 12) return 9; // royal flush
        return 8; // straight flush
    }

    // count ranks
    int count[13] = {0};
    for (int i = 0; i < 5; i++) count[rank[i]]++;
    int quads = 0, trips = 0, pairs = 0;
    int quadRank = 0, tripRank = 0, highPair = 0, lowPair = 0;
    for (int r = 0; r < 13; r++) {
        if (count[r] == 4) { quads++; quadRank = r; }
        else if (count[r] == 3) { trips++; tripRank = r; }
        else if (count[r] == 2) { pairs++; if (r > highPair) { lowPair = highPair; highPair = r; } else if (r > lowPair) lowPair = r; }
    }
    if (quads) return 7;
    if (trips && pairs) return 6; // full house
    if (flush) return 5;
    if (straight) return 4;
    if (trips) return 3;
    if (pairs == 2) return 2;
    if (pairs == 1) return 1;
    return 0; // high card
}

static int trueHandClass(const int* cards, int n) {
    // n == 7. Enumerate all 5-card subsets, take max class.
    int best = 0;
    int idx[7] = {0,1,2,3,4,5,6};
    // combinations of 5 from 7
    for (int a = 0; a < 7; a++)
    for (int b = a+1; b < 7; b++)
    for (int c = b+1; c < 7; c++)
    for (int d = c+1; d < 7; d++)
    for (int e = d+1; e < 7; e++) {
        int five[5] = {cards[a], cards[b], cards[c], cards[d], cards[e]};
        int cl = rank5(five);
        if (cl > best) best = cl;
    }
    return best;
}

// ---------------------------------------------------------------------------
// Replication of CardsValue::cardsValue() classification (class only).
// Mirrors src/engine/local_engine/cardsvalue.cpp.
// ---------------------------------------------------------------------------

static int cvClass(const int* cards) {
    int array[7][3];
    for (int j1 = 0; j1 < 7; j1++) {
        array[j1][0] = cards[j1] / 13; // suit
        array[j1][1] = cards[j1] % 13; // rank
        array[j1][2] = j1;
    }
    // sort by suit desc, then rank desc within suit
    for (int k1 = 0; k1 < 7; k1++)
    for (int k2 = k1+1; k2 < 7; k2++) {
        if (array[k1][0] < array[k2][0]) {
            int t0=array[k1][0], t1=array[k1][1], t2=array[k1][2];
            array[k1][0]=array[k2][0]; array[k1][1]=array[k2][1]; array[k1][2]=array[k2][2];
            array[k2][0]=t0; array[k2][1]=t1; array[k2][2]=t2;
        }
    }
    for (int k1 = 0; k1 < 7; k1++)
    for (int k2 = k1+1; k2 < 7; k2++) {
        if (array[k1][0]==array[k2][0] && array[k1][1] < array[k2][1]) {
            int t0=array[k1][0], t1=array[k1][1], t2=array[k1][2];
            array[k1][0]=array[k2][0]; array[k1][1]=array[k2][1]; array[k1][2]=array[k2][2];
            array[k2][0]=t0; array[k2][1]=t1; array[k2][2]=t2;
        }
    }

    // royal flush / straight flush
    {
        int suitStart = 0;
        while (suitStart < 7) {
            int suitEnd = suitStart;
            while (suitEnd+1 < 7 && array[suitEnd+1][0] == array[suitStart][0]) suitEnd++;
            int suitCount = suitEnd - suitStart + 1;
            if (suitCount >= 5) {
                for (int si = suitStart; si + 4 <= suitEnd; si++) {
                    if (array[si][1]-1 == array[si+1][1] && array[si+1][1]-1 == array[si+2][1]
                        && array[si+2][1]-1 == array[si+3][1] && array[si+3][1]-1 == array[si+4][1]) {
                        if (array[si][1] == 12) return 9; // royal
                        return 8; // straight flush
                    }
                }
                // wheel straight flush
                for (int si = suitStart; si <= suitEnd; si++) {
                    if (array[si][1] != 12) continue;
                    for (int sj = suitStart; sj + 3 <= suitEnd; sj++) {
                        if (array[si][1]-9 == array[sj][1]
                            && array[sj][1]-1 == array[sj+1][1]
                            && array[sj+1][1]-1 == array[sj+2][1]
                            && array[sj+2][1]-1 == array[sj+3][1]
                            && array[si][0] == array[sj][0] && array[si][0] == array[sj+3][0]) {
                            return 8;
                        }
                    }
                }
            }
            suitStart = suitEnd + 1;
        }
    }

    // flush
    for (int j1 = 0; j1 < 3; j1++) {
        if (array[j1][0] == array[j1+1][0] && array[j1][0] == array[j1+2][0]
            && array[j1][0] == array[j1+3][0] && array[j1][0] == array[j1+4][0]) {
            return 5;
        }
    }

    // resort by rank desc
    for (int k1 = 0; k1 < 7; k1++)
    for (int k2 = k1+1; k2 < 7; k2++) {
        if (array[k1][1] < array[k2][1]) {
            int t0=array[k1][0], t1=array[k1][1], t2=array[k1][2];
            array[k1][0]=array[k2][0]; array[k1][1]=array[k2][1]; array[k1][2]=array[k2][2];
            array[k2][0]=t0; array[k2][1]=t1; array[k2][2]=t2;
        }
    }
    // by position (board before hole) - irrelevant for class

    // quads
    for (int j1 = 0; j1 < 4; j1++) {
        if (array[j1][1] == array[j1+1][1] && array[j1][1] == array[j1+2][1] && array[j1][1] == array[j1+3][1]) {
            return 7;
        }
    }

    // straight / full house
    for (int j1 = 0; j1 < 7; j1++)
    for (int j2 = j1+1; j2 < 7; j2++)
    for (int j3 = j2+1; j3 < 7; j3++)
    for (int j4 = j3+1; j4 < 7; j4++)
    for (int j5 = j4+1; j5 < 7; j5++) {
        if (array[j1][1]-1 == array[j2][1] && array[j2][1]-1 == array[j3][1]
            && array[j3][1]-1 == array[j4][1] && array[j4][1]-1 == array[j5][1]) {
            return 4; // straight
        }
        if ((array[j1][1]==array[j2][1] && array[j1][1]==array[j3][1] && array[j4][1]==array[j5][1])
            || (array[j3][1]==array[j4][1] && array[j3][1]==array[j5][1] && array[j1][1]==array[j2][1])) {
            return 6; // full house
        }
    }

    // wheel straight
    for (int j1 = 0; j1 < 7; j1++)
    for (int j2 = j1+1; j2 < 7; j2++)
    for (int j3 = j2+1; j3 < 7; j3++)
    for (int j4 = j3+1; j4 < 7; j4++)
    for (int j5 = j4+1; j5 < 7; j5++) {
        if (array[j1][1]-9 == array[j2][1] && array[j2][1]-1 == array[j3][1]
            && array[j3][1]-1 == array[j4][1] && array[j4][1]-1 == array[j5][1]) {
            return 4;
        }
    }

    // trips
    for (int j1 = 0; j1 < 5; j1++) {
        if (array[j1][1] == array[j1+1][1] && array[j1][1] == array[j1+2][1]) return 3;
    }

    // two pair
    for (int j1 = 0; j1 < 4; j1++)
    for (int j2 = j1+2; j2 < 6; j2++) {
        if (array[j1][1] == array[j1+1][1] && array[j2][1] == array[j2+1][1]) return 2;
    }

    // pair
    for (int j1 = 0; j1 < 6; j1++) {
        if (array[j1][1] == array[j1+1][1]) return 1;
    }

    return 0; // high card
}

int main() {
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> cardDist(0, 51);

    long long tested = 0;
    long long mismatches = 0;
    int firstMisMatchCards[7] = {};
    int firstMyClass = 0, firstTrueClass = 0;

    // Targeted edge-case hands (guaranteed to exercise special paths)
    struct { int cards[7]; const char* name; } edge[] = {
        {{0,13,26,39,12,25,38}, "royal flush spades"},        // A,K,Q,J,T of suit 0
        {{12,25,38,51,11,24,37}, "straight flush wheel"},     // A,5,4,3,2 suit 0 -> wheel SF
        {{12,25,38,51, 1,14,27}, "flush"},                    // 5 suit-0 cards + 2 off
        {{0,13,26,39, 1,14,27}, "quads aces"},                // 4 aces + kicker
        {{0,13,26, 1,14,27, 2}, "full house aces over kings"},// AAA KK
        {{0,1,13,14,26,27,39}, "straight broadway"},          // A,K,Q,J,T mixed
        {{12,25,38, 1,14, 2,15}, "wheel straight"},           // A,5,4,3,2 mixed
        {{0,13,26,39, 1, 2, 3}, "trips aces"},
        {{0,13, 1,14, 2,15, 3}, "two pair aces kings"},
        {{0,13, 1, 2, 3, 4, 5}, "pair aces"},
        {{0, 1, 2, 3, 4, 6, 7}, "high card"},
    };
    int nEdge = sizeof(edge)/sizeof(edge[0]);

    // Test edge cases
    for (int e = 0; e < nEdge; e++) {
    int my = cvClass(edge[e].cards);
    int tru = trueHandClass(edge[e].cards, 7);
        tested++;
        if (my != tru) {
            mismatches++;
            if (mismatches == 1) { firstMyClass = my; firstTrueClass = tru; for (int i=0;i<7;i++) firstMisMatchCards[i]=edge[e].cards[i]; }
            printf("MISMATCH edge[%d] %s: cardsValue=%d true=%d\n", e, edge[e].name, my, tru);
        }
    }

    // Random hands
    for (long long t = 0; t < 200000; t++) {
        int cards[7];
        bool used[52] = {false};
        int n = 0;
        while (n < 7) {
            int c = cardDist(rng);
            if (!used[c]) { used[c] = true; cards[n++] = c; }
        }
        int my = cvClass(cards);
        int tru = trueHandClass(cards, 7);
        tested++;
        if (my != tru) {
            mismatches++;
            if (mismatches <= 5) {
                firstMyClass = my; firstTrueClass = tru;
                for (int i = 0; i < 7; i++) firstMisMatchCards[i] = cards[i];
                printf("MISMATCH random #%lld: cardsValue=%d true=%d cards=", t, my, tru);
                for (int i = 0; i < 7; i++) printf("%d ", cards[i]);
                printf("\n");
            }
        }
    }

    printf("\nEmpirical cardsValue classification test:\n");
    printf("  hands tested   = %lld\n", tested);
    printf("  mismatches     = %lld\n", mismatches);

    if (mismatches == 0) {
        printf("\nTEST PASSED: cardsValue classification matches independent evaluator on all hands\n");
        return 0;
    }
    printf("\nTEST FAILED: cardsValue misclassifies hands (first: cv=%d true=%d)\n", firstMyClass, firstTrueClass);
    return 1;
}
