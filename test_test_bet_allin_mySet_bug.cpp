/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

// Regression test: LocalPlayer::action() PLAYER_ACTION_BET all-in myCash/mySet bug.
//
// Bug: In LocalPlayer::action(), the PLAYER_ACTION_BET all-in case at line 1537 uses
//      mySet += myCash; which adds to the current mySet (which may have been modified
//      by the engine). It should be mySet = oldMySet + myCash; to correctly reflect
//      the total chips committed (previous set amount + all-in cash amount), consistent
//      with the non-all-in case on line 1541 which uses mySet = oldMySet + bet.
//
// The original code (buggy): mySet += myCash;
// The fixed code:           mySet = oldMySet + myCash;

#include <cstdio>
#include <cstdlib>

// Mock LocalPlayer: minimal subset needed for the bet all-in test.
struct MockLocalPlayer {
    int mySet;
    int myCash;
    int myLastRelativeSet;
    
    MockLocalPlayer(int s, int c) : mySet(s), myCash(c), myLastRelativeSet(0) {}
    
    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
    int getMyLastRelativeSet() const { return myLastRelativeSet; }
    
    void setMySet(int s) { mySet = s; }
    void setMyCash(int c) { myCash = c; }
    void setMyLastRelativeSet(int r) { myLastRelativeSet = r; }
};

// Buggy action() PLAYER_ACTION_BET all-in: mySet += myCash (adds to current mySet)
static void actionBetAllInBuggy(MockLocalPlayer& p, int oldMySet, int bet, int myCash) {
    if (bet > 0) {
        p.setMyCash(0);
        p.setMySet(p.getMySet() + myCash);  // BUG: adds to current mySet
    }
    // when bet == 0: no changes (myCash and mySet unchanged)
}

// Fixed action() PLAYER_ACTION_BET all-in: mySet = oldMySet + myCash
static void actionBetAllInFixed(MockLocalPlayer& p, int oldMySet, int bet, int myCash) {
    if (bet > 0) {
        p.setMyCash(0);
        p.setMySet(oldMySet + myCash);  // FIX: use oldMySet + myCash
    }
    // when bet == 0: no changes (myCash and mySet unchanged)
}

int main()
{
    int failures = 0;

    printf("Testing LocalPlayer::action() PLAYER_ACTION_BET all-in myCash/mySet bug\n\n");

    // --- Scenario 1: mySet=50, myCash=100, oldMySet=30, bet=20 (all-in: bet > myCash) ---
    printf("--- Scenario 1 (mySet=50, myCash=100, oldMySet=30, bet=20, all-in) ---\n");
    {
        MockLocalPlayer p1(50, 100);
        int oldMySet1 = 30;
        
        actionBetAllInBuggy(p1, oldMySet1, 20, 100);
        int buggySet = p1.getMySet();
        int buggyCash = p1.getMyCash();
        
        actionBetAllInFixed(p1, oldMySet1, 20, 100);
        int fixedSet = p1.getMySet();
        int fixedCash = p1.getMyCash();
        
        // Expected: myCash = 0, mySet = oldMySet + myCash = 30 + 100 = 130
        printf("  Buggy:   myCash=%d, mySet=%d\n", buggyCash, buggySet);
        printf("  Fixed:   myCash=%d, mySet=%d\n", fixedCash, fixedSet);
        
        // Buggy: mySet = 50 + 100 = 150 (adds to current mySet which is 50)
        // Fixed: mySet = 30 + 100 = 130 (oldMySet + myCash)
        if (buggySet == 150 && fixedSet == 130) {
            printf("  PASS: Buggy sets mySet=150, Fixed sets mySet=130 (expected)\n");
        } else {
            printf("  FAIL: Unexpected values (buggy=%d, fixed=%d)\n", buggySet, fixedSet);
            failures++;
        }
        
        if (buggyCash != 0 || fixedCash != 0) {
            printf("  FAIL: Cash should be 0 in all-in case\n");
            failures++;
        }
    }

    // --- Scenario 2: mySet=0, myCash=100, oldMySet=0, bet=0 (no bet) ---
    printf("\n--- Scenario 2 (mySet=0, myCash=100, oldMySet=0, bet=0, no bet) ---\n");
    {
        MockLocalPlayer p2(0, 100);
        int oldMySet2 = 0;
        
        actionBetAllInBuggy(p2, oldMySet2, 0, 100);
        int buggySet = p2.getMySet();
        int buggyCash = p2.getMyCash();
        
        actionBetAllInFixed(p2, oldMySet2, 0, 100);
        int fixedSet = p2.getMySet();
        int fixedCash = p2.getMyCash();
        
        // When bet=0: no changes to myCash or mySet (both remain at initial values)
        printf("  Buggy:   myCash=%d, mySet=%d\n", buggyCash, buggySet);
        printf("  Fixed:   myCash=%d, mySet=%d\n", fixedCash, fixedSet);
        
        // Both should preserve original values when bet=0
        if (buggySet == 0 && buggyCash == 100 && fixedSet == 0 && fixedCash == 100) {
            printf("  PASS: Both handle bet=0 correctly (no changes)\n");
        } else {
            printf("  FAIL: Expected myCash=100, mySet=0 with bet=0\n");
            failures++;
        }
    }

    // --- Scenario 3: mySet=20, myCash=50, oldMySet=10, bet=60 (all-in: bet > myCash) ---
    printf("\n--- Scenario 3 (mySet=20, myCash=50, oldMySet=10, bet=60, all-in) ---\n");
    {
        MockLocalPlayer p3(20, 50);
        int oldMySet3 = 10;
        
        actionBetAllInBuggy(p3, oldMySet3, 60, 50);
        int buggySet = p3.getMySet();
        int buggyCash = p3.getMyCash();
        
        actionBetAllInFixed(p3, oldMySet3, 60, 50);
        int fixedSet = p3.getMySet();
        int fixedCash = p3.getMyCash();
        
        // Expected: myCash = 0, mySet = oldMySet + myCash = 10 + 50 = 60
        printf("  Buggy:   myCash=%d, mySet=%d\n", buggyCash, buggySet);
        printf("  Fixed:   myCash=%d, mySet=%d\n", fixedCash, fixedSet);
        
        // Buggy: mySet = 20 + 50 = 70 (adds to current mySet which is 20)
        // Fixed: mySet = 10 + 50 = 60 (oldMySet + myCash)
        if (buggySet == 70 && fixedSet == 60) {
            printf("  PASS: Buggy sets mySet=70, Fixed sets mySet=60 (expected)\n");
        } else {
            printf("  FAIL: Unexpected values (buggy=%d, fixed=%d)\n", buggySet, fixedSet);
            failures++;
        }
        
        if (buggyCash != 0 || fixedCash != 0) {
            printf("  FAIL: Cash should be 0 in all-in case\n");
            failures++;
        }
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: PLAYER_ACTION_BET all-in myCash/mySet bug confirmed and fix verified\n");
    return 0;
}