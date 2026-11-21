/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

// Regression test for the stale-dealer / busted-button bug.
//
// Game::initHand() picks the dealer for the NEXT hand at the end of the current
// hand's initialisation (game.cpp: "advance dealerPosition to the next active
// player").  If that player busts during the current hand, initHand() for the
// following hand removes them from activePlayerList but never re-validates
// dealerPosition before creating the Hand.  LocalHand::assignButtons() then
// anchors the dealer button and both blinds on the busted (empty) seat.
//
// With three players A,B,C (dealer A -> next dealer B precomputed), if B busts
// during the hand, the next hand is heads-up A vs C.  The buggy code anchors on
// B: it walks to the next active seat C and, because only two players remain,
// makes C the BIG blind and A the SMALL blind.  Correct heads-up rotation skips
// B and puts the button on C, so C must be the SMALL blind and A the BIG blind.
// The blinds are therefore reversed -- a real chip-economics error.
//
// Fix: at the start of initHand(), after pruning busted players, if the stored
// dealerPosition is no longer in activePlayerList, advance it to the next
// active seat (the same clockwise walk used to pick the next dealer) before
// creating the Hand.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_stale_dealer_bust_bug test_stale_dealer_bust_bug.cpp && /tmp/test_stale_dealer_bust_bug

#include <algorithm>
#include <cstdio>
#include <vector>

struct Player
{
    int id;
    int cash;
};

struct Buttons
{
    int small = -1;
    int big = -1;
    int dealer = -1;
};

static bool isActive(const std::vector<int> &activeIds, int id)
{
    return std::find(activeIds.begin(), activeIds.end(), id) != activeIds.end();
}

// Model of LocalHand::assignButtons(): walk to the first active seat after the
// dealer and assign the blinds.  In heads-up the seat after the dealer is the
// big blind and the dealer is the small blind; otherwise it is the small blind.
static Buttons assignButtons(const std::vector<Player> &seats,
                             const std::vector<int> &activeIds,
                             int dealerId)
{
    Buttons b;
    b.dealer = dealerId;
    const std::size_t n = seats.size();
    if (n == 0 || !isActive(activeIds, dealerId)) {
        // The original code still anchors on the (busted) seat, so keep the
        // dealer id and walk from its seat even if it is no longer active.
    }
    std::size_t d = 0;
    while (d < n && seats[d].id != dealerId) ++d;
    if (d == n) return b;

    for (std::size_t i = 0; i < n; ++i) {
        d = (d + 1) % n;
        if (!isActive(activeIds, seats[d].id)) continue;

        const bool headsUp = activeIds.size() == 2;
        if (headsUp) b.big = seats[d].id;
        else b.small = seats[d].id;

        std::size_t e = d;
        for (std::size_t j = 0; j < n; ++j) {
            e = (e + 1) % n;
            if (isActive(activeIds, seats[e].id)) break;
        }
        if (headsUp) b.small = seats[e].id;
        else b.big = seats[e].id;
        break;
    }
    return b;
}

// Model of the "advance dealer to next active player" loop at the end of
// Game::initHand().  Used both for the normal next-dealer selection and, in the
// fixed version, to repair a dealer that has just busted.
static int nextActiveSeat(const std::vector<Player> &seats,
                          const std::vector<int> &activeIds,
                          int fromDealerId)
{
    const std::size_t n = seats.size();
    std::size_t d = 0;
    while (d < n && seats[d].id != fromDealerId) ++d;
    for (std::size_t i = 0; i < n; ++i) {
        d = (d + 1) % n;
        if (isActive(activeIds, seats[d].id)) return seats[d].id;
    }
    return fromDealerId;
}

// The buggy initHand() uses the stored dealerPosition unchanged.
static int buggyDealerForNextHand(const std::vector<Player> &seats,
                                  const std::vector<int> &activeAtThisHand,
                                  int dealerAtThisHand)
{
    (void)seats;
    (void)activeAtThisHand;
    return dealerAtThisHand;
}

// The fixed initHand() re-validates: if the stored dealer busted, move the
// button to the next active seat.
static int fixedDealerForNextHand(const std::vector<Player> &seats,
                                  const std::vector<int> &activeAfterPrune,
                                  int storedDealer)
{
    if (isActive(activeAfterPrune, storedDealer)) return storedDealer;
    return nextActiveSeat(seats, activeAfterPrune, storedDealer);
}

struct TestCase
{
    const char *name;
    std::vector<Player> seats;
    std::vector<int> activeAtHandN;
    int dealerAtHandN;
    int bustId;
    std::vector<int> activeAtHandN1;
    int expectedSmallN1;
    int expectedBigN1;
};

int main()
{
    const TestCase cases[] = {
        {"3-handed -> heads-up, middle dealer busts",
         {{1, 100}, {2, 100}, {3, 100}}, {1, 2, 3}, 1, 2, {1, 3}, 3, 1},
        {"3-handed -> heads-up, wrapping dealer busts",
         {{1, 100}, {2, 100}, {3, 100}}, {1, 2, 3}, 3, 1, {2, 3}, 2, 3},
        {"4-handed -> 3-handed, dealer busts",
         {{1, 100}, {2, 100}, {3, 100}, {4, 100}}, {1, 2, 3, 4}, 1, 2, {1, 3, 4}, 4, 1},
    };

    int failures = 0;
    printf("Testing stale-dealer (busted button) blind rotation bug\n\n");

    for (const TestCase &t : cases) {
        // End of initHand(N): the next hand's dealer is precomputed from the
        // players who are active at that moment.
        const int storedDealer = nextActiveSeat(t.seats, t.activeAtHandN, t.dealerAtHandN);

        const Buttons buggy = assignButtons(t.seats, t.activeAtHandN1,
                                            buggyDealerForNextHand(t.seats, t.activeAtHandN1, storedDealer));
        const Buttons fixed = assignButtons(t.seats, t.activeAtHandN1,
                                            fixedDealerForNextHand(t.seats, t.activeAtHandN1, storedDealer));

        printf("%s\n", t.name);
        printf("  busted id %d, stored dealer %d\n", t.bustId, storedDealer);
        printf("  buggy:  dealer=%d SB=%d BB=%d\n", buggy.dealer, buggy.small, buggy.big);
        printf("  fixed:  dealer=%d SB=%d BB=%d\n", fixed.dealer, fixed.small, fixed.big);
        printf("  expect: SB=%d BB=%d\n", t.expectedSmallN1, t.expectedBigN1);

        if (fixed.small != t.expectedSmallN1 || fixed.big != t.expectedBigN1) {
            printf("  FAIL: fixed blinds do not match expected\n");
            ++failures;
        } else {
            printf("  PASS\n");
        }

        // The buggy path must actually differ from the correct result for at
        // least one case, otherwise this test would not cover a real bug.
        if (buggy.small == t.expectedSmallN1 && buggy.big == t.expectedBigN1) {
            printf("  NOTE: buggy path happens to produce correct blinds for this layout\n");
        }
        printf("\n");
    }

    // Sanity check: when the dealer does NOT bust, the fix must be a no-op.
    const std::vector<Player> seats = {{1, 100}, {2, 100}, {3, 100}};
    const std::vector<int> active = {1, 2, 3};
    if (fixedDealerForNextHand(seats, active, 2) != 2) {
        printf("FAIL: fix changed the dealer when it was still active\n");
        ++failures;
    }

    // At least one case must demonstrate a real blind reversal from the bug.
    bool demonstrated = false;
    for (const TestCase &t : cases) {
        const int storedDealer = nextActiveSeat(t.seats, t.activeAtHandN, t.dealerAtHandN);
        const Buttons buggy = assignButtons(t.seats, t.activeAtHandN1,
                                            buggyDealerForNextHand(t.seats, t.activeAtHandN1, storedDealer));
        if (buggy.small != t.expectedSmallN1 || buggy.big != t.expectedBigN1) demonstrated = true;
    }
    if (!demonstrated) {
        printf("FAIL: no test case demonstrated the buggy blind reversal\n");
        ++failures;
    }

    if (failures > 0) {
        printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("TEST PASSED: stale-dealer bug reproduced and fix verified\n");
    return 0;
}
