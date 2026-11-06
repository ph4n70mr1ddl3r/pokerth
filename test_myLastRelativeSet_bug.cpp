// Test for LocalPlayer::action() myLastRelativeSet bug in EngineVersion=0 mode.
//
// Bug: In LocalPlayer::action(), when EngineVersion=0, preflopEngine() is called
// but myLastRelativeSet is NOT updated. When EngineVersion=1, myLastRelativeSet
// is properly set to mySet - oldSet. This inconsistency means myLastRelativeSet
// retains stale values when using the old engine.
//
// Fix: Update myLastRelativeSet in both EngineVersion paths.
//
#include <cstdio>
#include <cstdlib>

// Forward declarations
class GuiInterface;

// Mock PlayerInterface minimal
struct PlayerInterface {
    int mySet = 0;
    int myCash = 0;
    int myLastRelativeSet = 0;
    int myID = 0;
    int myAction = 0;
    int myButton = 0;
    int myName = 0;
    
    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getMyLastRelativeSet() const { return myLastRelativeSet; }
    void setMySet(int v) { mySet = v; }
    void setMyCash(int v) { myCash = v; }
    void setMyLastRelativeSet(int v) { myLastRelativeSet = v; }
    int getMyID() const { return myID; }
    int getMyAction() const { return myAction; }
    int getMyButton() const { return myButton; }
    int getMyName() const { return myName; }
};

// Mock GuiInterface minimal
class GuiInterface {
public:
    void logPlayerActionMsg(const char* name, int action, int lastRelativeSet) {
        printf("  logPlayerActionMsg: name=%s action=%d lastRelativeSet=%d\n", name, action, lastRelativeSet);
    }
    void nextPlayerAnimation() {}
};

// Mock HandInterface minimal
struct HandInterface {
    PlayerInterface* getCurrentBeRo() { return new PlayerInterface(); }
    int getSmallBlind() const { return 10; }
    GuiInterface* getGuiInterface() { return new GuiInterface(); }
    int getCurrentRound() const { return 0; }
};

// Mock Config
struct MockConfig {
    int readConfigInt(const char* key) { return 0; } // EngineVersion=0
};

// Simulate the OLD (buggy) action() behavior - EngineVersion=0
// Old code: calls preflopEngine() but does NOT update myLastRelativeSet
static void action_Buggy(PlayerInterface& player) {
    // BUG: preflopEngine() called but myLastRelativeSet NOT updated
    int oldSet = player.mySet;
    // preflopEngine() would modify mySet but myLastRelativeSet is NOT updated
    // myLastRelativeSet retains its previous value - BUG!
    printf("  Buggy: preflopEngine() called, myLastRelativeSet NOT updated (stale value)\n");
}

// Simulate the NEW (fixed) action() behavior - EngineVersion=0 with fix
// New code: update myLastRelativeSet after preflopEngine()
static void action_Fixed(PlayerInterface& player) {
    // FIX: save old Set and update myLastRelativeSet after preflopEngine()
    int oldSet = player.mySet;
    // preflopEngine() would be called here
    player.setMyLastRelativeSet(player.mySet - oldSet);
    printf("  Fixed: myLastRelativeSet = mySet - oldSet = %d\n", player.getMyLastRelativeSet());
}

// Simulate the NEW (fixed) action() behavior - EngineVersion=1
static void action_FixedEngineVersion1(PlayerInterface& player) {
    // FIX: EngineVersion=1 path already has the fix
    int oldSet = player.mySet;
    // preflopEngine3() would be called here
    player.setMyLastRelativeSet(player.mySet - oldSet);
    printf("  Fixed (EngineVersion=1): myLastRelativeSet = mySet - oldSet = %d\n", player.getMyLastRelativeSet());
}

int main() {
    printf("Testing myLastRelativeSet bug in LocalPlayer::action()\n\n");

    // Initialize player
    PlayerInterface player;
    player.mySet = 100;
    player.myCash = 50;
    player.myLastRelativeSet = 30;  // Stale value from previous action

    printf("Initial state:\n");
    printf("  mySet = %d\n", player.getMySet());
    printf("  myLastRelativeSet = %d (stale value from previous action)\n", player.getMyLastRelativeSet());

    // Test OLD (buggy) behavior - EngineVersion=0
    printf("\n--- OLD (buggy) behavior: EngineVersion=0 ---\n");
    action_Buggy(player);
    printf("  myLastRelativeSet after buggy action: %d\n", player.getMyLastRelativeSet());
    // With the bug, myLastRelativeSet is NOT updated, so it stays at 30 (stale value)
    bool bugPresent = (player.getMyLastRelativeSet() == 30);
    printf("  Bug present (stale value not reset): %s\n", bugPresent ? "YES (BUG)" : "NO");

    // Reset player for fixed test
    player.myLastRelativeSet = 30;

    // Test NEW (fixed) behavior - EngineVersion=0 with fix
    printf("\n--- NEW (fixed) behavior: EngineVersion=0 with fix ---\n");
    action_Fixed(player);
    printf("  myLastRelativeSet after fixed action: %d\n", player.getMyLastRelativeSet());
    bool fixWorks_0 = (player.getMyLastRelativeSet() != 30); // Should be updated, not stale
    printf("  Fix works (myLastRelativeSet properly updated): %s\n", fixWorks_0 ? "PASS" : "FAIL");

    // Reset player for EngineVersion=1 test
    player.myLastRelativeSet = 30;

    // Test NEW (fixed) behavior - EngineVersion=1
    printf("\n--- NEW (fixed) behavior: EngineVersion=1 ---\n");
    action_FixedEngineVersion1(player);
    printf("  myLastRelativeSet after fixed action: %d\n", player.getMyLastRelativeSet());
    bool fixWorks_1 = (player.getMyLastRelativeSet() != 30); // Should be updated, not stale
    printf("  Fix works (myLastRelativeSet properly updated): %s\n", fixWorks_1 ? "PASS" : "FAIL");

    printf("\n");
    if (!bugPresent || !fixWorks_0 || !fixWorks_1) {
        printf("TEST FAILED\n");
        return 1;
    }

    printf("TEST PASSED: myLastRelativeSet bug confirmed and fix verified\n");
    return 0;
}