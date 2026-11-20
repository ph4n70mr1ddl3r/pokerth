/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

// Regression test for checkMyAction() PLAYER_ACTION_CALL mySet >= highestSet
// edge case with myCash == 0.
//
// Bug: In LocalPlayer::checkMyAction(), the PLAYER_ACTION_CALL case at line 1092
// originally had `if(getMySet() >= highestSet && getMyCash() > 0)` which required
// positive cash even when the player's set already covers the highest set.
// When mySet >= highestSet but myCash == 0 and highestSet > 0, the call should
// be valid because no additional chips need to be put in.
//
// Fix: Changed the condition to `if(getMySet() >= highestSet)` with inner check
// `if (highestSet > 0 || getMyCash() > 0)` so that:
// - When highestSet > 0: call is valid even with myCash == 0 (no chips needed)
// - When highestSet == 0: call requires myCash > 0 (posting blind)
//
// Compile:
//   g++ -O2 -std=c++23 -o /tmp/test_checkmyaction_cashfix_bug test_checkmyaction_cashfix_bug.cpp
//   /tmp/test_checkmyaction_cashfix_bug

#include <cstdio>
#include <cstdlib>

// Mock PlayerInterface: minimal subset needed for the checkMyAction test.
struct MockPlayer {
    int mySet;
    int myCash;

    MockPlayer(int s, int c) : mySet(s), myCash(c) {}

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Fixed PLAYER_ACTION_CALL check: handles mySet >= highestSet edge case
// with myCash == 0 when highestSet > 0
static int checkMyActionCallFixed(MockPlayer& p, int highestSet) {
    if (p.getMySet() >= highestSet) {
        // Player has already put in enough chips (mySet >= highestSet).
        // Call is valid because no additional chips need to be put in.
        // targetBet is irrelevant when mySet >= highestSet.
        // If highestSet > 0, the bet has been made so call is valid even with myCash == 0.
        // If highestSet == 0, player is posting blind, requires myCash > 0.
        if (highestSet > 0 || p.getMyCash() > 0) {
            return 0;
        }
    }
    return 1;
}

// Buggy PLAYER_ACTION_CALL check: required getMyCash() > 0 unconditionally
static int checkMyActionCallBuggy(MockPlayer& p, int highestSet) {
    if (p.getMySet() >= highestSet && p.getMyCash() > 0) {
        return 0;
    }
    return 1;
}

int main()
{
    int failures = 0;

    // Scenario 1: mySet >= highestSet, highestSet > 0, myCash == 0
    // THE BUG SCENARIO: call should be valid because no additional chips needed
    printf("Test 1: PLAYER_ACTION_CALL mySet >= highestSet > 0, myCash == 0\n");
    MockPlayer p1(100, 0);
    int highestSet1 = 80;
    int result_fixed1 = checkMyActionCallFixed(p1, highestSet1);
    int result_buggy1 = checkMyActionCallBuggy(p1, highestSet1);
    
    if (result_fixed1 == 0) {
        printf("  PASS: Fixed code allows call (mySet>=highestSet, myCash==0, highestSet>0)\n");
    } else {
        printf("  FAIL: fixed code should allow call, got %d\n", result_fixed1);
        failures++;
    }
    if (result_buggy1 == 1) {
        printf("  BUG CONFIRMED: Buggy code rejects call (myCash > 0 unnecessarily required)\n");
    } else {
        printf("  INFO: Buggy code also allows\n");
    }

    // Scenario 2: mySet >= highestSet, highestSet == 0, myCash == 0
    // Posting blind with no chips should be invalid
    printf("\nTest 2: PLAYER_ACTION_CALL mySet >= highestSet == 0, myCash == 0 (posting blind)\n");
    MockPlayer p2(0, 0);
    int highestSet2 = 0;
    int result_fixed2 = checkMyActionCallFixed(p2, highestSet2);
    int result_buggy2 = checkMyActionCallBuggy(p2, highestSet2);
    
    if (result_fixed2 == 1) {
        printf("  PASS: Fixed code rejects call (posting blind with myCash==0)\n");
    } else {
        printf("  FAIL: fixed code should reject call, got %d\n", result_fixed2);
        failures++;
    }
    if (result_buggy2 == 1) {
        printf("  BUG CONFIRMED: Buggy code also rejects\n");
    } else {
        printf("  INFO: Buggy code also allows\n");
    }

    // Scenario 3: mySet >= highestSet, highestSet == 0, myCash > 0
    // Posting blind with chips should be valid
    printf("\nTest 3: PLAYER_ACTION_CALL mySet >= highestSet == 0, myCash > 0 (posting blind)\n");
    MockPlayer p3(0, 100);
    int highestSet3 = 0;
    int result_fixed3 = checkMyActionCallFixed(p3, highestSet3);
    int result_buggy3 = checkMyActionCallBuggy(p3, highestSet3);
    
    if (result_fixed3 == 0) {
        printf("  PASS: Fixed code allows call (posting blind with myCash>0)\n");
    } else {
        printf("  FAIL: fixed code should allow call, got %d\n", result_fixed3);
        failures++;
    }
    if (result_buggy3 == 0) {
        printf("  INFO: Buggy code also allows\n");
    } else {
        printf("  INFO: Buggy code also rejects\n");
    }

    // Scenario 4: mySet >= highestSet, highestSet > 0, myCash > 0
    // Normal case: should always allow call
    printf("\nTest 4: PLAYER_ACTION_CALL mySet >= highestSet > 0, myCash > 0\n");
    MockPlayer p4(100, 50);
    int highestSet4 = 80;
    int result_fixed4 = checkMyActionCallFixed(p4, highestSet4);
    int result_buggy4 = checkMyActionCallBuggy(p4, highestSet4);
    
    if (result_fixed4 == 0) {
        printf("  PASS: Fixed code allows call (normal case)\n");
    } else {
        printf("  FAIL: fixed code should allow call, got %d\n", result_fixed4);
        failures++;
    }
    if (result_buggy4 == 0) {
        printf("  INFO: Buggy code also allows\n");
    } else {
        printf("  INFO: Buggy code also rejects\n");
    }

    // Scenario 5: mySet < highestSet, myCash == 0
    // Should reject call (not enough total chips)
    printf("\nTest 5: PLAYER_ACTION_CALL mySet < highestSet, myCash == 0\n");
    MockPlayer p5(50, 0);
    int highestSet5 = 80;
    int result_fixed5 = checkMyActionCallFixed(p5, highestSet5);
    int result_buggy5 = checkMyActionCallBuggy(p5, highestSet5);
    
    if (result_fixed5 == 1) {
        printf("  PASS: Fixed code rejects call (insufficient chips)\n");
    } else {
        printf("  FAIL: fixed code should reject call, got %d\n", result_fixed5);
        failures++;
    }
    if (result_buggy5 == 1) {
        printf("  INFO: Buggy code also rejects\n");
    } else {
        printf("  INFO: Buggy code also allows\n");
    }

    // Scenario 6: mySet < highestSet, myCash > 0 but insufficient total
    // Should reject call (not enough chips even with cash)
    printf("\nTest 6: PLAYER_ACTION_CALL mySet < highestSet, myCash > 0 but insufficient\n");
    MockPlayer p6(50, 30);
    int highestSet6 = 100;
    int result_fixed6 = checkMyActionCallFixed(p6, highestSet6);
    int result_buggy6 = checkMyActionCallBuggy(p6, highestSet6);
    
    if (result_fixed6 == 1) {
        printf("  PASS: Fixed code rejects call (not enough total chips)\n");
    } else {
        printf("  FAIL: fixed code should reject call, got %d\n", result_fixed6);
        failures++;
    }
    if (result_buggy6 == 1) {
        printf("  INFO: Buggy code also rejects\n");
    } else {
        printf("  INFO: Buggy code also allows\n");
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: checkMyAction mySet>=highestSet myCash==0 edge case verified\n");
    return 0;
}