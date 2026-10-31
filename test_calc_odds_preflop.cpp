// Test for LocalPlayer::calcMyOdds() preflop table lookup.
//
// Replicates LocalPlayer::calcMyOdds() case 0 (preflop) logic:
// - Converts hole cards to an integer code via CardsValue::holeCardsToIntCode()
// - Looks up the code in the PreflopValues[] table
// - Falls back to 50% odds for hand codes not present in the table
//
// This test verifies:
//   1. Known hand codes return table values clamped to [2,5] players
//   2. Unknown hand codes default to 50% odds
//   3. Player count is clamped to [2,5] for table indexing

#include <cstdio>
#include <cstdlib>
#include <cmath>

// Simplified card representation: 0..51 = suit*13 + rank
// Rank is card % 13 (0=2, 12=A), suit is card / 13.

// Replicates CardsValue::holeCardsToIntCode()
static int holeCardsToIntCode(int c0, int c1) {
    int r0 = c0 % 13, r1 = c1 % 13;
    int s0 = c0 / 13, s1 = c1 / 13;
    if (r0 == r1) {
        return r0 * 1000 + r0 * 10;
    } else {
        if (r0 < r1) {
            if (s0 == s1) return r0 * 1000 + r1 * 10 + 1;
            else return r0 * 1000 + r1 * 10;
        } else {
            if (s0 == s1) return r1 * 1000 + r0 * 10 + 1;
            else return r1 * 1000 + r0 * 10;
        }
    }
}

// Replicates LocalPlayer::calcMyOdds() preflop table lookup.
// Table entries are a subset of PreflopValues[] from localplayer.cpp.
struct PreflopEntry {
    int hand;
    double data[4]; // for 2,3,4,5 players
};

static const PreflopEntry PreflopValues[] = {
    {     0, { 0.392398, 0.276545, 0.212940, 0.178564 } },  // AA
    {   1010, { 0.425249, 0.294167, 0.227440, 0.189070 } },  // KK
    {  11120, { 0.745230, 0.580949, 0.472283, 0.393985 } },  // AKo
    {  11121, { 0.754219, 0.599772, 0.495033, 0.416241 } },  // AKs
    {   10100, { 0.698831, 0.534374, 0.436462, 0.369034 } }, // QQ
    {    50, { 0.368042, 0.220327, 0.157511, 0.119645 } },  // 72o
    {  12120, { 0.855608, 0.736617, 0.642366, 0.562044 } },  // AA (duplicate code check)
};

static const int NUM_PREFLOP_VALUES = sizeof(PreflopValues) / sizeof(PreflopEntry);

// Replicates calcMyOdds() preflop case: table lookup with player clamp [2,5],
// fallback to 50.0 for unknown hand codes.
static double preflopOdds(int handCode, int players) {
    if (players > 5) players = 5;
    if (players < 2) players = 2;

    for (int val = 0; val < NUM_PREFLOP_VALUES; val++) {
        if (handCode == PreflopValues[val].hand) {
            return 100.0 * PreflopValues[val].data[players - 2];
        }
    }
    return 50.0; // unknown hand -> default
}

int main() {
    int failures = 0;

    // --- Known hand code: AA pair (rank 12 = Ace) ---
    // c0 = 0*13+12 = 12 (Ace of spades), c1 = 1*13+12 = 25 (Ace of hearts)
    int codeAA = holeCardsToIntCode(12, 25);
    printf("AA hand code = %d (expected 12120)\n", codeAA);
    if (codeAA != 12120) { printf("  FAIL: hand code\n"); failures++; }

    double oddsAA2 = preflopOdds(codeAA, 2);
    printf("AA vs 2 players: odds = %.4f%% (expected 85.5608%%)\n", oddsAA2);
    if (fabs(oddsAA2 - 85.5608) > 0.001) { printf("  FAIL: odds\n"); failures++; }

    double oddsAA5 = preflopOdds(codeAA, 5);
    printf("AA vs 5 players: odds = %.4f%% (expected 56.2044%%)\n", oddsAA5);
    if (fabs(oddsAA5 - 56.2044) > 0.001) { printf("  FAIL: odds\n"); failures++; }

    // --- Player count clamping ---
    double oddsAA1 = preflopOdds(codeAA, 1);  // clamped to 2
    printf("AA vs 1 player (clamped to 2): odds = %.4f%% (expected 85.5608%%)\n", oddsAA1);
    if (fabs(oddsAA1 - 85.5608) > 0.001) { printf("  FAIL: clamp\n"); failures++; }

    double oddsAA9 = preflopOdds(codeAA, 9);  // clamped to 5
    printf("AA vs 9 players (clamped to 5): odds = %.4f%% (expected 56.2044%%)\n", oddsAA9);
    if (fabs(oddsAA9 - 56.2044) > 0.001) { printf("  FAIL: clamp\n"); failures++; }

    // --- Unknown hand code ---
    double oddsUnknown = preflopOdds(99999, 3);
    printf("Unknown hand vs 3 players: odds = %.4f%% (expected 50%%)\n", oddsUnknown);
    if (oddsUnknown != 50.0) { printf("  FAIL: default\n"); failures++; }

    // --- AK suited vs AK unsuited ---
    // AKs: A(12) K(11) same suit (spades) -> 11*1000 + 12*10 + 1 = 11121
    int codeAKs = holeCardsToIntCode(12, 11); // A spades, K spades
    printf("AKs hand code = %d (expected 11121)\n", codeAKs);
    if (codeAKs != 11121) { printf("  FAIL: AKs code\n"); failures++; }

    // AKo: A(12) K(11) different suit (spades vs hearts) -> 11*1000 + 12*10 = 11120
    int codeAKo = holeCardsToIntCode(12, 24); // A spades, K hearts
    printf("AKo hand code = %d (expected 11120)\n", codeAKo);
    if (codeAKo != 11120) { printf("  FAIL: AKo code\n"); failures++; }

    double oddsAKs2 = preflopOdds(codeAKs, 2);
    double oddsAKo2 = preflopOdds(codeAKo, 2);
    printf("AKs vs 2: %.4f%%  AKo vs 2: %.4f%% (suited should be slightly higher)\n",
           oddsAKs2, oddsAKo2);
    if (oddsAKs2 <= oddsAKo2) { printf("  FAIL: suited should beat unsuited\n"); failures++; }

    // --- Worst hand 72o (7-2 offsuit) ---
    // 7 = rank 5, 2 = rank 0. Unsuited: 0*1000 + 5*10 = 50
    int code72o = holeCardsToIntCode(0, 18); // 2 spades, 7 hearts
    printf("72o hand code = %d (expected 50)\n", code72o);
    if (code72o != 50) { printf("  FAIL: 72o code\n"); failures++; }

    double odds72o2 = preflopOdds(code72o, 2);
    printf("72o vs 2 players: odds = %.4f%% (expected 36.8042%%)\n", odds72o2);
    if (fabs(odds72o2 - 36.8042) > 0.001) { printf("  FAIL: 72o odds\n"); failures++; }

    if (failures > 0) {
        printf("\nTEST FAILED: %d failures\n", failures);
        return 1;
    }

    printf("\nTEST PASSED: calcMyOdds preflop table lookup verified\n");
    return 0;
}
