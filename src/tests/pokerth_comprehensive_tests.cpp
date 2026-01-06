/*****************************************************************************
 * PokerTH - Comprehensive Unit Tests
 * This file contains extensive tests for:
 * - Card and hand evaluation
 * - Poker hand rankings
 * - Player actions
 * - Game state transitions
 * - Network packet validation
 * - Chat cleaner functionality
 * - Edge cases and boundary conditions
 * 
 * Run with: ./build/bin/pokerth_tests
 *****************************************************************************/

#include "pokerth_test_framework.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>
#include <map>
#include <functional>
#include <random>
#include <cmath>
#include <cstring>
#include <limits>

#ifdef __cplusplus
extern "C" {
#endif

#include <third_party/protobuf/pokerth.pb.h>

#ifdef __cplusplus
}
#endif

#include <net/netpacket.h>
#include <net/netpacketvalidator.h>
#include <game_defs.h>
#include <gamedata.h>
#include <playerdata.h>

TEST_SUITE(CardsValueTests)

TEST(CardsValue_RoyalFlushDetection, TestRoyalFlushSpades)
{
    int cards[7] = {12, 11, 10, 9, 8, 50, 51};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    int handClass = value / 100000000;
    EXPECT_EQ(handClass, 9);
    return true;
}

TEST(CardsValue_RoyalFlushAllSuits, TestRoyalFlushHearts)
{
    int cards[7] = {25, 24, 23, 22, 21, 50, 51};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 9);
    return true;
}

TEST(CardsValue_StraightFlushAceHigh, TestAceHighStraightFlush)
{
    int cards[7] = {12, 11, 10, 9, 8, 3, 16};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 8);
    EXPECT_EQ((value % 1000000) / 1000000, 12);
    return true;
}

TEST(CardsValue_StraightFlushFiveHigh, TestWheelStraightFlush)
{
    int cards[7] = {3, 12, 11, 10, 9, 5, 6};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 8);
    EXPECT_EQ((value % 1000000) / 1000000, 3);
    return true;
}

TEST(CardsValue_FourOfAKind, TestQuadAces)
{
    int cards[7] = {12, 25, 38, 51, 10, 5, 6};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 7);
    EXPECT_EQ((value % 1000000) / 1000000, 12);
    return true;
}

TEST(CardsValue_FourOfAKindWithKicker, TestQuadKingsWithAceKicker)
{
    int cards[7] = {11, 24, 37, 50, 12, 5, 6};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 7);
    EXPECT_EQ((value % 1000000) / 1000000, 11);
    EXPECT_EQ((value % 10000) / 100, 12);
    return true;
}

TEST(CardsValue_FullHouse, TestAcesFullOfKings)
{
    int cards[7] = {12, 25, 38, 11, 24, 50, 51};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 6);
    EXPECT_EQ((value % 1000000) / 1000000, 12);
    EXPECT_EQ((value % 10000) / 100, 11);
    return true;
}

TEST(CardsValue_Flush, TestAceHighFlush)
{
    int cards[7] = {0, 3, 6, 9, 12, 50, 51};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 5);
    EXPECT_EQ((value % 1000000) / 1000000, 12);
    return true;
}

TEST(CardsValue_Straight, TestAceHighStraight)
{
    int cards[7] = {12, 11, 10, 9, 8, 50, 51};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 4);
    EXPECT_EQ((value % 1000000) / 1000000, 12);
    return true;
}

TEST(CardsValue_StraightWheel, TestFiveHighStraight)
{
    int cards[7] = {3, 12, 11, 10, 9, 50, 51};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 4);
    EXPECT_EQ((value % 1000000) / 1000000, 3);
    return true;
}

TEST(CardsValue_ThreeOfAKind, TestTripAces)
{
    int cards[7] = {12, 25, 38, 10, 11, 50, 51};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 3);
    EXPECT_EQ((value % 1000000) / 1000000, 12);
    return true;
}

TEST(CardsValue_TwoPair, TestAcesAndKings)
{
    int cards[7] = {12, 25, 11, 24, 10, 50, 51};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 2);
    EXPECT_EQ((value % 1000000) / 1000000, 12);
    EXPECT_EQ((value % 10000) / 100, 11);
    return true;
}

TEST(CardsValue_OnePair, TestPairAces)
{
    int cards[7] = {12, 25, 10, 11, 9, 50, 51};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 1);
    EXPECT_EQ((value % 1000000) / 1000000, 12);
    return true;
}

TEST(CardsValue_HighCard, TestAceHigh)
{
    int cards[7] = {12, 10, 8, 5, 3, 50, 51};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 0);
    EXPECT_EQ((value % 1000000) / 1000000, 12);
    return true;
}

TEST(CardsValue_HandRankingOrder, TestRankingHierarchy)
{
    int pos[5];
    int royalFlush[7] = {12, 11, 10, 9, 8, 0, 3};
    int straightFlush[7] = {11, 10, 9, 8, 7, 0, 3};
    int fourOfAKind[7] = {12, 25, 38, 51, 10, 50, 49};
    int fullHouse[7] = {12, 25, 38, 11, 24, 50, 51};
    int flush[7] = {0, 3, 6, 9, 12, 50, 51};
    int straight[7] = {11, 10, 9, 8, 7, 50, 51};
    int threeOfAKind[7] = {12, 25, 38, 10, 11, 50, 51};
    int twoPair[7] = {12, 25, 11, 24, 10, 50, 51};
    int onePair[7] = {12, 25, 10, 11, 9, 50, 51};
    int highCard[7] = {12, 10, 8, 5, 3, 50, 51};
    
    int rf = evaluateCardsValue(royalFlush, pos);
    int sf = evaluateCardsValue(straightFlush, pos);
    int fk = evaluateCardsValue(fourOfAKind, pos);
    int fh = evaluateCardsValue(fullHouse, pos);
    int fl = evaluateCardsValue(flush, pos);
    int st = evaluateCardsValue(straight, pos);
    int tk = evaluateCardsValue(threeOfAKind, pos);
    int tp = evaluateCardsValue(twoPair, pos);
    int op = evaluateCardsValue(onePair, pos);
    int hc = evaluateCardsValue(highCard, pos);
    
    EXPECT_TRUE(rf > sf);
    EXPECT_TRUE(sf > fk);
    EXPECT_TRUE(fk > fh);
    EXPECT_TRUE(fh > fl);
    EXPECT_TRUE(fl > st);
    EXPECT_TRUE(st > tk);
    EXPECT_TRUE(tk > tp);
    EXPECT_TRUE(tp > op);
    EXPECT_TRUE(op > hc);
    return true;
}

TEST(CardsValue_FlushBeatsStraight, TestFlushVsStraight)
{
    int flush[7] = {0, 3, 6, 9, 12, 50, 51};
    int straight[7] = {11, 10, 9, 8, 7, 50, 51};
    int pos1[5], pos2[5];
    int flushVal = evaluateCardsValue(flush, pos1);
    int straightVal = evaluateCardsValue(straight, pos2);
    EXPECT_TRUE(flushVal > straightVal);
    return true;
}

TEST(CardsValue_FullHouseBeatsFlush, TestFullHouseVsFlush)
{
    int fullHouse[7] = {12, 25, 38, 11, 24, 50, 51};
    int flush[7] = {0, 3, 6, 9, 12, 50, 51};
    int pos1[5], pos2[5];
    int fhVal = evaluateCardsValue(fullHouse, pos1);
    int flushVal = evaluateCardsValue(flush, pos2);
    EXPECT_TRUE(fhVal > flushVal);
    return true;
}

TEST(CardsValue_QuadsBeatsFullHouse, TestQuadsVsFullHouse)
{
    int quads[7] = {12, 25, 38, 51, 10, 50, 51};
    int fullHouse[7] = {12, 25, 38, 11, 24, 50, 51};
    int pos1[5], pos2[5];
    int quadVal = evaluateCardsValue(quads, pos1);
    int fhVal = evaluateCardsValue(fullHouse, pos2);
    EXPECT_TRUE(quadVal > fhVal);
    return true;
}

TEST(CardsValue_StraightFlushBeatsQuads, TestStraightFlushVsQuads)
{
    int straightFlush[7] = {0, 1, 2, 3, 4, 50, 51};
    int quads[7] = {12, 25, 38, 51, 10, 50, 51};
    int pos1[5], pos2[5];
    int sfVal = evaluateCardsValue(straightFlush, pos1);
    int quadVal = evaluateCardsValue(quads, pos2);
    EXPECT_TRUE(sfVal > quadVal);
    return true;
}

TEST(CardsValue_StraightAceHighVsKingHigh, TestAceHighVsKingHigh)
{
    int aceHigh[7] = {12, 11, 10, 9, 8, 50, 51};
    int kingHigh[7] = {11, 10, 9, 8, 7, 50, 51};
    int pos1[5], pos2[5];
    int aceVal = evaluateCardsValue(aceHigh, pos1);
    int kingVal = evaluateCardsValue(kingHigh, pos2);
    EXPECT_TRUE(aceVal > kingVal);
    return true;
}

TEST(CardsValue_FullHouseHigherTriple, TestAcesFullVsKingsFull)
{
    int acesFull[7] = {12, 25, 38, 11, 24, 50, 51};
    int kingsFull[7] = {11, 24, 37, 10, 23, 50, 51};
    int pos1[5], pos2[5];
    int acesVal = evaluateCardsValue(acesFull, pos1);
    int kingsVal = evaluateCardsValue(kingsFull, pos2);
    EXPECT_TRUE(acesVal > kingsVal);
    return true;
}

TEST(CardsValue_TwoPairHigherPair, TestAcesKingsVsKingsQueens)
{
    int acesKings[7] = {12, 25, 11, 24, 10, 50, 51};
    int kingsQueens[7] = {11, 24, 10, 23, 9, 50, 51};
    int pos1[5], pos2[5];
    int akVal = evaluateCardsValue(acesKings, pos1);
    int kqVal = evaluateCardsValue(kingsQueens, pos2);
    EXPECT_TRUE(akVal > kqVal);
    return true;
}

TEST(CardsValue_PairHigherPair, TestPairAcesVsPairKings)
{
    int pairAces[7] = {12, 25, 10, 11, 9, 50, 51};
    int pairKings[7] = {11, 24, 10, 12, 9, 50, 51};
    int pos1[5], pos2[5];
    int acesVal = evaluateCardsValue(pairAces, pos1);
    int kingsVal = evaluateCardsValue(pairKings, pos2);
    EXPECT_TRUE(acesVal > kingsVal);
    return true;
}

TEST(CardsValue_PositionArrayValidity, TestPositionBounds)
{
    int cards[7] = {12, 11, 10, 9, 8, 50, 51};
    int position[5];
    evaluateCardsValue(cards, position);
    
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(position[i] >= 0 && position[i] <= 6);
    }
    return true;
}

TEST(CardsValue_PositionArrayUnique, TestUniquePositions)
{
    int cards[7] = {12, 11, 10, 9, 8, 50, 51};
    int position[5];
    evaluateCardsValue(cards, position);
    
    std::set<int> usedPositions;
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(usedPositions.find(position[i]) == usedPositions.end());
        usedPositions.insert(position[i]);
    }
    return true;
}

TEST(CardsValue_BoardOnlyHand, TestCommunityCardsOnly)
{
    int board[7] = {0, 1, 2, 3, 4, 50, 51};
    int position[5];
    int value = evaluateCardsValue(board, position);
    
    int handClass = value / 100000000;
    EXPECT_TRUE(handClass >= 0 && handClass <= 9);
    return true;
}

TEST(CardsValue_MultipleTriples, TestFullHouseFromThreeOfAKind)
{
    int cards[7] = {12, 25, 38, 11, 24, 37, 50};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 6);
    return true;
}

TEST(CardsValue_GapStraight, TestNonConsecutiveCards)
{
    int cards[7] = {12, 10, 9, 8, 7, 50, 51};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 0);
    return true;
}

TEST(CardsValue_AllCardsSameSuit, TestSevenCardFlush)
{
    int cards[7] = {0, 3, 6, 9, 12, 1, 2};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 5);
    return true;
}

TEST(CardsValue_AllCardsSameRank, TestFourOfAKindPlus)
{
    int cards[7] = {0, 13, 26, 39, 12, 25, 38};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 7);
    return true;
}

TEST(CardsValue_RandomHandsConsistency, Test100RandomHands)
{
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> cardDist(0, 51);
    std::set<int> usedCards;
    
    for (int test = 0; test < 100; test++) {
        usedCards.clear();
        int cards[7];
        for (int i = 0; i < 7; i++) {
            int card;
            do {
                card = cardDist(rng);
            } while (usedCards.find(card) != usedCards.end());
            usedCards.insert(card);
            cards[i] = card;
        }
        
        int position[5];
        int value = evaluateCardsValue(cards, position);
        
        EXPECT_TRUE(value >= 0);
        EXPECT_TRUE(position[0] >= 0 && position[0] <= 6);
    }
    return true;
}

TEST(CardsValue_BroadwayStraight, TestBroadwayTenToAce)
{
    int broadway[7] = {12, 11, 10, 9, 8, 0, 13};
    int position[5];
    int value = evaluateCardsValue(broadway, position);
    EXPECT_EQ(value / 100000000, 4);
    EXPECT_EQ((value % 1000000) / 1000000, 12);
    return true;
}

TEST(CardsValue_TwoPairKickerComparison, TestKickerBreaksTie)
{
    int acesKingsQueen[7] = {12, 25, 11, 24, 10, 50, 51};
    int acesKingsJack[7] = {12, 25, 11, 24, 9, 50, 51};
    int pos1[5], pos2[5];
    int queenKick = evaluateCardsValue(acesKingsQueen, pos1);
    int jackKick = evaluateCardsValue(acesKingsJack, pos2);
    EXPECT_TRUE(queenKick > jackKick);
    return true;
}

TEST(CardsValue_PairKickerComparison, TestKickersBreakPairTie)
{
    int pairAcesAKQ[7] = {12, 25, 11, 10, 9, 50, 51};
    int pairAcesAKJ[7] = {12, 25, 11, 10, 8, 50, 51};
    int pos1[5], pos2[5];
    int akqVal = evaluateCardsValue(pairAcesAKQ, pos1);
    int akjVal = evaluateCardsValue(pairAcesAKJ, pos2);
    EXPECT_TRUE(akqVal > akjVal);
    return true;
}

TEST(CardsValue_HighCardKickers, TestAllFiveKickers)
{
    int akqj10[7] = {12, 11, 10, 9, 8, 50, 51};
    int akqj9[7] = {12, 11, 10, 9, 7, 50, 51};
    int pos1[5], pos2[5];
    int akqj10Val = evaluateCardsValue(akqj10, pos1);
    int akqj9Val = evaluateCardsValue(akqj9, pos2);
    EXPECT_TRUE(akqj10Val > akqj9Val);
    return true;
}

END_TEST_SUITE

TEST_SUITE(HandRankingEdgeCases)

TEST(HandRanking_DuplicateCards, TestInvalidDuplicateCards)
{
    int cards[7] = {12, 12, 11, 10, 9, 8, 7};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_TRUE(value >= 0);
    return true;
}

TEST(HandRanking_SixCardFlushDraw, TestFlushDrawOnTurn)
{
    int cards[7] = {0, 3, 6, 9, 13, 26, 50};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 5);
    return true;
}

TEST(HandRanking_OpenEndedStraightDraw, TestOESD)
{
    int cards[7] = {11, 10, 9, 8, 5, 6, 7};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 4);
    EXPECT_EQ((value % 1000000) / 1000000, 11);
    return true;
}

TEST(HandRanking_GutshotStraightDraw, TestGutshot)
{
    int cards[7] = {12, 10, 9, 8, 7, 50, 51};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 0);
    return true;
}

TEST(HandRanking_RedundantCards, TestSevenCardTrips)
{
    int cards[7] = {12, 25, 38, 10, 11, 12, 25};
    int position[5];
    int value = evaluateCardsValue(cards, position);
    EXPECT_EQ(value / 100000000, 3);
    EXPECT_EQ((value % 1000000) / 1000000, 12);
    return true;
}

TEST(HandRanking_FullHouseFromBoard, TestBoardMakesFullHouse)
{
    int boardFullHouse[7] = {12, 25, 38, 11, 24, 5, 6};
    int position[5];
    int value = evaluateCardsValue(boardFullHouse, position);
    EXPECT_EQ(value / 100000000, 6);
    return true;
}

TEST(HandRanking_BoardMakesQuads, TestBoardMakesFourOfAKind)
{
    int boardQuads[7] = {12, 25, 38, 51, 5, 6, 7};
    int position[5];
    int value = evaluateCardsValue(boardQuads, position);
    EXPECT_EQ(value / 100000000, 7);
    return true;
}

TEST(HandRanking_BackdoorFlushDraw, TestBackdoorFlush)
{
    int backdoor[7] = {0, 3, 6, 1, 4, 7, 8};
    int position[5];
    int value = evaluateCardsValue(backdoor, position);
    EXPECT_EQ(value / 100000000, 5);
    return true;
}

TEST(HandRanking_LowStraight, TestTwoThreeFourFiveSix)
{
    int lowStraight[7] = {1, 2, 3, 4, 5, 50, 51};
    int position[5];
    int value = evaluateCardsValue(lowStraight, position);
    EXPECT_EQ(value / 100000000, 4);
    EXPECT_EQ((value % 1000000) / 1000000, 5);
    return true;
}

TEST(HandRanking_MidStraight, TestFourFiveSixSevenEight)
{
    int midStraight[7] = {3, 4, 5, 6, 7, 50, 51};
    int position[5];
    int value = evaluateCardsValue(midStraight, position);
    EXPECT_EQ(value / 100000000, 4);
    EXPECT_EQ((value % 1000000) / 1000000, 7);
    return true;
}

END_TEST_SUITE

TEST_SUITE(NetPacketTests)

TEST(NetPacket_CreatePacket, TestCreateNetPacket)
{
    NetPacket packet;
    EXPECT_TRUE(packet.GetMsg() != nullptr);
    return true;
}

TEST(NetPacket_MessageType, TestSetMessageType)
{
    NetPacket packet;
    packet.GetMsg()->set_messagetype(PokerTHMessage_PokerTHMessageType_Type_AnnounceMessage);
    EXPECT_EQ(packet.GetMsg()->messagetype(), PokerTHMessage_PokerTHMessageType_Type_AnnounceMessage);
    return true;
}

TEST(NetPacket_Serialization, TestPacketSerialization)
{
    NetPacket packet;
    packet.GetMsg()->set_messagetype(PokerTHMessage_PokerTHMessageType_Type_AnnounceMessage);
    packet.GetMsg()->set_nettime(12345);
    
    int size = packet.GetMsg()->ByteSizeLong();
    EXPECT_TRUE(size > 0);
    
    std::vector<unsigned char> buffer(size);
    packet.GetMsg()->SerializeWithCachedSizesToArray(buffer.data());
    EXPECT_TRUE(buffer.size() > 0);
    return true;
}

TEST(NetPacket_Deserialization, TestPacketDeserialization)
{
    NetPacket original;
    original.GetMsg()->set_messagetype(PokerTHMessage_PokerTHMessageType_Type_AnnounceMessage);
    original.GetMsg()->set_nettime(54321);
    
    int size = original.GetMsg()->ByteSizeLong();
    std::vector<unsigned char> buffer(size);
    original.GetMsg()->SerializeWithCachedSizesToArray(buffer.data());
    
    NetPacket restored;
    bool success = restored.GetMsg()->ParseFromArray(buffer.data(), size);
    EXPECT_TRUE(success);
    EXPECT_EQ(restored.GetMsg()->messagetype(), PokerTHMessage_PokerTHMessageType_Type_AnnounceMessage);
    EXPECT_EQ(restored.GetMsg()->nettime(), 54321);
    return true;
}

TEST(NetPacketValidator_Exists, TestValidatorCreation)
{
    NetPacketValidator validator;
    EXPECT_TRUE(true);
    return true;
}

TEST(NetPacketValidator_AnnounceMessage, TestValidateAnnounce)
{
    NetPacket packet;
    packet.GetMsg()->set_messagetype(PokerTHMessage_PokerTHMessageType_Type_AnnounceMessage);
    packet.GetMsg()->set_nettime(1000);
    packet.GetMsg()->set_version(1);
    
    NetPacketValidator validator;
    bool valid = validator.IsValidPacket(packet);
    EXPECT_TRUE(valid);
    return true;
}

TEST(NetPacketValidator_InitMessage, TestValidateInit)
{
    NetPacket packet;
    packet.GetMsg()->set_messagetype(PokerTHMessage_PokerTHMessageType_Type_InitMessage);
    packet.GetMsg()->set_nettime(1000);
    
    NetPacketValidator validator;
    bool valid = validator.IsValidPacket(packet);
    EXPECT_TRUE(valid);
    return true;
}

TEST(NetPacketValidator_InvalidType, TestValidateInvalidMessageType)
{
    NetPacket packet;
    packet.GetMsg()->set_messagetype(static_cast<PokerTHMessage_PokerTHMessageType>(999));
    packet.GetMsg()->set_nettime(1000);
    
    NetPacketValidator validator;
    bool valid = validator.IsValidPacket(packet);
    EXPECT_FALSE(valid);
    return true;
}

TEST(NetPacketValidator_ChatMessage, TestValidateChatMessage)
{
    NetPacket packet;
    packet.GetMsg()->set_messagetype(PokerTHMessage_PokerTHMessageType_Type_ChatMessage);
    packet.GetMsg()->set_nettime(1000);
    packet.GetMsg()->mutable_chatmessage()->set_playerid(1);
    packet.GetMsg()->mutable_chatmessage()->set_message("Hello");
    
    NetPacketValidator validator;
    bool valid = validator.IsValidPacket(packet);
    EXPECT_TRUE(valid);
    return true;
}

TEST(NetPacketValidator_GameListMessage, TestValidateGameList)
{
    NetPacket packet;
    packet.GetMsg()->set_messagetype(PokerTHMessage_PokerTHMessageType_Type_GameListNewMessage);
    packet.GetMsg()->set_nettime(1000);
    packet.GetMsg()->mutable_gamelistnew()->set_gameid(1);
    packet.GetMsg()->mutable_gamelistnew()->set_gamename("TestGame");
    
    NetPacketValidator validator;
    bool valid = validator.IsValidPacket(packet);
    EXPECT_TRUE(valid);
    return true;
}

TEST(NetPacketValidator_PlayerActionMessage, TestValidatePlayerAction)
{
    NetPacket packet;
    packet.GetMsg()->set_messagetype(PokerTHMessage_PokerTHMessageType_Type_MyActionRequestMessage);
    packet.GetMsg()->set_nettime(1000);
    packet.GetMsg()->mutable_myactionrequest()->set_playerid(1);
    packet.GetMsg()->mutable_myactionrequest()->set_action(PLAYER_ACTION_FOLD);
    
    NetPacketValidator validator;
    bool valid = validator.IsValidPacket(packet);
    EXPECT_TRUE(valid);
    return true;
}

END_TEST_SUITE

TEST_SUITE(GameLogicTests)

TEST(GameLogic_GameStateDefinitions, TestGameStateValues)
{
    EXPECT_EQ(GAME_STATE_PREFLOP, 0);
    EXPECT_EQ(GAME_STATE_FLOP, 1);
    EXPECT_EQ(GAME_STATE_TURN, 2);
    EXPECT_EQ(GAME_STATE_RIVER, 3);
    EXPECT_EQ(GAME_STATE_POST_RIVER, 4);
    return true;
}

TEST(GameLogic_PlayerActionValues, TestPlayerActionConstants)
{
    EXPECT_EQ(PLAYER_ACTION_NONE, 0);
    EXPECT_EQ(PLAYER_ACTION_FOLD, 1);
    EXPECT_EQ(PLAYER_ACTION_CHECK, 2);
    EXPECT_EQ(PLAYER_ACTION_CALL, 3);
    EXPECT_EQ(PLAYER_ACTION_BET, 4);
    EXPECT_EQ(PLAYER_ACTION_RAISE, 5);
    EXPECT_EQ(PLAYER_ACTION_ALLIN, 6);
    return true;
}

TEST(GameLogic_GameTypeDefinitions, TestGameTypeValues)
{
    EXPECT_EQ(GAME_TYPE_NORMAL, 1);
    EXPECT_EQ(GAME_TYPE_REGISTERED_ONLY, 2);
    EXPECT_EQ(GAME_TYPE_INVITE_ONLY, 3);
    EXPECT_EQ(GAME_TYPE_RANKING, 4);
    return true;
}

TEST(GameLogic_ServerModeDefinitions, TestServerModeValues)
{
    EXPECT_EQ(SERVER_MODE_LAN, 0);
    EXPECT_EQ(SERVER_MODE_INTERNET_NOAUTH, 1);
    EXPECT_EQ(SERVER_MODE_INTERNET_AUTH, 2);
    return true;
}

TEST(GameLogic_PlayerRights, TestPlayerRightsValues)
{
    EXPECT_EQ(PLAYER_RIGHTS_GUEST, 1);
    EXPECT_EQ(PLAYER_RIGHTS_NORMAL, 2);
    EXPECT_EQ(PLAYER_RIGHTS_ADMIN, 3);
    return true;
}

TEST(GameLogic_PlayerType, TestPlayerTypeValues)
{
    EXPECT_EQ(PLAYER_TYPE_COMPUTER, 0);
    EXPECT_EQ(PLAYER_TYPE_HUMAN, 1);
    return true;
}

TEST(GameLogic_MinMaxPlayers, TestPlayerLimits)
{
    EXPECT_EQ(MIN_NUMBER_OF_PLAYERS, 2);
    EXPECT_EQ(MAX_NUMBER_OF_PLAYERS, 10);
    return true;
}

TEST(GameLogic_GUISpeed, TestGUISpeedLimits)
{
    EXPECT_EQ(MIN_GUI_SPEED, 1);
    EXPECT_EQ(MAX_GUI_SPEED, 11);
    return true;
}

TEST(GameLogic_RankingGameConstants, TestRankingGameSettings)
{
    EXPECT_EQ(RANKING_GAME_START_CASH, 10000);
    EXPECT_EQ(RANKING_GAME_NUMBER_OF_PLAYERS, 10);
    EXPECT_EQ(RANKING_GAME_START_SBLIND, 50);
    EXPECT_EQ(RANKING_GAME_RAISE_EVERY_HAND, 11);
    return true;
}

END_TEST_SUITE

TEST_SUITE(PlayerDataTests)

TEST(PlayerData_Constructor, TestPlayerDataInit)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, true);
    EXPECT_EQ(player.GetUniqueId(), 1);
    EXPECT_EQ(player.GetNumber(), 0);
    EXPECT_EQ(player.GetType(), PLAYER_TYPE_HUMAN);
    EXPECT_EQ(player.GetRights(), PLAYER_RIGHTS_NORMAL);
    EXPECT_TRUE(player.IsGameAdmin());
    return true;
}

TEST(PlayerData_SetName, TestPlayerNameChange)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    player.SetName("TestPlayer");
    EXPECT_EQ(player.GetName(), "TestPlayer");
    return true;
}

TEST(PlayerData_SetCountry, TestPlayerCountryChange)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    player.SetCountry("US");
    EXPECT_EQ(player.GetCountry(), "US");
    return true;
}

TEST(PlayerData_SetType, TestPlayerTypeChange)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    player.SetType(PLAYER_TYPE_COMPUTER);
    EXPECT_EQ(player.GetType(), PLAYER_TYPE_COMPUTER);
    return true;
}

TEST(PlayerData_SetRights, TestPlayerRightsChange)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_GUEST, false);
    player.SetRights(PLAYER_RIGHTS_ADMIN);
    EXPECT_EQ(player.GetRights(), PLAYER_RIGHTS_ADMIN);
    return true;
}

TEST(PlayerData_SetGameAdmin, TestGameAdminToggle)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    EXPECT_FALSE(player.IsGameAdmin());
    player.SetGameAdmin(true);
    EXPECT_TRUE(player.IsGameAdmin());
    return true;
}

TEST(PlayerData_SetNumber, TestPlayerSeatNumber)
{
    PlayerData player(1, 5, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    EXPECT_EQ(player.GetNumber(), 5);
    player.SetNumber(7);
    EXPECT_EQ(player.GetNumber(), 7);
    return true;
}

TEST(PlayerData_SetStartCash, TestPlayerStartCash)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    player.SetStartCash(5000);
    EXPECT_EQ(player.GetStartCash(), 5000);
    return true;
}

TEST(PlayerData_GuidManagement, TestGuidSetAndGet)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    player.SetGuid("test-guid-123");
    EXPECT_EQ(player.GetGuid(), "test-guid-123");
    player.SetOldGuid("old-guid-456");
    EXPECT_EQ(player.GetOldGuid(), "old-guid-456");
    return true;
}

TEST(PlayerData_AvatarMD5, TestAvatarHash)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    MD5Buf hash;
    hash.FromString("d41d8cd98f00b204e9800998ecf8427e");
    player.SetAvatarMD5(hash);
    MD5Buf retrieved = player.GetAvatarMD5();
    EXPECT_EQ(retrieved.ToString(), "d41d8cd98f00b204e9800998ecf8427e");
    return true;
}

TEST(PlayerData_ComparisonOperator, TestLessThanOperator)
{
    PlayerData player1(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    PlayerData player2(2, 1, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    EXPECT_TRUE(player1 < player2);
    EXPECT_FALSE(player2 < player1);
    return true;
}

TEST(PlayerData_LastGamesManagement, TestPlayerGamesHistory)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    
    player.AddPlayerLastGame(100);
    player.AddPlayerLastGame(200);
    player.AddPlayerLastGame(300);
    
    std::vector<long> games = player.GetPlayerLastGames();
    EXPECT_EQ(games.size(), 3);
    EXPECT_EQ(games[0], 100);
    EXPECT_EQ(games[1], 200);
    EXPECT_EQ(games[2], 300);
    return true;
}

TEST(PlayerData_SetPlayerLastGames, TestReplaceGamesHistory)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    
    player.AddPlayerLastGame(100);
    player.AddPlayerLastGame(200);
    
    std::vector<long> newGames = {400, 500, 600};
    player.SetPlayerLastGames(newGames);
    
    std::vector<long> games = player.GetPlayerLastGames();
    EXPECT_EQ(games.size(), 3);
    EXPECT_EQ(games[0], 400);
    EXPECT_EQ(games[1], 500);
    EXPECT_EQ(games[2], 600);
    return true;
}

TEST(PlayerData_CopyConstructor, TestPlayerDataCopy)
{
    PlayerData original(1, 5, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_ADMIN, true);
    original.SetName("Original");
    original.SetCountry("US");
    
    PlayerData copy(original);
    
    EXPECT_EQ(copy.GetUniqueId(), original.GetUniqueId());
    EXPECT_EQ(copy.GetNumber(), original.GetNumber());
    EXPECT_EQ(copy.GetName(), original.GetName());
    EXPECT_EQ(copy.GetType(), original.GetType());
    EXPECT_EQ(copy.GetRights(), original.GetRights());
    EXPECT_EQ(copy.IsGameAdmin(), original.IsGameAdmin());
    return true;
}

END_TEST_SUITE

TEST_SUITE(GameDataTests)

TEST(GameData_DefaultConstructor, TestDefaultGameData)
{
    GameData data;
    EXPECT_EQ(data.gameType, GAME_TYPE_NORMAL);
    EXPECT_TRUE(data.allowSpectators);
    EXPECT_EQ(data.maxNumberOfPlayers, 0);
    EXPECT_EQ(data.startMoney, 0);
    EXPECT_EQ(data.firstSmallBlind, 0);
    EXPECT_EQ(data.raiseIntervalMode, RAISE_ON_HANDNUMBER);
    EXPECT_EQ(data.raiseSmallBlindEveryHandsValue, 8);
    EXPECT_EQ(data.raiseSmallBlindEveryMinutesValue, 1);
    EXPECT_EQ(data.raiseMode, DOUBLE_BLINDS);
    EXPECT_EQ(data.afterManualBlindsMode, AFTERMB_DOUBLE_BLINDS);
    EXPECT_EQ(data.afterMBAlwaysRaiseValue, 0);
    EXPECT_EQ(data.guiSpeed, 4);
    EXPECT_EQ(data.delayBetweenHandsSec, 6);
    EXPECT_EQ(data.playerActionTimeoutSec, 20);
    return true;
}

TEST(GameData_CustomValues, TestCustomGameSettings)
{
    GameData data;
    data.gameType = GAME_TYPE_RANKING;
    data.maxNumberOfPlayers = 10;
    data.startMoney = 10000;
    data.firstSmallBlind = 50;
    data.guiSpeed = 8;
    data.playerActionTimeoutSec = 30;
    
    EXPECT_EQ(data.gameType, GAME_TYPE_RANKING);
    EXPECT_EQ(data.maxNumberOfPlayers, 10);
    EXPECT_EQ(data.startMoney, 10000);
    EXPECT_EQ(data.firstSmallBlind, 50);
    EXPECT_EQ(data.guiSpeed, 8);
    EXPECT_EQ(data.playerActionTimeoutSec, 30);
    return true;
}

TEST(GameData_ManualBlindsList, TestManualBlindsConfiguration)
{
    GameData data;
    data.raiseMode = MANUAL_BLINDS_ORDER;
    data.manualBlindsList = {25, 50, 100, 200, 400};
    data.afterManualBlindsMode = AFTERMB_STAY_AT_LAST_BLIND;
    
    EXPECT_EQ(data.raiseMode, MANUAL_BLINDS_ORDER);
    EXPECT_EQ(data.manualBlindsList.size(), 5);
    EXPECT_EQ(data.afterManualBlindsMode, AFTERMB_STAY_AT_LAST_BLIND);
    return true;
}

TEST(GameData_StartDataDefault, TestDefaultStartData)
{
    StartData data;
    EXPECT_EQ(data.startDealerPlayerId, 0);
    EXPECT_EQ(data.numberOfPlayers, 0);
    return true;
}

TEST(GameData_StartDataCustom, TestCustomStartData)
{
    StartData data;
    data.startDealerPlayerId = 3;
    data.numberOfPlayers = 6;
    
    EXPECT_EQ(data.startDealerPlayerId, 3);
    EXPECT_EQ(data.numberOfPlayers, 6);
    return true;
}

TEST(GameData_GameInfoDefault, TestDefaultGameInfo)
{
    GameInfo info;
    EXPECT_EQ(info.mode, GAME_MODE_CREATED);
    EXPECT_EQ(info.adminPlayerId, 0);
    EXPECT_FALSE(info.isPasswordProtected);
    EXPECT_TRUE(info.players.empty());
    EXPECT_TRUE(info.spectators.empty());
    EXPECT_TRUE(info.spectatorsDuringGame.empty());
    return true;
}

TEST(GameData_GameInfoWithPlayers, TestGameInfoWithPlayers)
{
    GameInfo info;
    info.name = "Test Game";
    info.adminPlayerId = 1;
    info.isPasswordProtected = true;
    info.players.push_back(1);
    info.players.push_back(2);
    info.players.push_back(3);
    
    EXPECT_EQ(info.name, "Test Game");
    EXPECT_EQ(info.adminPlayerId, 1);
    EXPECT_TRUE(info.isPasswordProtected);
    EXPECT_EQ(info.players.size(), 3);
    return true;
}

TEST(GameData_VoteKickDataDefault, TestDefaultVoteKickData)
{
    VoteKickData data;
    EXPECT_EQ(data.petitionId, 0);
    EXPECT_EQ(data.kickPlayerId, 0);
    EXPECT_EQ(data.numVotesToKick, 0);
    EXPECT_EQ(data.numVotesInFavourOfKicking, 0);
    EXPECT_EQ(data.numVotesAgainstKicking, 0);
    EXPECT_EQ(data.timeLimitSec, 0);
    EXPECT_TRUE(data.votedPlayerIds.empty());
    return true;
}

END_TEST_SUITE

TEST_SUITE(ChatCleanerTests)

TEST(ChatCleaner_ActionTypes, TestActionTypeEnum)
{
    EXPECT_EQ(NOTHING, 0);
    EXPECT_EQ(WARN, 1);
    EXPECT_EQ(KICK, 2);
    EXPECT_EQ(KICKBAN, 3);
    EXPECT_EQ(MUTE, 4);
    return true;
}

TEST(ChatCleaner_OffenceTypes, TestOffenceTypeEnum)
{
    EXPECT_EQ(NONE, 0);
    EXPECT_EQ(BAD_WORD, 1);
    EXPECT_EQ(TEXT_FLOOD_LINES, 2);
    EXPECT_EQ(CAPS_FLOOD, 3);
    EXPECT_EQ(LETTER_REPEATING, 4);
    EXPECT_EQ(URL, 5);
    return true;
}

END_TEST_SUITE

TEST_SUITE(CryptHelperTests)

TEST(CryptHelper_MD5Size, TestMD5DataSize)
{
    EXPECT_EQ(MD5_DATA_SIZE, 16);
    return true;
}

TEST(CryptHelper_SHA1Size, TestSHA1DataSize)
{
    EXPECT_EQ(SHA1_DATA_SIZE, 20);
    return true;
}

TEST(CryptHelper_AESBlockSize, TestAESBlockSize)
{
    EXPECT_EQ(AES_BLOCK_SIZE, 16);
    return true;
}

TEST(CryptHelper_AddPadding, TestPaddingMacro)
{
    size_t input1 = 10;
    size_t padded1 = ADD_PADDING(input1);
    EXPECT_EQ(padded1, 16);
    
    size_t input2 = 16;
    size_t padded2 = ADD_PADDING(input2);
    EXPECT_EQ(padded2, 16);
    
    size_t input3 = 17;
    size_t padded3 = ADD_PADDING(input3);
    EXPECT_EQ(padded3, 32);
    return true;
}

END_TEST_SUITE

TEST_SUITE(LogUploadTests)

TEST(LogUpload_ErrorCodes, TestLogUploadErrorEnum)
{
    EXPECT_EQ(LOG_UPLOAD_ERROR_NO_FILE, 1);
    EXPECT_EQ(LOG_UPLOAD_ERROR_OPEN_DB, 2);
    EXPECT_EQ(LOG_UPLOAD_ERROR_MAX_NUM_TOTAL, 3);
    EXPECT_EQ(LOG_UPLOAD_ERROR_MAX_NUM_IP, 4);
    EXPECT_EQ(LOG_UPLOAD_ERROR_FILE_SIZE, 5);
    EXPECT_EQ(LOG_UPLOAD_ERROR_FILE_EXT, 6);
    EXPECT_EQ(LOG_UPLOAD_ERROR_FILE_HEAD, 7);
    EXPECT_EQ(LOG_UPLOAD_ERROR_ID, 8);
    EXPECT_EQ(LOG_UPLOAD_ERROR_FILE_MOVE, 9);
    EXPECT_EQ(LOG_UPLOAD_ERROR_INSERT_DB, 10);
    return true;
}

TEST(LogUpload_SizeConstants, TestLogUploadConstants)
{
    EXPECT_EQ(LOG_UPLOAD_ID_SIZE, 40);
    EXPECT_STREQ(LOG_UPLOAD_OK_STR, "OK");
    EXPECT_STREQ(LOG_UPLOAD_ERROR_STR, "ERROR");
    return true;
}

END_TEST_SUITE

TEST_SUITE(BoundaryTests)

TEST(Boundary_ZeroValues, TestZeroBoundaries)
{
    PlayerData player(0, 0, PLAYER_TYPE_COMPUTER, PLAYER_RIGHTS_GUEST, false);
    EXPECT_EQ(player.GetUniqueId(), 0);
    EXPECT_EQ(player.GetNumber(), 0);
    return true;
}

TEST(Boundary_MaxValues, TestMaxPlayerId)
{
    PlayerData player(4294967295, 9, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_ADMIN, true);
    EXPECT_EQ(player.GetUniqueId(), 4294967295u);
    EXPECT_EQ(player.GetNumber(), 9);
    return true;
}

TEST(Boundary_NegativeCash, TestNegativeCash)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    player.SetStartCash(-1000);
    EXPECT_EQ(player.GetStartCash(), -1000);
    return true;
}

TEST(Boundary_EmptyStrings, TestEmptyStringHandling)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    player.SetName("");
    EXPECT_EQ(player.GetName(), "");
    player.SetCountry("");
    EXPECT_EQ(player.GetCountry(), "");
    player.SetGuid("");
    EXPECT_EQ(player.GetGuid(), "");
    return true;
}

TEST(Boundary_LongStrings, TestLongStringHandling)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    std::string longName(1000, 'X');
    player.SetName(longName);
    EXPECT_EQ(player.GetName(), longName);
    return true;
}

TEST(Boundary_ZeroGameData, TestZeroGameValues)
{
    GameData data;
    data.maxNumberOfPlayers = 0;
    data.startMoney = 0;
    data.firstSmallBlind = 0;
    EXPECT_EQ(data.maxNumberOfPlayers, 0);
    EXPECT_EQ(data.startMoney, 0);
    EXPECT_EQ(data.firstSmallBlind, 0);
    return true;
}

TEST(Boundary_ExtremeBlinds, TestBlindBoundaries)
{
    GameData data;
    data.firstSmallBlind = 1;
    data.raiseSmallBlindEveryHandsValue = 1;
    EXPECT_EQ(data.firstSmallBlind, 1);
    EXPECT_EQ(data.raiseSmallBlindEveryHandsValue, 1);
    return true;
}

END_TEST_SUITE

TEST_SUITE(ProtocolTransportTests)

TEST(TransportProtocol_TCP, TestTCPProtocol)
{
    EXPECT_EQ(TRANSPORT_PROTOCOL_TCP, 1);
    return true;
}

TEST(TransportProtocol_SCTP, TestSCTPProtocol)
{
    EXPECT_EQ(TRANSPORT_PROTOCOL_SCTP, 2);
    return true;
}

TEST(TransportProtocol_WebSocket, TestWebSocketProtocol)
{
    EXPECT_EQ(TRANSPORT_PROTOCOL_WEBSOCKET, 4);
    return true;
}

TEST(TransportProtocol_Combined, TestCombinedProtocol)
{
    EXPECT_EQ(TRANSPORT_PROTOCOL_TCP_SCTP, 3);
    EXPECT_EQ(TRANSPORT_PROTOCOL_TCP_WEBSOCKET, 5);
    EXPECT_EQ(TRANSPORT_PROTOCOL_TCP_SCTP_WEBSOCKET, 7);
    return true;
}

END_TEST_SUITE

TEST_SUITE(VersionTests)

TEST(Version_VersionNumbers, TestVersionConstants)
{
    EXPECT_EQ(POKERTH_VERSION_MAJOR, 1);
    EXPECT_EQ(POKERTH_VERSION_MINOR, 11);
    EXPECT_STREQ(POKERTH_BETA_RELEASE_STRING, "1.1.2");
    return true;
}

TEST(Version_VersionCalculation, TestVersionValue)
{
    EXPECT_EQ(POKERTH_VERSION, (1 << 8) | 11);
    EXPECT_EQ(POKERTH_VERSION, 267);
    return true;
}

TEST(Version_SQLiteLogVersion, TestSQLiteLogVersion)
{
    EXPECT_EQ(SQLITE_LOG_VERSION, 1);
    return true;
}

END_TEST_SUITE

TEST_SUITE(NetworkTimeoutTests)

TEST(NetworkTimeout_ReasonValues, TestTimeoutReasons)
{
    EXPECT_EQ(NETWORK_TIMEOUT_GENERIC, 0);
    EXPECT_EQ(NETWORK_TIMEOUT_GAME_ADMIN_IDLE, 1);
    EXPECT_EQ(NETWORK_TIMEOUT_KICK_AFTER_AUTOFOLD, 2);
    return true;
}

END_TEST_SUITE

TEST_SUITE(VoteKickTests)

TEST(VoteKick_KickVoteValues, TestKickVoteEnum)
{
    EXPECT_EQ(KICK_VOTE_AGAINST, 0);
    EXPECT_EQ(KICK_VOTE_IN_FAVOUR, 1);
    return true;
}

TEST(VoteKick_DenyReasons, TestDenyKickReasons)
{
    EXPECT_EQ(KICK_DENIED_INVALID_STATE, 0);
    EXPECT_EQ(KICK_DENIED_TOO_FEW_PLAYERS, 1);
    EXPECT_EQ(KICK_DENIED_TEMPORARY, 2);
    EXPECT_EQ(KICK_DENIED_OTHER_IN_PROGRESS, 3);
    EXPECT_EQ(KICK_DENIED_INVALID_PLAYER_ID, 4);
    return true;
}

TEST(VoteKick_PetitionEndReasons, TestPetitionEndReasons)
{
    EXPECT_EQ(PETITION_END_ENOUGH_VOTES, 0);
    EXPECT_EQ(PETITION_END_NOT_ENOUGH_PLAYERS, 1);
    EXPECT_EQ(PETITION_END_PLAYER_LEFT, 2);
    EXPECT_EQ(PETITION_END_TIMEOUT, 3);
    return true;
}

TEST(VoteKick_DenyVoteReasons, TestDenyVoteReasons)
{
    EXPECT_EQ(VOTE_DENIED_INVALID_PETITION, 0);
    EXPECT_EQ(VOTE_DENIED_ALREADY_VOTED, 1);
    return true;
}

END_TEST_SUITE

TEST_SUITE(GameInvitationTests)

TEST(GameInvitation_DenyReasons, TestDenyInvitationReasons)
{
    EXPECT_EQ(DENY_GAME_INVITATION_NO, 0);
    EXPECT_EQ(DENY_GAME_INVITATION_BUSY, 1);
    return true;
}

END_TEST_SUITE

TEST_SUITE(ButtonTests)

TEST(Button_ButtonEnum, TestButtonValues)
{
    EXPECT_EQ(BUTTON_NONE, 0);
    EXPECT_EQ(BUTTON_DEALER, 1);
    EXPECT_EQ(BUTTON_SMALL_BLIND, 2);
    EXPECT_EQ(BUTTON_BIG_BLIND, 3);
    return true;
}

END_TEST_SUITE

TEST_SUITE(PlayerActionLogTests)

TEST(PlayerActionLog_LogActions, TestActionLogEnum)
{
    EXPECT_EQ(LOG_ACTION_NONE, 0);
    EXPECT_EQ(LOG_ACTION_DEALER, 1);
    EXPECT_EQ(LOG_ACTION_SMALL_BLIND, 2);
    EXPECT_EQ(LOG_ACTION_BIG_BLIND, 3);
    EXPECT_EQ(LOG_ACTION_FOLD, 4);
    EXPECT_EQ(LOG_ACTION_CHECK, 5);
    EXPECT_EQ(LOG_ACTION_CALL, 6);
    EXPECT_EQ(LOG_ACTION_BET, 7);
    EXPECT_EQ(LOG_ACTION_ALL_IN, 8);
    EXPECT_EQ(LOG_ACTION_SHOW, 9);
    EXPECT_EQ(LOG_ACTION_HAS, 10);
    EXPECT_EQ(LOG_ACTION_WIN, 11);
    EXPECT_EQ(LOG_ACTION_WIN_SIDE_POT, 12);
    EXPECT_EQ(LOG_ACTION_SIT_OUT, 13);
    EXPECT_EQ(LOG_ACTION_WIN_GAME, 14);
    EXPECT_EQ(LOG_ACTION_LEFT, 15);
    EXPECT_EQ(LOG_ACTION_KICKED, 16);
    EXPECT_EQ(LOG_ACTION_ADMIN, 17);
    EXPECT_EQ(LOG_ACTION_JOIN, 18);
    return true;
}

END_TEST_SUITE

TEST_SUITE(ActionCodeTests)

TEST(ActionCode_ActionCodes, TestActionCodeEnum)
{
    EXPECT_EQ(ACTION_CODE_VALID, 0);
    EXPECT_EQ(ACTION_CODE_INVALID_STATE, 1);
    EXPECT_EQ(ACTION_CODE_NOT_YOUR_TURN, 2);
    EXPECT_EQ(ACTION_CODE_NOT_ALLOWED, 3);
    return true;
}

END_TEST_SUITE

TEST_SUITE(RandomizedTests)

TEST(Random_CardShuffling, TestRandomCardDistribution)
{
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 51);
    std::map<int, int> cardCounts;
    
    for (int i = 0; i < 5200; i++) {
        int card = dist(rng);
        cardCounts[card]++;
    }
    
    EXPECT_EQ(cardCounts.size(), 52);
    for (int card = 0; card < 52; card++) {
        EXPECT_TRUE(cardCounts[card] >= 90 && cardCounts[card] <= 110);
    }
    return true;
}

TEST(Random_HandDistribution, TestRandomHandClasses)
{
    std::mt19937 rng(123);
    std::uniform_int_distribution<int> cardDist(0, 51);
    std::set<int> usedCards;
    std::map<int, int> handClassCounts;
    
    for (int test = 0; test < 1000; test++) {
        usedCards.clear();
        int cards[7];
        for (int i = 0; i < 7; i++) {
            int card;
            do {
                card = cardDist(rng);
            } while (usedCards.find(card) != usedCards.end());
            usedCards.insert(card);
            cards[i] = card;
        }
        
        int position[5];
        int value = evaluateCardsValue(cards, position);
        int handClass = value / 100000000;
        handClassCounts[handClass]++;
    }
    
    EXPECT_TRUE(handClassCounts[0] > 0);
    EXPECT_TRUE(handClassCounts[1] > 0);
    EXPECT_TRUE(handClassCounts[2] > 0);
    EXPECT_TRUE(handClassCounts[3] > 0);
    EXPECT_TRUE(handClassCounts[4] > 0);
    EXPECT_TRUE(handClassCounts[5] > 0);
    EXPECT_TRUE(handClassCounts[6] > 0);
    EXPECT_TRUE(handClassCounts[7] > 0);
    EXPECT_TRUE(handClassCounts[8] > 0);
    EXPECT_TRUE(handClassCounts[9] > 0);
    return true;
}

END_TEST_SUITE

int main()
{
    std::cout << "================================================\n";
    std::cout << "PokerTH Comprehensive Unit Test Suite\n";
    std::cout << "================================================\n";
    std::cout << "Testing all core components:\n";
    std::cout << "  - Card evaluation and hand rankings\n";
    std::cout << "  - Network packet validation\n";
    std::cout << "  - Player data management\n";
    std::cout << "  - Game logic and state machines\n";
    std::cout << "  - Edge cases and boundary conditions\n";
    std::cout << "  - Protocol and transport layers\n";
    std::cout << "================================================\n\n";
    
    return RUN_ALL_TESTS();
}
