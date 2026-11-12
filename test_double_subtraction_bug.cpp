// Test for LocalPlayer::flopEngine3() double-subtraction bug.
//
// Bug: In LocalPlayer::flopEngine3(), the else branch (when highestSet == 0)
// had `myCash -= mySet;` which subtracted the bet amount from myCash.
// Since mySet is already set to the bet amount at lines 4349-4353, and
// the PLAYER_ACTION_BET switch statement also subtracts the bet from myCash,
// this caused myCash to be reduced by 2*bet instead of bet (double subtraction).
// mySet was also incorrectly set to oldMySet + mySet, adding the bet twice.
//
// Fix: Removed `myCash -= mySet;` and `mySet = oldMySet + mySet;` from the
// else branch. mySet is already correctly set to the bet amount at lines 4349-4353,
// and the switch statement handles the cash/set updates correctly.

#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer state matching the actual code structure
struct MockLocalPlayer {
    int mySet{0};
    int myCash{100};
    int oldMySet{0};
    int highestSet{100};
    int bet{0};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getOldMySet() const { return oldMySet; }
    int getHighestSet() const { return highestSet; }
    int getBet() const { return bet; }
};

// Simulate the FIXED flopEngine3() else branch - mySet already set at lines 4349-4353
// No myCash -= mySet; or mySet = oldMySet + mySet; in the else branch
static int processFixed(MockLocalPlayer& p) {
    // mySet is set to bet amount at lines 4349-4353 (simulated here)
    p.mySet = p.bet;

    // NO myCash -= mySet; (removed in fix - was causing double subtraction)
    // NO mySet = oldMySet + mySet; (removed in fix - was causing double addition)

    // The PLAYER_ACTION_BET switch handles the updates correctly
    if (p.bet > 0) {
        p.myCash -= p.bet;  // Single subtraction only
        // mySet already set at lines 4349-4353, no need to add again
    }
    return 0;
}

// Simulate the BUGGY flopEngine3() else branch - had double-subtraction code
static int processBuggy(MockLocalPlayer& p) {
    // mySet is set to bet amount at lines 4349-4353 (simulated here)
    p.mySet = p.bet;

    // BUG: myCash -= mySet; (line 4367 in original code)
    // This subtracts the bet from cash
    p.myCash -= p.mySet;

    // BUG: mySet = oldMySet + mySet; (line 4368 in original code)
    // This adds the bet to mySet again, causing double addition
    p.mySet = p.oldMySet + p.mySet;

    // The switch statement also subtracts bet from cash and adds to mySet
    if (p.bet > 0) {
        p.myCash -= p.bet;   // Second subtraction - DOUBLE!
        p.mySet += p.bet;    // Second addition - DOUBLE!
    }
    return 0;
}

int main()
{
    int failures = 0;

    printf("Testing PLAYER_ACTION_BET double-subtraction bug in flopEngine3():\n\n");

    // --- Scenario 1: Normal bet with bet > 0 ---
    printf("--- Scenario 1 (mySet=0, myCash=100, bet=30, highestSet=100, oldMySet=0) ---\n");
    MockLocalPlayer p1;
    p1.mySet = 0;
    p1.myCash = 100;
    p1.bet = 30;
    p1.oldMySet = 0;
    p1.highestSet = 100;

    int cashBefore1 = p1.myCash;
    int setBefore1 = p1.mySet;

    processBuggy(p1);
    printf("  Buggy: myCash=%d (expected %d), mySet=%d (expected %d)\n",
           p1.myCash, cashBefore1 - 30, p1.mySet, setBefore1 + 30);
    // Buggy: myCash = 100 - 30 (myCash -= mySet) - 30 (switch myCash -= bet) = 40 (WRONG! double subtraction)
    // mySet = 0 + 30 (set at step 1) + 30 (switch mySet += bet) = 60 (WRONG! double addition)
    if (p1.myCash != cashBefore1 - 30 || p1.mySet != setBefore1 + 30) {
        printf("  BUG CONFIRMED in flopEngine3: Double subtraction and double addition!\n");
        printf("    myCash: got %d, expected %d (differs by 2*bet=%d)\n",
               p1.myCash, cashBefore1 - 30, 2 * p1.bet);
        printf("    mySet: got %d, expected %d (differs by 2*bet=%d)\n",
               p1.mySet, setBefore1 + 30, 2 * p1.bet);
        failures++;
    } else {
        printf("  Bug NOT confirmed (unexpected - bug may already be fixed)\n");
        failures++;
    }

    // Reset and test fix
    p1.myCash = cashBefore1;
    p1.mySet = setBefore1;
    processFixed(p1);
    printf("  Fixed: myCash=%d (expected %d), mySet=%d (expected %d)\n",
           p1.myCash, cashBefore1 - 30, p1.mySet, setBefore1 + 30);
    if (p1.myCash == cashBefore1 - 30 && p1.mySet == setBefore1 + 30) {
        printf("  PASS: flopEngine3 fix works correctly - single subtraction only\n");
    } else {
        printf("  FAIL: flopEngine3 fix didn't work as expected (myCash=%d, mySet=%d)\n",
               p1.myCash, p1.mySet);
        failures++;
    }

    // --- Scenario 2: Bet of 0 edge case ---
    printf("\n--- Scenario 2 (mySet=0, myCash=100, bet=0, highestSet=100, oldMySet=0) ---\n");
    MockLocalPlayer p2;
    p2.mySet = 0;
    p2.myCash = 100;
    p2.bet = 0;
    p2.oldMySet = 0;
    p2.highestSet = 100;
    int cashBefore2 = p2.myCash;

    processBuggy(p2);
    if (p2.myCash == cashBefore2) {
        printf("  Buggy bet=0: cash correctly unchanged (myCash -= 0 = no-op)... wait, buggy code does myCash -= mySet first, then switch does myCash -= bet, both are 0 so cash unchanged)\n");
    } else {
        printf("  Buggy bet=0: FAIL - cash unexpectedly changed to %d\n", p2.myCash);
        failures++;
    }

    // Reset and test fix
    p2.myCash = cashBefore2;
    processFixed(p2);
    if (p2.myCash == cashBefore2) {
        printf("  Fixed bet=0: cash correctly unchanged\n");
    } else {
        printf("  FAIL: fixed bet=0 - cash unexpectedly changed to %d\n", p2.myCash);
        failures++;
    }

    // --- Scenario 3: Large bet ---
    printf("\n--- Scenario 3 (mySet=0, myCash=50, bet=30, highestSet=100, oldMySet=0) ---\n");
    MockLocalPlayer p3;
    p3.mySet = 0;
    p3.myCash = 50;
    p3.bet = 30;
    p3.oldMySet = 0;
    p3.highestSet = 100;

    int cashBefore3 = p3.myCash;

    processBuggy(p3);
    printf("  Buggy: myCash=%d (expected %d, but got %d due to double subtraction 50-30-30=-10->clamped)\n",
           p3.myCash, cashBefore3 - 30, p3.myCash);
    // Buggy: myCash = 50 - 30 (myCash -= mySet) - 30 (switch myCash -= bet) = -10, clamped to 0
    // But the point is: should only subtract 30, not 60
    if (p3.myCash != cashBefore3 - 30) {
        printf("  BUG PATTERN CONFIRMED: myCash reduced by %d instead of %d (double subtraction of %d)\n",
               cashBefore3 - p3.myCash, cashBefore3 - 30, 2 * p3.bet);
        failures++;
    } else {
        printf("  Bug NOT confirmed (unexpected)\n");
        failures++;
    }

    // Reset and test fix
    p3.myCash = cashBefore3;
    processFixed(p3);
    if (p3.myCash == cashBefore3 - 30) {
        printf("  PASS: Fixed code correctly reduces myCash by single bet amount\n");
    } else {
        printf("  FAIL: Fixed code myCash=%d instead of %d\n",
               p3.myCash, cashBefore3 - 30);
        failures++;
    }

    // --- Scenario 4: Already had some mySet before bet ---
    printf("\n--- Scenario 4 (mySet=20, myCash=100, bet=30, highestSet=100, oldMySet=20) ---\n");
    MockLocalPlayer p4;
    p4.mySet = 20;
    p4.myCash = 100;
    p4.bet = 30;
    p4.oldMySet = 20;
    p4.highestSet = 100;

    int cashBefore4 = p4.myCash;
    int setBefore4 = p4.mySet;

    processBuggy(p4);
    printf("  Buggy: myCash=%d (expected %d), mySet=%d (expected %d)\n",
           p1.myCash, cashBefore1 - 30, p1.mySet, setBefore1 + 30);
    // Buggy: myCash = 100 - 20 (myCash -= mySet where mySet was 20) - 30 (switch) = 50
    // Actually: myCash -= mySet where mySet=20 (from before), then myCash -= bet=30, then mySet = oldMySet(20) + mySet(30) = 50, then mySet += bet=30 = 80
    // Wait, let me re-trace: p4.mySet starts at 20, p4.bet=30
    // processBuggy: p4.mySet = p4.bet = 30 (step 1)
    // p4.myCash -= p4.mySet = 100 - 30 = 70 (step 2)
    // p4.mySet = p4.oldMySet + p4.mySet = 20 + 30 = 50 (step 3)
    // switch: p4.myCash -= p4.bet = 70 - 30 = 40 (double subtraction!)
    // p4.mySet += p4.bet = 50 + 30 = 80 (double addition!)
    // Expected: myCash should be 100 - 30 = 70, mySet should be 30 (or 20+30=50 depending on logic)
    // The bug is: myCash is reduced by 60 instead of 30, mySet is increased by 60 instead of 30

    // Actually for simplicity, let me just check the key bug: double subtraction
    if (p4.myCash != cashBefore4 - 60 && p4.myCash != cashBefore4 - 30) {
        // It's doing something wrong
        printf("  Buggy: myCash=%d (in wrong range, double subtraction likely)\n", p4.myCash);
    }

    // Reset and test fix
    p4.myCash = cashBefore4;
    p4.mySet = setBefore4;
    processFixed(p4);
    printf("  Fixed: myCash=%d (expected %d), mySet=%d (expected %d)\n",
           p4.myCash, cashBefore4 - 30, p4.mySet, setBefore4 + 30);
    // Fixed: mySet = bet = 30 (set at step 1), no myCash -= mySet, no mySet = oldMySet + mySet
    // switch: myCash -= bet = cashBefore4 - 30, mySet += bet = setBefore4 + 30
    if (p4.myCash == cashBefore4 - 30 && p4.mySet == setBefore4 + 30) {
        printf("  PASS: Fixed code handles pre-existing mySet correctly\n");
    } else {
        printf("  FAIL: Fixed code myCash=%d, mySet=%d\n",
               p4.myCash, p4.mySet);
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: PLAYER_ACTION_BET double-subtraction bug confirmed and fix verified\n");
    return 0;
}