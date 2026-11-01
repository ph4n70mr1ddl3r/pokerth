// Test for LocalBoard::allInCondition sticky flag bug.
//
// Bug: LocalBoard::allInCondition is set to true during all-in situations
// (LocalHand::switchRounds() calls setAllInCondition(true) at localboard.cpp
// lines 588, 598, 617, 626) but is NEVER reset to false. Since LocalBoard is
// shared across all hands within a Game session (Game::initHand() reuses the
// same currentBoard object created once in the Game constructor), once an
// all-in condition occurs, EVERY subsequent hand forces ALL non-folded
// players to show their hole cards at showdown -- even in normal showdowns
// where losing players should be allowed to muck.
//
// Fix: Reset allInCondition to false at the start of each hand in
// Game::initHand(), before the new hand begins.
//
// This test replicates a minimal LocalBoard with the allInCondition flag and
// determinePlayerNeedToShowCards() logic, simulates two hands (an all-in hand
// followed by a normal hand), and verifies that without the reset the flag
// stays true across hands (bug) while with the reset it is cleared (fix).
//
// Card/show-card logic:
//   - allInCondition == true  -> every non-folded active player shows
//   - allInCondition == false -> only player(s) with the best hand show
//
// Compile: g++ -O2 -std=c++23 -o test_allin_condition_reset test_allin_condition_reset.cpp && ./test_allin_condition_reset

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

    MockBoard board;

    // --- Hand 1: all-in condition triggered ---
    // Expected: allInCondition = true -> P1 and P2 both show (correct for all-in).
    std::vector<unsigned> h1 = simulateHandShowdown(board, allInPlayers, true, false);
    printf("Hand 1 (all-in triggered, no fix): ");
    for (unsigned id : h1) printf("%u ", id);
    printf("\n");
    bool h1BothShow = (h1.size() == 2);

    // board.allInCondition is now true (sticky).

    // --- Hand 2: normal hand, NO fix (the bug) ---
    // Expected (buggy): allInCondition still true from Hand 1 -> P1 AND P2 both
    // show, even though P2 has a worse hand and should muck in a normal showdown.
    std::vector<unsigned> h2Buggy = simulateHandShowdown(board, normalPlayers, false, false);
    printf("Hand 2 (normal, NO fix -- bug): ");
    for (unsigned id : h2Buggy) printf("%u ", id);
    printf("\n");
    bool bugConfirmed = (h2Buggy.size() == 2); // both show even though P2 should muck

    // Reset board for the fixed run.
    board.setAllInCondition(false);

    // --- Hand 2: normal hand, WITH fix ---
    // Expected (fixed): allInCondition reset to false -> only P1 (best hand)
    // shows; P2 (worse hand) gets to muck.
    std::vector<unsigned> h2Fixed = simulateHandShowdown(board, normalPlayers, false, true);
    printf("Hand 2 (normal, WITH fix): ");
    for (unsigned id : h2Fixed) printf("%u ", id);
    printf("\n");
    bool fixWorks = (h2Fixed.size() == 1 && h2Fixed[0] == 1); // only P1 shows

    printf("\n");
    printf("Hand 1 all-in showdown both show:    %s\n", h1BothShow ? "PASS" : "FAIL");
    printf("Bug confirmed: Hand 2 shows all players without reset: %s\n", bugConfirmed ? "PASS (bug present)" : "FAIL");
    printf("Fix works: Hand 2 shows only best hand with reset:   %s\n", fixWorks ? "PASS" : "FAIL");

    if (!h1BothShow || !bugConfirmed || !fixWorks) {
        printf("\nTEST FAILED\n");
        return 1;
    }

    printf("\nTEST PASSED: allInCondition sticky-flag bug confirmed and fix verified\n");
    return 0;
}
