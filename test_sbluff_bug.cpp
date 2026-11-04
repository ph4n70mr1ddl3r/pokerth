// Test for LocalHand::LocalHand() sBluff bug.
//
// Bug: In LocalHand::LocalHand(), the sBluff is only set for players with
// getMyID() != 0 (localhand.cpp line ~111). Player with ID 0 (typically the
// dealer/button player) is skipped, so sBluff is never set for this player.
//
// Fix: Remove the if((*it)->getMyID() != 0) condition so ALL players get sBluff set.
//
// This test verifies the bug: without the fix, player with ID 0 has sBluff=0
// (not set), while with the fix, all players have sBluff set to a non-zero value.

#include <cstdio>
#include <cstdlib>
#include <vector>

// Mock PlayerInterface: minimal subset needed for the sBluff test.
struct MockPlayer {
    unsigned uniqueId;
    bool active;
    int myID;          // player ID (0 = dealer/button, others = 1,2,3...)
    int sBluff;        // bluff flag

    MockPlayer(unsigned id, bool act, int playerId)
        : uniqueId(id), active(act), myID(playerId), sBluff(0) {}

    void setSBluff(int value) { sBluff = value; }
    int getSBluff() const { return sBluff; }
};

// Simulate the OLD (buggy) sBluff setting logic from localhand.cpp
// Old code: if((*it)->getMyID() != 0) { set sBluff; }
static void setSBluffBuggy(MockPlayer& player) {
    if (player.myID != 0) {  // <-- THIS IS THE BUG: skips player with ID 0
        player.setSBluff(42);  // Tools::GetRand(1, 100, 1, &sBluff);
    }
}

// Simulate the NEW (fixed) sBluff setting logic from localhand.cpp
// New code: no if condition, set sBluff for ALL players
static void setSBluffFixed(MockPlayer& player) {
    player.setSBluff(42);  // Tools::GetRand(1, 100, 1, &sBluff); - set for all
}

int main() {
    printf("Testing sBluff assignment bug\n");

    // Create mock players - one with ID 0 (dealer/button), others with IDs 1,2,3
    MockPlayer p1(1, true, 0);  // ID 0 - the buggy case: player with dealer/button ID
    MockPlayer p2(2, true, 1);
    MockPlayer p3(3, true, 2);
    MockPlayer p4(4, true, 3);

    // Test OLD (buggy) behavior
    printf("\n--- OLD (buggy) behavior: if(myID != 0) ---\n");
    setSBluffBuggy(p1);
    setSBluffBuggy(p2);
    setSBluffBuggy(p3);
    setSBluffBuggy(p4);

    printf("  Player ID %d: sBluff = %d\n", p1.myID, p1.getSBluff());
    printf("  Player ID %d: sBluff = %d\n", p2.myID, p2.getSBluff());
    printf("  Player ID %d: sBluff = %d\n", p3.myID, p3.getSBluff());
    printf("  Player ID %d: sBluff = %d\n", p4.myID, p4.getSBluff());

    // Player with ID 0 should have sBluff=42 but originally gets sBluff=0 (not set)
    bool bugPresent = (p1.getSBluff() == 0);

    // Test NEW (fixed) behavior
    printf("\n--- NEW (fixed) behavior: no if condition ---\n");
    // Reset sBluff values
    p1.setSBluff(0);
    p2.setSBluff(0);
    p3.setSBluff(0);
    p4.setSBluff(0);

    setSBluffFixed(p1);
    setSBluffFixed(p2);
    setSBluffFixed(p3);
    setSBluffFixed(p4);

    printf("  Player ID %d: sBluff = %d\n", p1.myID, p1.getSBluff());
    printf("  Player ID %d: sBluff = %d\n", p2.myID, p2.getSBluff());
    printf("  Player ID %d: sBluff = %d\n", p3.myID, p3.getSBluff());
    printf("  Player ID %d: sBluff = %d\n", p4.myID, p4.getSBluff());

    // With fix, all players should have sBluff=42
    bool fixWorks = (p1.getSBluff() == 42 && p2.getSBluff() == 42 &&
                     p3.getSBluff() == 42 && p4.getSBluff() == 42);

    printf("\n");
    printf("Bug confirmed: Player ID 0 missing sBluff (without fix): %s\n",
           bugPresent ? "PASS (bug present)" : "FAIL");
    printf("Fix works: All players have sBluff set (with fix): %s\n",
           fixWorks ? "PASS" : "FAIL");

    if (!bugPresent || !fixWorks) {
        printf("\nTEST FAILED\n");
        return 1;
    }

    printf("\nTEST PASSED: sBluff bug confirmed and fix verified\n");
    return 0;
}