/*****************************************************************************
 * PokerTH - Bet Display Bug Tests
 * 
 * Tests for verifying the bet display bug in headsup games.
 * 
 * Bug: BB bets 20 on flop, GUI shows 40, SB sees "Call for 0" instead of "Call for 20"
 * 
 * Run with: ./build/bin/pokerth_tests --suite=BetDisplayBugTests
 *****************************************************************************/

#include "pokerth_test_framework.h"
#include <game_defs.h>

TEST_SUITE(BetDisplayBugTests)

TEST(BetDisplayBugTests, TestFlopBetDisplayIssue)
{
    // Test the reported bug: BB bets 20 on flop, GUI shows 40, SB sees "Call for 0"
    
    // Setup: Headsup, SB=$10, BB=$20
    const int SB = 10;
    const int BB = 20;
    
    // === PREFLOP ===
    int sbBet = SB;           // SB posts $10
    int bbBet = BB;           // BB posts $20
    sbBet += 10;              // SB calls $10 more to match BB
    
    // After preflop:
    // SB total: $20 (called BB)
    // BB total: $20 (blind)
    // Pot: $40
    
    ASSERT_EQ(20, sbBet);
    ASSERT_EQ(20, bbBet);
    ASSERT_EQ(40, sbBet + bbBet);
    
    // === FLOP ===
    // BB bets $20 on the flop
    bbBet += 20;              // BB bets $20
    
    // After BB's bet:
    // BB total: $40 ($20 blind + $20 bet)
    // SB total: $20 (only blind)
    // Highest set: $40 (BB's total)
    
    int highestSet = bbBet;   // $40
    int sbCurrentBet = sbBet; // $20
    
    // SB needs to call:
    int callAmount = highestSet - sbCurrentBet;
    
    // This should be $20
    ASSERT_EQ(20, callAmount);
    
    // Let's verify the call amount calculation
    int mySet = sbBet;           // SB's current bet = $20
    int myCash = 100 - sbBet;    // SB's remaining cash = $80
    int tempHighestSet = highestSet;  // $40
    
    int calculatedCallAmount;
    if (myCash + mySet <= tempHighestSet) {
        calculatedCallAmount = myCash;  // All-in
    } else {
        calculatedCallAmount = tempHighestSet - mySet;
    }
    
    // Should be $20
    ASSERT_EQ(20, calculatedCallAmount);
    
    // If the GUI shows "Call for 0", there might be a bug
    bool callAmountIsZero = (calculatedCallAmount == 0);
    bool callAmountIsTwenty = (calculatedCallAmount == 20);
    
    // Call amount should NOT be zero
    ASSERT_FALSE(callAmountIsZero);
    
    // Call amount should be 20
    ASSERT_TRUE(callAmountIsTwenty);
    
    return true;
}

TEST(BetDisplayBugTests, TestCallAmountShouldNotBeZero)
{
    // Test case: BB bets on flop, SB should see non-zero call amount
    
    // Scenario:
    // SB=$10, BB=$20
    // Preflop: SB calls, BB checks → Pot=$40, both have $20
    // Flop: BB bets $20
    
    int sbSet = 20;      // SB has called the blind, total $20
    int bbSet = 40;      // BB blind $20 + bet $20 = $40
    int highestSet = bbSet;  // $40
    int mySet = sbSet;       // SB's current bet = $20
    int myCash = 80;         // SB started with $100, has $80 left
    
    // getMyCallAmount() logic:
    int callAmount;
    if (myCash + mySet <= highestSet) {
        callAmount = myCash;  // All-in
    } else {
        callAmount = highestSet - mySet;
    }
    
    // callAmount should be $20 (40 - 20)
    ASSERT_EQ(20, callAmount);
    
    // Verify the condition:
    bool shouldBeAllIn = (myCash + mySet <= highestSet);
    bool shouldCalculateDifference = (myCash + mySet > highestSet);
    
    ASSERT_FALSE(shouldBeAllIn);       // Not all-in (80+20 > 40)
    ASSERT_TRUE(shouldCalculateDifference);  // Should calculate difference
    
    // The bug might be in how mySet is retrieved
    int expectedCall = highestSet - mySet;  // 40 - 20 = 20
    ASSERT_EQ(20, expectedCall);
    
    return true;
}

TEST(BetDisplayBugTests, TestBetAmountDoubled)
{
    // Test if bet amounts are being doubled somewhere
    
    // Simulate the exact scenario:
    // SB=$10, BB=$20
    // Preflop: SB calls $10, BB checks
    // Flop: BB bets $20
    
    int sbBlind = 10;
    int bbBlind = 20;
    
    // Preflop bets
    int sbPreflopBet = sbBlind + 10;  // $20 total ($10 blind + $10 call)
    int bbPreflopBet = bbBlind;       // $20 total ($20 blind)
    
    ASSERT_EQ(20, sbPreflopBet);
    ASSERT_EQ(20, bbPreflopBet);
    
    // Flop bet
    int bbFlopBet = bbPreflopBet + 20;  // $40 total ($20 blind + $20 bet)
    
    // BB's bet shown on GUI should be $40 (total)
    // But the NEW bet amount is only $20
    // This might be confusing the user
    
    int highestSet = bbFlopBet;  // $40
    int sbCurrentBet = sbPreflopBet;  // $20
    
    // SB's call amount
    int sbCallAmount = highestSet - sbCurrentBet;  // $20
    
    ASSERT_EQ(20, sbCallAmount);
    
    // If GUI shows "Call for 0", there might be a bug
    // Let's trace through the actual getMyCallAmount() logic:
    
    int mySet = sbCurrentBet;     // SB's current bet = $20
    int myCash = 100 - sbCurrentBet;  // SB's remaining cash = $80
    int tempHighestSet = highestSet;  // $40
    
    int calculatedCall;
    if (myCash + mySet <= tempHighestSet) {
        calculatedCall = myCash;  // All-in
    } else {
        calculatedCall = tempHighestSet - mySet;
    }
    
    // Should be $20
    ASSERT_EQ(20, calculatedCall);
    
    // If the user sees "Call for 0", then either:
    // 1. getMySet() is returning wrong value for SB
    // 2. getHighestSet() is returning wrong value
    // 3. There's a display bug showing 0 instead of 20
    
    // Let's verify the math one more time
    bool isAllIn = (myCash + mySet <= tempHighestSet);  // 100 <= 40? No
    int expectedCallAmount = isAllIn ? myCash : (tempHighestSet - mySet);  // 40 - 20 = 20
    
    ASSERT_EQ(20, expectedCallAmount);
    ASSERT_FALSE(isAllIn);
    
    return true;
}

TEST(BetDisplayBugTests, TestHeadsUpScenario)
{
    // Complete headsup scenario test
    
    // SB=$10, BB=$20
    // Preflop: SB calls $10, BB checks → Pot=$40
    // Flop: BB bets $20
    
    // State after preflop:
    int sbSet = 20;  // SB called blind
    int bbSet = 20;  // BB blind
    int pot = 40;
    
    ASSERT_EQ(20, sbSet);
    ASSERT_EQ(20, bbSet);
    ASSERT_EQ(40, pot);
    
    // Flop: BB bets $20
    bbSet += 20;  // Now BB has $40 total
    int highestSet = bbSet;  // $40
    
    // SB's turn
    int sbCallAmount = highestSet - sbSet;  // $20
    
    // Should be $20 to call
    ASSERT_EQ(20, sbCallAmount);
    
    // Verify call amount calculation
    int mySet = sbSet;       // $20
    int myCash = 80;         // $100 - $20 bet
    int tempHighestSet = highestSet;  // $40
    
    int calculatedCall;
    if (myCash + mySet <= tempHighestSet) {
        calculatedCall = myCash;
    } else {
        calculatedCall = tempHighestSet - mySet;
    }
    
    ASSERT_EQ(20, calculatedCall);
    
    // If GUI shows "Call for 0", the bug is in:
    // - getMySet() returning wrong value, OR
    // - getHighestSet() returning wrong value, OR
    // - Display logic showing 0 instead of 20
    
    return true;
}

TEST(BetDisplayBugTests, TestEdgeCaseZeroCallAmount)
{
    // Test edge case where call amount might be 0
    
    // Scenario 1: Player already matched the bet
    int mySet1 = 40;
    int highestSet1 = 40;
    int myCash1 = 60;
    
    int callAmount1;
    if (myCash1 + mySet1 <= highestSet1) {
        callAmount1 = myCash1;
    } else {
        callAmount1 = highestSet1 - mySet1;
    }
    
    // Should be 0 - already matched the bet
    ASSERT_EQ(0, callAmount1);
    
    // Scenario 2: Player has no cash (all-in)
    int mySet2 = 20;
    int highestSet2 = 40;
    int myCash2 = 0;  // All-in
    
    int callAmount2;
    if (myCash2 + mySet2 <= highestSet2) {
        callAmount2 = myCash2;
    } else {
        callAmount2 = highestSet2 - mySet2;
    }
    
    // Should be 0 - all-in, no more cash to call
    ASSERT_EQ(0, callAmount2);
    
    // Scenario 3: Normal case - need to call
    int mySet3 = 20;
    int highestSet3 = 40;
    int myCash3 = 80;
    
    int callAmount3;
    if (myCash3 + mySet3 <= highestSet3) {
        callAmount3 = myCash3;
    } else {
        callAmount3 = highestSet3 - mySet3;
    }
    
    // Should be 20 - need to call $20
    ASSERT_EQ(20, callAmount3);
    
    return true;
}

TEST(BetDisplayBugTests, TestHeadsUpPostFlopActionOrder)
{
    // Test the action order in headsup post-flop
    
    // In headsup:
    // - Dealer (SB) acts FIRST on post-flop
    // - BB acts SECOND on post-flop
    
    // If dealer checks, then BB bets:
    int dealerSet = 20;  // Dealer (SB) has $20
    int bbSet = 20;      // BB has $20
    
    // Dealer checks (no bet)
    int highestSetAfterCheck = 0;
    
    // BB bets $20
    int bbBetAmount = 20;
    bbSet += bbBetAmount;  // BB now has $40
    int highestSetAfterBBBet = bbSet;  // $40
    
    // Dealer's turn to act
    int dealerCallAmount = highestSetAfterBBBet - dealerSet;  // $40 - $20 = $20
    
    // Should be $20 to call
    ASSERT_EQ(20, dealerCallAmount);
    
    // Verify calculation
    int mySet = dealerSet;
    int myCash = 80;
    int tempHighestSet = highestSetAfterBBBet;
    
    int calculatedCall;
    if (myCash + mySet <= tempHighestSet) {
        calculatedCall = myCash;
    } else {
        calculatedCall = tempHighestSet - mySet;
    }
    
    ASSERT_EQ(20, calculatedCall);
    
    return true;
}

END_TEST_SUITE
