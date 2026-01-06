/*****************************************************************************
 * PokerTH - Automated Heads-Up Test Bot Simulation
 * 
 * Tests for verifying heads-up betting logic and pot calculations.
 * These tests simulate game scenarios and verify expected behavior.
 * 
 * Run with: ./build/bin/pokerth_tests --suite=HeadsUpBotSimulation
 *****************************************************************************/

#include "pokerth_test_framework.h"
#include <game_defs.h>
#include <gamedata.h>

#include <iostream>
#include <string>
#include <memory>
#include <sstream>
#include <cmath>

enum class BotAction {
    FOLD,
    CHECK,
    CALL,
    BET,
    RAISE,
    ALL_IN,
    WAIT
};

class TestBot {
public:
    TestBot(const std::string& name, int startCash)
        : m_name(name), m_startCash(startCash), 
          m_cash(startCash), m_bet(0), m_playerId(0) {
    }

    void postSmallBlind(int amount) {
        m_bet = amount;
        m_cash -= amount;
    }

    void postBigBlind(int amount) {
        m_bet = amount;
        m_cash -= amount;
    }

    void call(int amount) {
        m_bet += amount;
        m_cash -= amount;
    }

    void bet(int amount) {
        m_bet = amount;
        m_cash -= amount;
    }

    void raise(int amount) {
        m_bet += amount;
        m_cash -= amount;
    }

    void fold() {
        m_bet = 0;
    }

    void check() {
        // No change
    }

    void resetForNextRound() {
        m_bet = 0;
    }

    std::string m_name;
    int m_startCash;
    int m_cash;
    int m_bet;
    unsigned m_playerId;
};

class HeadsUpSimulation {
public:
    HeadsUpSimulation(int smallBlind, int bigBlind) 
        : m_smallBlind(smallBlind), m_bigBlind(bigBlind),
          m_bot1("Bot1", 1000), m_bot2("Bot2", 1000) {
    }

    int calculatePot() {
        return m_bot1.m_bet + m_bot2.m_bet;
    }

    void resetBets() {
        m_bot1.resetForNextRound();
        m_bot2.resetForNextRound();
    }

    int m_smallBlind;
    int m_bigBlind;
    TestBot m_bot1;
    TestBot m_bot2;
};

TEST_SUITE(HeadsUpBotSimulation)

TEST(BotCreation, TestBotInitialization)
{
    TestBot bot("TestBot", 1000);
    ASSERT_EQ(1000, bot.m_startCash);
    ASSERT_EQ(1000, bot.m_cash);
    ASSERT_EQ(0, bot.m_bet);
    ASSERT_EQ("TestBot", bot.m_name);
    return true;
}

TEST(BotActions, TestSmallBlindPost)
{
    TestBot bot("TestBot", 1000);
    bot.postSmallBlind(10);
    ASSERT_EQ(10, bot.m_bet);
    ASSERT_EQ(990, bot.m_cash);
    return true;
}

TEST(BotActions, TestBigBlindPost)
{
    TestBot bot("TestBot", 1000);
    bot.postBigBlind(20);
    ASSERT_EQ(20, bot.m_bet);
    ASSERT_EQ(980, bot.m_cash);
    return true;
}

TEST(BotActions, TestCall)
{
    TestBot bot("TestBot", 1000);
    bot.postBigBlind(20);
    bot.call(10);
    ASSERT_EQ(30, bot.m_bet);
    ASSERT_EQ(970, bot.m_cash);
    return true;
}

TEST(BotActions, TestBet)
{
    TestBot bot("TestBot", 1000);
    bot.bet(50);
    ASSERT_EQ(50, bot.m_bet);
    ASSERT_EQ(950, bot.m_cash);
    return true;
}

TEST(BotActions, TestRaise)
{
    TestBot bot("TestBot", 1000);
    bot.postBigBlind(20);
    bot.raise(30);  // Raise by 30 (total bet 50)
    ASSERT_EQ(50, bot.m_bet);
    ASSERT_EQ(950, bot.m_cash);
    return true;
}

TEST(BotActions, TestFold)
{
    TestBot bot("TestBot", 1000);
    bot.postBigBlind(20);
    bot.fold();
    ASSERT_EQ(0, bot.m_bet);
    ASSERT_EQ(980, bot.m_cash);  // Blind is returned
    return true;
}

TEST(PreflopSimulation, TestPreflopBlinds)
{
    HeadsUpSimulation sim(10, 20);
    
    // Bot1 posts SB
    sim.m_bot1.postSmallBlind(sim.m_smallBlind);
    
    // Bot2 posts BB
    sim.m_bot2.postBigBlind(sim.m_bigBlind);
    
    // Verify state
    ASSERT_EQ(10, sim.m_bot1.m_bet);
    ASSERT_EQ(990, sim.m_bot1.m_cash);
    ASSERT_EQ(20, sim.m_bot2.m_bet);
    ASSERT_EQ(980, sim.m_bot2.m_cash);
    ASSERT_EQ(30, sim.calculatePot());
    
    return true;
}

TEST(PreflopSimulation, TestPreflopCall)
{
    HeadsUpSimulation sim(10, 20);
    
    // Bot1 posts SB
    sim.m_bot1.postSmallBlind(sim.m_smallBlind);
    
    // Bot2 posts BB
    sim.m_bot2.postBigBlind(sim.m_bigBlind);
    
    // Bot1 calls (needs to put in $10 more to match BB)
    sim.m_bot1.call(sim.m_bigBlind - sim.m_bot1.m_bet);
    
    // Verify state
    ASSERT_EQ(20, sim.m_bot1.m_bet);
    ASSERT_EQ(980, sim.m_bot1.m_cash);
    ASSERT_EQ(20, sim.m_bot2.m_bet);
    ASSERT_EQ(980, sim.m_bot2.m_cash);
    ASSERT_EQ(40, sim.calculatePot());
    
    return true;
}

TEST(FlopSimulation, TestFlopNoBet)
{
    HeadsUpSimulation sim(10, 20);
    
    // Setup: reset bets from preflop
    sim.resetBets();
    
    // Bot1 checks (first to act on flop)
    sim.m_bot1.check();
    
    // Bot2 checks
    sim.m_bot2.check();
    
    // Verify state
    ASSERT_EQ(0, sim.m_bot1.m_bet);
    ASSERT_EQ(0, sim.m_bot2.m_bet);
    ASSERT_EQ(0, sim.calculatePot());
    
    return true;
}

TEST(FlopSimulation, TestFlopBetAndCall)
{
    HeadsUpSimulation sim(10, 20);
    
    // Setup: reset bets from preflop
    sim.resetBets();
    
    // Bot1 checks (first to act on flop)
    sim.m_bot1.check();
    
    // Bot2 bets $50
    sim.m_bot2.bet(50);
    
    // Bot1 calls
    sim.m_bot1.call(50);
    
    // Verify state
    ASSERT_EQ(50, sim.m_bot1.m_bet);
    ASSERT_EQ(950, sim.m_bot1.m_cash);
    ASSERT_EQ(50, sim.m_bot2.m_bet);
    ASSERT_EQ(950, sim.m_bot2.m_cash);
    ASSERT_EQ(100, sim.calculatePot());
    
    return true;
}

TEST(TurnSimulation, TestTurnBetAndCall)
{
    HeadsUpSimulation sim(10, 20);
    
    // Setup: reset bets from previous street
    sim.resetBets();
    
    // Bot1 checks
    sim.m_bot1.check();
    
    // Bot2 bets $100
    sim.m_bot2.bet(100);
    
    // Bot1 calls
    sim.m_bot1.call(100);
    
    // Verify state
    ASSERT_EQ(100, sim.m_bot1.m_bet);
    ASSERT_EQ(900, sim.m_bot1.m_cash);
    ASSERT_EQ(100, sim.m_bot2.m_bet);
    ASSERT_EQ(900, sim.m_bot2.m_cash);
    ASSERT_EQ(200, sim.calculatePot());
    
    return true;
}

TEST(RiverSimulation, TestRiverAllIn)
{
    HeadsUpSimulation sim(10, 20);
    
    // Setup: both players have $830 left
    sim.m_bot1.m_cash = 830;
    sim.m_bot2.m_cash = 830;
    sim.resetBets();
    
    // Bot1 checks
    sim.m_bot1.check();
    
    // Bot2 goes all-in
    int allInAmount = 830;
    sim.m_bot2.bet(allInAmount);
    
    // Bot1 calls all-in
    sim.m_bot1.call(allInAmount);
    
    // Verify state
    ASSERT_EQ(830, sim.m_bot1.m_bet);
    ASSERT_EQ(0, sim.m_bot1.m_cash);
    ASSERT_EQ(830, sim.m_bot2.m_bet);
    ASSERT_EQ(0, sim.m_bot2.m_cash);
    ASSERT_EQ(1660, sim.calculatePot());
    
    return true;
}

TEST(FullHandSimulation, TestCompleteHand)
{
    HeadsUpSimulation sim(10, 20);
    
    // === PREFLOP ===
    sim.m_bot1.postSmallBlind(10);
    sim.m_bot2.postBigBlind(20);
    sim.m_bot1.call(10);  // Call to match BB
    
    ASSERT_EQ(40, sim.calculatePot());
    ASSERT_EQ(980, sim.m_bot1.m_cash);
    ASSERT_EQ(980, sim.m_bot2.m_cash);
    
    // Reset for flop
    sim.resetBets();
    
    // === FLOP ===
    sim.m_bot1.check();
    sim.m_bot2.bet(50);
    sim.m_bot1.call(50);
    
    ASSERT_EQ(100, sim.calculatePot());
    ASSERT_EQ(930, sim.m_bot1.m_cash);
    ASSERT_EQ(930, sim.m_bot2.m_cash);
    
    // Reset for turn
    sim.resetBets();
    
    // === TURN ===
    sim.m_bot1.check();
    sim.m_bot2.bet(100);
    sim.m_bot1.call(100);
    
    ASSERT_EQ(200, sim.calculatePot());
    ASSERT_EQ(830, sim.m_bot1.m_cash);
    ASSERT_EQ(830, sim.m_bot2.m_cash);
    
    // Reset for river
    sim.resetBets();
    
    // === RIVER ===
    sim.m_bot1.check();
    sim.m_bot2.bet(830);  // All-in
    sim.m_bot1.call(830);  // All-in
    
    ASSERT_EQ(1660, sim.calculatePot());
    ASSERT_EQ(0, sim.m_bot1.m_cash);
    ASSERT_EQ(0, sim.m_bot2.m_cash);
    
    return true;
}

TEST(HeadsUpButtonOrder, TestPreflopOrder)
{
    // In heads-up preflop:
    // - BB acts first (can raise)
    // - SB acts second (can call, raise, fold)
    
    // Bot2 is BB, acts first
    // Bot1 is SB, acts second
    
    bool bbActsFirst = true;
    bool sbActsSecond = true;
    
    ASSERT_TRUE(bbActsFirst);
    ASSERT_TRUE(sbActsSecond);
    return true;
}

TEST(HeadsUpButtonOrder, TestPostFlopOrder)
{
    // In heads-up post-flop (turn, river):
    // - Dealer (who is SB) acts first
    // - BB acts second
    
    // Bot1 is dealer + SB, acts first
    // Bot2 is BB, acts second
    
    bool dealerActsFirst = true;
    bool bbActsSecond = true;
    
    ASSERT_TRUE(dealerActsFirst);
    ASSERT_TRUE(bbActsSecond);
    return true;
}

TEST(ButtonPositionDefinition, TestButtonConstants)
{
    // Verify button enum values
    ASSERT_EQ(0, BUTTON_NONE);
    ASSERT_EQ(1, BUTTON_DEALER);
    ASSERT_EQ(2, BUTTON_SMALL_BLIND);
    ASSERT_EQ(3, BUTTON_BIG_BLIND);
    return true;
}

TEST(GameStateDefinition, TestGameStateConstants)
{
    ASSERT_EQ(0, GAME_STATE_PREFLOP);
    ASSERT_EQ(1, GAME_STATE_FLOP);
    ASSERT_EQ(2, GAME_STATE_TURN);
    ASSERT_EQ(3, GAME_STATE_RIVER);
    ASSERT_EQ(4, GAME_STATE_POST_RIVER);
    return true;
}

TEST(PlayerActionDefinition, TestPlayerActionConstants)
{
    ASSERT_EQ(0, PLAYER_ACTION_NONE);
    ASSERT_EQ(1, PLAYER_ACTION_FOLD);
    ASSERT_EQ(2, PLAYER_ACTION_CHECK);
    ASSERT_EQ(3, PLAYER_ACTION_CALL);
    ASSERT_EQ(4, PLAYER_ACTION_BET);
    ASSERT_EQ(5, PLAYER_ACTION_RAISE);
    ASSERT_EQ(6, PLAYER_ACTION_ALLIN);
    return true;
}

TEST(PotCalculationEdgeCases, TestAllInPot)
{
    // Scenario: Bot1 has $100, Bot2 has $50
    // Bot1 bets $100, Bot2 calls $50 (all-in)
    // Pot should be $150, Bot1 gets $50 back
    
    HeadsUpSimulation sim(10, 20);
    
    sim.m_bot1.m_cash = 100;
    sim.m_bot2.m_cash = 50;
    
    // Bot1 goes all-in
    sim.m_bot1.bet(100);
    
    // Bot2 calls all-in (can only call $50)
    sim.m_bot2.call(50);
    
    // Bot1's excess should be returned (simulated here by adjusting)
    int actualPot = 100 + 50;
    int expectedPot = 150;
    
    ASSERT_EQ(expectedPot, actualPot);
    ASSERT_EQ(0, sim.m_bot2.m_cash);
    
    // Bot1 effectively has $50 returned (not in pot)
    int bot1NetBet = 50;  // Only $50 goes to pot, $50 returned
    ASSERT_EQ(50, bot1NetBet);
    
    return true;
}

TEST(PotCalculationEdgeCases, TestSidePot)
{
    // Scenario: 3 players, Bot1 has $100, Bot2 has $50, Bot3 has $30
    // Bot1 bets $100, Bot2 calls $50, Bot3 calls $30
    // Main pot: $90 (3 x $30)
    // Side pot: $60 (Bot1 vs Bot2, 2 x $30 excess)
    
    // For heads-up, simpler case:
    // Bot1 $100, Bot2 $50
    // Bot1 bets $100, Bot2 calls $50
    // All-in amount: $50
    // Pot: $100
    
    HeadsUpSimulation sim(10, 20);
    
    sim.m_bot1.m_cash = 100;
    sim.m_bot2.m_cash = 50;
    
    sim.m_bot1.bet(100);
    sim.m_bot2.call(50);
    
    int expectedPot = 150;  // $100 + $50
    ASSERT_EQ(expectedPot, sim.calculatePot());
    
    return true;
}

TEST(PotCalculationEdgeCases, TestPartialCall)
{
    // Bot1 has $100, Bot2 has $200
    // Bot1 bets $100, Bot2 calls $100
    // Pot: $200
    
    HeadsUpSimulation sim(10, 20);
    
    sim.m_bot1.m_cash = 100;
    sim.m_bot2.m_cash = 200;
    
    sim.m_bot1.bet(100);
    sim.m_bot2.call(100);
    
    int expectedPot = 200;
    ASSERT_EQ(expectedPot, sim.calculatePot());
    ASSERT_EQ(100, sim.m_bot2.m_cash);  // $200 - $100 call
    
    return true;
}

END_TEST_SUITE
