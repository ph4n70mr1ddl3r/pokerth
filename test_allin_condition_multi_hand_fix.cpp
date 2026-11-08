// Test for LocalBoard::allInCondition sticky flag fix across multiple hands.
//
// Bug: LocalBoard::allInCondition is set to true during all-in situations
// (LocalHand::switchRounds() calls setAllInCondition(true) at localboard.cpp
// lines 588, 598, 617, 626) but is NEVER reset to false. Since LocalBoard is
// shared across all hands within a Game session (Game::initHand() reuses the
// same currentBoard object created once in the Game constructor), once an
// all-in condition occurs, EVERY subsequent hand forces ALL non-folded
// players to show their hole cards at showdown -- even in normal showdowns
// where losing players should be allowed to muck (information leak).
//
// Fix: Reset allInCondition to false at the start of each hand in
// Game::initHand(), before the new hand begins (game.cpp:167).
//
//
// Card/show-card logic:
//   - allInCondition == true  -> every non-folded active player shows
//   - allInCondition == false -> only player(s) with the best hand show
//
// Compile: g++ -O2 -std=c++23 -o test_allin_condition_multi_hand_fix test_allin_condition_multi_hand_fix.cpp && ./test_allin_condition_multi_hand_fix

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <list>
#include <algorithm>

// ---------------------------------------------------------------------------
// Mock player: minimal subset of PlayerInterface state needed by the show-card
// logic.
// ---------------------------------------------------------------------------
struct MockPlayer {
    unsigned uniqueId;
    bool active;
    int action;        // 0 = not folded, 1 = PLAYER_ACTION_FOLD
    int cardsValue;    // hand strength for showdown comparison
};

// ---------------------------------------------------------------------------
// Minimal replica of LocalBoard's allInCondition flag and the show-card logic
// from LocalBoard::determinePlayerNeedToShowCards().
// ---------------------------------------------------------------------------
struct MockBoard {
    bool allInCondition = false;
    std::list<unsigned> playerNeedToShowCards;

    void setAllInCondition(bool theValue) {
        allInCondition = theValue;
    }

    void determinePlayerNeedToShowCards(const std::vector<MockPlayer>& activePlayers) {
        playerNeedToShowCards.clear();

        if (allInCondition) {
            // allInCondition: everybody who did not fold has to show
            for (const auto& p : activePlayers) {
                if (p.action != 1) {
                    playerNeedToShowCards.push_back(p.uniqueId);
                }
            }
        } else {
            // normal showdown: only the best hand(s) show
            int bestValue = 0;
            for (const auto& p : activePlayers) {
                if (p.action != 1 && p.cardsValue > bestValue) {
                    bestValue = p.cardsValue;
                }
            }
            for (const auto& p : activePlayers) {
                if (p.action != 1 && p.cardsValue == bestValue) {
                    playerNeedToShowCards.push_back(p.uniqueId);
                }
            }
        }
    }

    std::vector<unsigned> showingIds() const {
        return std::vector<unsigned>(playerNeedToShowCards.begin(), playerNeedToShowCards.end());
    }
};

// ---------------------------------------------------------------------------
// Simulate a hand's showdown given a board state and active players.
// applyFix == true  -> reset allInCondition to false at start of hand (the fix)
// applyFix == false -> leave flag as-is from previous hand (the bug)
// ---------------------------------------------------------------------------
static std::vector<unsigned> simulateHandShowdown(MockBoard& board,
                                                   const std::vector<MockPlayer>& activePlayers,
                                                   bool triggerAllIn,
                                                   bool applyFix) {
    // Start of hand: if fix is applied, reset the sticky flag.
    if (applyFix) {
        board.setAllInCondition(false);
    }

    // During the hand, an all-in condition may be triggered.
    if (triggerAllIn) {
        board.setAllInCondition(true);
    }

    // Showdown: determine who needs to show cards.
    board.determinePlayerNeedToShowCards(activePlayers);
    return board.showingIds();
}

// Test A: Without fix, bug persists across hands
// Test B: With fix, flag is cleared each hand
int main() {
    // Active players (non-folded) for a normal hand:
    //   P1 (id 1): best hand, cardsValue = 200
    //   P2 (id 2): worse hand, cardsValue = 100
    //   P3 (id 3): folded (action == 1)
    std::vector<MockPlayer> normalPlayers = {
        { 1, true, 0, 200 },
        { 2, true, 0, 100 },
        { 3, true, 1,  50 }
    };

    // Active players for an all-in hand:
    //   P1 and P2 all-in, both show in all-in showdown regardless of strength.
    std::vector<MockPlayer> allInPlayers = {
        { 1, true, 0, 200 },
        { 2, true, 0, 100 }
    };

    printf("=== Test: allInCondition sticky-flag fix across multiple hands ===\n\n");

    // -------------------------------------------------------
    // Test A: Without fix (bug persists)
    // -------------------------------------------------------
    printf("Test A: Without fix (bug should persist across hands)\n");
    printf("--------------------------------------------------------\n");

    MockBoard boardNoFix;

    // --- Hand 1: all-in condition triggered ---
    std::vector<unsigned> h1 = simulateHandShowdown(boardNoFix, allInPlayers, true, false);
    printf("Hand 1 (all-in triggered, no fix): ");
    for (unsigned id : h1) printf("%u ", id);
    printf("\n");
    bool h1BothShow = (h1.size() == 2);
    if (h1BothShow) { printf("  Hand 1: both show (correct for all-in)\n"); }
    else { printf("  Hand 1: unexpected result\n"); }

    // --- Hand 2: normal hand, NO fix (the bug) ---
    // Expected (buggy): allInCondition still true from Hand 1 -> P1 AND P2 both
    // show, even though P2 has a worse hand and should muck in a normal showdown.
    std::vector<unsigned> h2Buggy = simulateHandShowdown(boardNoFix, normalPlayers, false, false);
    printf("Hand 2 (normal, NO fix -- bug): ");
    for (unsigned id : h2Buggy) printf("%u ", id);
    printf("\n");
    bool bugConfirmed = (h2Buggy.size() == 2); // both show even though P2 should muck
    if (bugConfirmed) { printf("  BUG CONFIRMED: both show despite P2 having worse hand\n"); }
    else { printf("  Bug NOT confirmed: unexpected\n"); }

    // --- Hand 3: normal hand, NO fix (bug still persists) ---
    std::vector<unsigned> h3Buggy = simulateHandShowdown(boardNoFix, normalPlayers, false, false);
    printf("Hand 3 (normal, NO fix -- bug persists): ");
    for (unsigned id : h3Buggy) printf("%u ", id);
    printf("\n");
    bool bugPersistent = (h3Buggy.size() == 2);
    if (bugPersistent) { printf("  Bug persists: PASS (bug confirmed across hands)\n"); }
    else { printf("  Bug does not persist: FAIL (unexpected - fix may already be applied)\n"); }

    printf("\n");

    // -------------------------------------------------------
    // Test B: With fix (flag cleared each hand)
    // -------------------------------------------------------
    printf("\nTest B: With fix (flag should be cleared each hand)\n");
    printf("--------------------------------------------------------\n");

    MockBoard boardWithFix;

    // --- Hand 1: all-in condition triggered ---
    std::vector<unsigned> h1f = simulateHandShowdown(boardWithFix, allInPlayers, true, true);
    printf("Hand 1 (all-in triggered, WITH fix): ");
    for (unsigned id : h1f) printf("%u ", id);
    printf("\n");
    bool h1BothShowF = (h1f.size() == 2);
    if (h1BothShowF) { printf("  Hand 1: both show (correct for all-in with fix)\n"); }
    else { printf("  Hand 1: unexpected result\n"); }

    // --- Hand 2: normal hand, WITH fix ---
    // Expected (fixed): allInCondition reset to false -> only P1 (best hand)
    // shows; P2 (worse hand) gets to muck.
    std::vector<unsigned> h2Fixed = simulateHandShowdown(boardWithFix, normalPlayers, false, true);
    printf("Hand 2 (normal, WITH fix): ");
    for (unsigned id : h2Fixed) printf("%u ", id);
    printf("\n");
    bool fixWorks2 = (h2Fixed.size() == 1 && h2Fixed[0] == 1); // only P1 shows
    if (fixWorks2) { printf("  Fix works: only best hand shows (P1)\n"); }
    else { printf("  Fix FAIL: expected only P1 to show\n"); }

    // --- Hand 3: normal hand, WITH fix (second hand) ---
    std::vector<unsigned> h3Fixed = simulateHandShowdown(boardWithFix, normalPlayers, false, true);
    printf("Hand 3 (normal, WITH fix, second hand): ");
    for (unsigned id : h3Fixed) printf("%u ", id);
    printf("\n");
    bool fixWorks3 = (h3Fixed.size() == 1 && h3Fixed[0] == 1); // only P1 shows
    if (fixWorks3) { printf("  Fix works: only best hand shows (P1) on second hand\n"); }
    else { printf("  Fix FAIL: expected only P1 to show on second hand\n"); }

    // --- Hand 4: normal hand, WITH fix (third hand) ---
    std::vector<unsigned> h4Fixed = simulateHandShowdown(boardWithFix, normalPlayers, false, true);
    printf("Hand 4 (normal, WITH fix, third hand): ");
    for (unsigned id : h4Fixed) printf("%u ", id);
    printf("\n");
    bool fixWorks4 = (h4Fixed.size() == 1 && h4Fixed[0] == 1); // only P1 shows
    if (fixWorks4) { printf("  Fix works: only best hand shows (P1) on third hand\n"); }
    else { printf("  Fix FAIL: expected only P1 to show on third hand\n"); }

    printf("\n");

    // -------------------------------------------------------
    // Summary
    // -------------------------------------------------------
    int passed = 0;
    int failed = 0;

    if (h1BothShow) passed++; else failed++;
    if (bugConfirmed) passed++; else failed++;
    if (bugPersistent) passed++; else failed++;
    if (h1BothShowF) passed++; else failed++;
    if (fixWorks2) passed++; else failed++;
    if (fixWorks3) passed++; else failed++;
    if (fixWorks4) passed++; else failed++;

    printf("Results: Passed: %d, Failed: %d\n", passed, failed);
    printf("\n");

    if (!h1BothShow || !bugConfirmed || !bugPersistent || !h1BothShowF || !fixWorks2 || !fixWorks3 || !fixWorks4) {
        printf("TEST FAILED\n");
        return 1;
    }

    printf("TEST PASSED: allInCondition sticky-flag bug and fix verified across multiple hands\n");
    return 0;
}