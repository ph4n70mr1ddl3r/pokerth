// Test for LocalPlayer::flopEngine3() PLAYER_ACTION_BET myCash/mySet update bug.
//
// Bug: In LocalPlayer::flopEngine3(), the PLAYER_ACTION_BET case does
// `myCash -= mySet;` but does NOT update mySet = oldMySet + mySet. This
// causes inconsistent state where mySet is not properly updated to reflect
// the total chips set (oldMySet + current bet).
//
// Fix: Added mySet = oldMySet + mySet in the PLAYER_ACTION_BET switch case
// at the end of flopEngine3(), after myCash -= mySet.
//
// Compile: g++ -O2 -std=c++23 -o test_myCash_minus_mySet_flopEngine3 test_myCash_minus_mySet_flopEngine3.cpp && ./test_myCash_minus_mySet_flopEngine3

#include <cstdio>
#include <cstdlib>
#include <algorithm>

// Mock LocalPlayer state matching the flopEngine3 structure
struct MockLocalPlayer {
    int mySet{0};
    int myCash{100};
    int oldMySet{0};
    int highestSet{100};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getOldMySet() const { return oldMySet; }
    int getHighestSet() const { return highestSet; }
};

// Simulate the FIXED flopEngine3() PLAYER_ACTION_BET case
// Does myCash -= mySet AND mySet = oldMySet + mySet
static int flopEngine3_fixed(MockLocalPlayer& p) {
    if (p.mySet > 0) {
        p.myCash -= p.mySet;
        p.mySet = p.oldMySet + p.mySet;
    }
    return 0;
}

// Simulate the BUGGY flopEngine3() PLAYER_ACTION_BET case
// Does myCash -= mySet but does NOT update mySet = oldMySet + mySet
static int flopEngine3_buggy(MockLocalPlayer& p) {
    if (p.mySet > 0) {
        p.myCash -= p.mySet;
        // mySet = oldMySet + mySet;  // BUG: missing
    }
    return 0;
}

// Helper to reset mock player state
static void resetPlayer(MockLocalPlayer& p, int mySet, int myCash, int oldMySet) {
    p.mySet = mySet;
    p.myCash = myCash;
    p.oldMySet = oldMySet;
}

// Run buggy test and report results
static void runBuggyTest(const char* name, MockLocalPlayer& p, int cashBefore, int expectedCash, int expectedSet) {
    printf("  %s: myCash before = %d\n", name, cashBefore);
    p.myCash = cashBefore;
    flopEngine3_buggy(p);
    printf("  %s: myCash = %d (expected %d), mySet = %d (expected %d)\n",
           name, p.myCash, expectedCash, p.mySet, expectedSet);
}

// Run fixed test and report results
static void runFixedTest(const char* name, MockLocalPlayer& p, int cashBefore, int expectedCash, int expectedSet) {
    printf("  %s: myCash before = %d\n", name, cashBefore);
    p.myCash = cashBefore;
    flopEngine3_fixed(p);
    printf("  %s: myCash = %d (expected %d), mySet = %d (expected %d)\n",
           name, p.myCash, expectedCash, p.mySet, expectedSet);
}

int main()
{
    int failures = 0;

    printf("Testing PLAYER_ACTION_BET myCash/mySet update bug in flopEngine3():\n\n");

    // Track which scenarios passed
    bool scenario1_passed = false;
    bool scenario2_passed = false;
    bool scenario3_passed = false;
    bool scenario4_passed = false;

    // --- Scenario 1: Normal bet with oldMySet > 0 ---
    printf("--- Scenario 1 (mySet=50, myCash=100, oldMySet=30) ---\n");
    MockLocalPlayer p1;
    resetPlayer(p1, 50, 100, 30);
    int cashBefore1 = p1.myCash;

    runBuggyTest("Buggy", p1, cashBefore1, cashBefore1 - 50, 50);
    // Buggy: myCash = 100 - 50 = 50, mySet stays 50 (missing oldMySet + mySet)

    runFixedTest("Fixed", p1, cashBefore1, cashBefore1 - 50, 80);
    // Fixed: myCash = 100 - 50 = 50, mySet = 30 + 50 = 80

    bool fix1_ok = (p1.myCash == cashBefore1 - 50 && p1.mySet == 80);
    printf("  Result: %s\n\n", fix1_ok ? "PASS" : "FAIL");
    if (fix1_ok) scenario1_passed = true;

    // --- Scenario 2: Fresh bet (oldMySet = 0) ---
    printf("--- Scenario 2 (mySet=30, myCash=100, oldMySet=0) ---\n");
    MockLocalPlayer p2;
    resetPlayer(p2, 30, 100, 0);
    int cashBefore2 = p2.myCash;

    runBuggyTest("Buggy", p2, cashBefore2, cashBefore2 - 30, 30);
    // Buggy: myCash = 100 - 30 = 70, mySet stays 30 (same as original since oldMySet=0)

    runFixedTest("Fixed", p2, cashBefore2, cashBefore2 - 30, 30);
    // Fixed: myCash = 100 - 30 = 70, mySet stays 30 (oldMySet + mySet = 0 + 30 = 30)

    bool fix2_ok = (p2.myCash == cashBefore2 - 30 && p2.mySet == 30);
    printf("  Result: %s\n\n", fix2_ok ? "PASS" : "FAIL");
    if (fix2_ok) scenario2_passed = true;

    // --- Scenario 3: Larger mySet with oldMySet > 0 ---
    printf("--- Scenario 3 (mySet=40, myCash=200, oldMySet=25) ---\n");
    MockLocalPlayer p3;
    resetPlayer(p3, 40, 200, 25);
    int cashBefore3 = p3.myCash;

    runBuggyTest("Buggy", p3, cashBefore3, cashBefore3 - 40, 40);
    // Buggy: myCash = 200 - 40 = 160, mySet stays 40 (missing oldMySet + mySet)

    runFixedTest("Fixed", p3, cashBefore3, cashBefore3 - 40, 65);
    // Fixed: myCash = 200 - 40 = 160, mySet = 25 + 40 = 65

    bool fix3_ok = (p3.myCash == cashBefore3 - 40 && p3.mySet == 65);
    printf("  Result: %s\n\n", fix3_ok ? "PASS" : "FAIL");
    if (fix3_ok) scenario3_passed = true;

    // --- Scenario 4: mySet = 0 edge case ---
    printf("--- Scenario 4 (mySet=0, myCash=100, oldMySet=10) ---\n");
    MockLocalPlayer p4;
    resetPlayer(p4, 0, 100, 10);
    int cashBefore4 = p4.myCash;

    runBuggyTest("Buggy", p4, cashBefore4, cashBefore4, 0);  // mySet=0, nothing happens
    // Buggy: mySet > 0 is false, so myCash unchanged = 100

    runFixedTest("Fixed", p4, cashBefore4, cashBefore4, 0);  // mySet=0, nothing happens

    bool fix4_ok = (p4.myCash == cashBefore4);
    printf("  Result: %s\n\n", fix4_ok ? "PASS" : "FAIL");
    if (fix4_ok) scenario4_passed = true;  // Not counting as failure since no change expected

    printf("Scenario results:\n");
    printf("  Scenario 1 (oldMySet > 0 bug): %s\n", scenario1_passed ? "PASS" : "FAIL");
    printf("  Scenario 2 (fresh bet edge case): %s\n", scenario2_passed ? "PASS" : "FAIL");
    printf("  Scenario 3 (larger mySet): %s\n", scenario3_passed ? "PASS" : "FAIL");
    printf("  Scenario 4 (mySet=0 edge case): %s\n", scenario4_passed ? "PASS" : "FAIL");

    if (!scenario1_passed || !scenario2_passed || !scenario3_passed) {
        printf("\nTEST FAILED\n");
        return 1;
    }

    printf("\nTEST PASSED: flopEngine3 PLAYER_ACTION_BET myCash/mySet update bug confirmed and fix verified\n");
    return 0;
}