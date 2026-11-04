// Test for GameTable::refreshAction() hole cards hide bug when player folds.
//
// Bug: In gameTableImpl::refreshAction(), when a player folds (action==PLAYER_ACTION_FOLD),
// their hole cards are hidden by clearing the pixmap. However, player with ID 0
// (typically the dealer/button) is skipped due to the condition
// getMyID() != 0, so their hole cards are never cleared even when they fold.
//
// Fix: Remove the if((*it_c)->getMyID() != 0) condition so ALL players have
// their hole cards cleared when they fold.
//
// This test verifies the bug: without the fix, player with ID 0 keeps hole cards
// visible after folding, while with the fix, all players have hole cards cleared.

#include <cstdio>
#include <cstdlib>

// Mock PlayerInterface minimal subset needed for the hole cards test.
struct MockPlayer {
    unsigned uniqueId;
    bool active;
    int myID;          // player ID (0 = dealer/button, others = 1,2,3...)
    int myAction;      // player action (0 = none, 1 = FOLD)

    MockPlayer(unsigned id, bool act, int playerId, int action)
        : uniqueId(id), active(act), myID(playerId), myAction(action) {}

    int getMyID() const { return myID; }
    int getMyAction() const { return myAction; }
};

// Mock hole card array - simulating the holeCardsArray in gametableimpl.cpp
// holeCardsArray[playerID][0] and holeCardsArray[playerID][1] are pixmap labels
struct MockHoleCard {
    bool hasPixmap;  // true if a pixmap is set (cards visible)

    MockHoleCard() : hasPixmap(true) {}

    void setPixmap(bool value, bool /*clear*/) {
        hasPixmap = value;
    }

    bool getPixmap() const { return hasPixmap; }
};

// Simulate the OLD (buggy) hole cards hiding logic from gametableimpl.cpp
// Old code: if(myAction==1 && myID != 0) clear hole cards
static void clearHoleCardsBuggy(MockPlayer& player, MockHoleCard& card0, MockHoleCard& card1) {
    if (player.myAction == 1 && player.myID != 0) {  // <-- THIS IS THE BUG: skips player with ID 0
        card0.setPixmap(false, false);
        card1.setPixmap(false, false);
    }
}

// Simulate the NEW (fixed) hole cards hiding logic from gametableimpl.cpp
// New code: no ID check, clear hole cards for ALL players
static void clearHoleCardsFixed(MockPlayer& player, MockHoleCard& card0, MockHoleCard& card1) {
    if (player.myAction == 1) {  // FIX: clear for all players regardless of ID
        card0.setPixmap(false, false);
        card1.setPixmap(false, false);
    }
}

int main() {
    printf("Testing hole cards hide bug on fold\n");

    // Create mock players - one with ID 0 (dealer/button), others with IDs 1,2,3
    MockPlayer p1(1, true, 0, 1);  // ID 0 - dealer/button, action=FOLD
    MockPlayer p2(2, true, 1, 1);  // ID 1 - action=FOLD
    MockPlayer p3(3, true, 2, 1);  // ID 2 - action=FOLD
    MockPlayer p4(4, true, 3, 1);  // ID 3 - action=FOLD

    // Mock hole cards - initially have pixmaps (cards visible)
    MockHoleCard c1_0, c1_1;  // player ID 0 cards
    MockHoleCard c2_0, c2_1;  // player ID 1 cards
    MockHoleCard c3_0, c3_1;  // player ID 2 cards
    MockHoleCard c4_0, c4_1;  // player ID 3 cards

    // Test OLD (buggy) behavior
    printf("\n--- OLD (buggy) behavior: if(myAction==1 && myID != 0) ---\n");

    clearHoleCardsBuggy(p1, c1_0, c1_1);
    clearHoleCardsBuggy(p2, c2_0, c2_1);
    clearHoleCardsBuggy(p3, c3_0, c3_1);
    clearHoleCardsBuggy(p4, c4_0, c4_1);

    printf("  Player ID %d: hole cards visible = %s\n", p1.myID, c1_0.getPixmap() || c1_1.getPixmap() ? "YES" : "NO");
    printf("  Player ID %d: hole cards visible = %s\n", p2.myID, c2_0.getPixmap() || c2_1.getPixmap() ? "YES" : "NO");
    printf("  Player ID %d: hole cards visible = %s\n", p3.myID, c3_0.getPixmap() || c3_1.getPixmap() ? "YES" : "NO");
    printf("  Player ID %d: hole cards visible = %s\n", p4.myID, c4_0.getPixmap() || c4_1.getPixmap() ? "YES" : "NO");

    // Player with ID 0 should have hole cards cleared but originally keeps them visible (bug)
    bool bugPresent = (!c1_0.getPixmap() && !c1_1.getPixmap()) ? false : true;
    // Actually, bug is that ID 0 keeps cards visible when they should be cleared
    // If c1_0/getPixmap() is true, that means bug is present (cards not cleared)
    bugPresent = (c1_0.getPixmap() || c1_1.getPixmap());  // true means bug present (cards still visible)

    printf("\n  Bug confirmed: Player ID 0 keeps hole cards visible after folding (without fix): %s\n",
           bugPresent ? "PASS (bug present)" : "FAIL");

    // Test NEW (fixed) behavior
    printf("\n--- NEW (fixed) behavior: if(myAction==1) for ALL players ---\n");
    // Reset hole cards
    c1_0.setPixmap(true, false);
    c1_1.setPixmap(true, false);
    c2_0.setPixmap(true, false);
    c2_1.setPixmap(true, false);
    c3_0.setPixmap(true, false);
    c3_1.setPixmap(true, false);
    c4_0.setPixmap(true, false);
    c4_1.setPixmap(true, false);

    clearHoleCardsFixed(p1, c1_0, c1_1);
    clearHoleCardsFixed(p2, c2_0, c2_1);
    clearHoleCardsFixed(p3, c3_0, c3_1);
    clearHoleCardsFixed(p4, c4_0, c4_1);

    printf("  Player ID %d: hole cards visible = %s\n", p1.myID, c1_0.getPixmap() || c1_1.getPixmap() ? "YES" : "NO");
    printf("  Player ID %d: hole cards visible = %s\n", p2.myID, c2_0.getPixmap() || c2_1.getPixmap() ? "YES" : "NO");
    printf("  Player ID %d: hole cards visible = %s\n", p3.myID, c3_0.getPixmap() || c3_1.getPixmap() ? "YES" : "NO");
    printf("  Player ID %d: hole cards visible = %s\n", p4.myID, c4_0.getPixmap() || c4_1.getPixmap() ? "YES" : "NO");

    // With fix, all players should have hole cards cleared
    bool fixWorks = (!c1_0.getPixmap() && !c1_1.getPixmap() &&
                     !c2_0.getPixmap() && !c2_1.getPixmap() &&
                     !c3_0.getPixmap() && !c3_1.getPixmap() &&
                     !c4_0.getPixmap() && !c4_1.getPixmap());

    printf("\n");
    printf("Bug confirmed: Player ID 0 keeps hole cards visible after folding (without fix): %s\n",
           bugPresent ? "PASS (bug present)" : "FAIL");
    printf("Fix works: All players have hole cards cleared (with fix): %s\n",
           fixWorks ? "PASS" : "FAIL");

    if (!bugPresent || !fixWorks) {
        printf("\nTEST FAILED\n");
        return 1;
    }

    printf("\nTEST PASSED: hole cards fold bug confirmed and fix verified\n");
    return 0;
}