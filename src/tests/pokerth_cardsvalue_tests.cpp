/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU Affero General Public License as            *
 * published by the Free Software Foundation, either version 3 of the        *
 * License, or (at your option) any later version.                           *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU Affero General Public License for more details.                       *
 *                                                                           *
 * You should have received a copy of the GNU Affero General Public License  *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************/

#include <boost/test/unit_test.hpp>
#include "cardsvalue.h"
#include "arraydata.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <set>
#include <map>
#include <functional>
#include <random>
#include <cmath>

using namespace std;

BOOST_AUTO_TEST_SUITE(CardsValueSuite)

struct CardsValueTestFixture {
    CardsValue cardsValue;
    
    CardsValueTestFixture() {}
    ~CardsValueTestFixture() {}
};

BOOST_FIXTURE_TEST_SUITE(CardsValueTests, CardsValueTestFixture)

BOOST_AUTO_TEST_CASE(StraightFlush_AceHigh)
{
    int cards[7] = {12, 11, 10, 9, 8, 0, 1};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 8);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 12);
    BOOST_CHECK_EQUAL(position[0], 0);
    BOOST_CHECK_EQUAL(position[1], 1);
}

BOOST_AUTO_TEST_CASE(StraightFlush_KingHigh)
{
    int cards[7] = {25, 24, 23, 22, 21, 5, 6};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 8);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 11);
}

BOOST_AUTO_TEST_CASE(StraightFlush_FiveHigh_Wheel)
{
    int cards[7] = {3, 12, 11, 10, 9, 5, 6};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 8);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 3);
}

BOOST_AUTO_TEST_CASE(RoyalFlush)
{
    int cards[7] = {12, 11, 10, 9, 8, 50, 51};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 9);
}

BOOST_AUTO_TEST_CASE(FourOfAKind_Aces)
{
    int cards[7] = {12, 25, 38, 51, 10, 5, 6};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 7);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 12);
    BOOST_CHECK_EQUAL(value % 10000 / 100, 10);
}

BOOST_AUTO_TEST_CASE(FourOfAKind_Kickers)
{
    int cards[7] = {0, 13, 26, 39, 12, 11, 10};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 7);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 0);
    BOOST_CHECK_EQUAL(value % 10000 / 100, 12);
}

BOOST_AUTO_TEST_CASE(FullHouse_AcesFullOfKings)
{
    int cards[7] = {12, 25, 38, 11, 24, 50, 51};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 6);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 12);
    BOOST_CHECK_EQUAL(value % 10000 / 100, 11);
}

BOOST_AUTO_TEST_CASE(FullHouse_KingsFullOfAces)
{
    int cards[7] = {11, 24, 37, 12, 25, 50, 51};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 6);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 11);
    BOOST_CHECK_EQUAL(value % 10000 / 100, 12);
}

BOOST_AUTO_TEST_CASE(Flush_AceHigh)
{
    int cards[7] = {0, 3, 6, 9, 12, 50, 51};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 5);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 12);
}

BOOST_AUTO_TEST_CASE(Flush_KickerComparison)
{
    int flush1[7] = {0, 3, 6, 9, 12, 1, 2};
    int flush2[7] = {0, 3, 6, 9, 11, 50, 51};
    int pos1[5], pos2[5];
    int value1 = cardsValue.cardsValue(flush1, pos1);
    int value2 = cardsValue.cardsValue(flush2, pos2);
    
    BOOST_CHECK_GT(value1, value2);
}

BOOST_AUTO_TEST_CASE(Straight_AceHigh)
{
    int cards[7] = {12, 11, 10, 9, 8, 50, 51};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 4);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 12);
}

BOOST_AUTO_TEST_CASE(Straight_KingHigh)
{
    int cards[7] = {11, 10, 9, 8, 7, 50, 51};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 4);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 11);
}

BOOST_AUTO_TEST_CASE(Straight_FiveHigh_Wheel)
{
    int cards[7] = {3, 12, 11, 10, 9, 50, 51};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 4);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 3);
}

BOOST_AUTO_TEST_CASE(ThreeOfAKind_Aces)
{
    int cards[7] = {12, 25, 38, 10, 11, 50, 51};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 3);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 12);
}

BOOST_AUTO_TEST_CASE(TwoPair_AcesAndKings)
{
    int cards[7] = {12, 25, 11, 24, 10, 50, 51};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 2);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 12);
    BOOST_CHECK_EQUAL(value % 10000 / 100, 11);
    BOOST_CHECK_EQUAL(value % 100, 10);
}

BOOST_AUTO_TEST_CASE(OnePair_Aces)
{
    int cards[7] = {12, 25, 10, 11, 9, 50, 51};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 1);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 12);
}

BOOST_AUTO_TEST_CASE(HighCard_Ace)
{
    int cards[7] = {12, 10, 8, 5, 3, 50, 51};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 0);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 12);
}

BOOST_AUTO_TEST_CASE(HandRanking_Order)
{
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
    
    int pos[5];
    int rf = cardsValue.cardsValue(royalFlush, pos);
    int sf = cardsValue.cardsValue(straightFlush, pos);
    int fk = cardsValue.cardsValue(fourOfAKind, pos);
    int fh = cardsValue.cardsValue(fullHouse, pos);
    int fl = cardsValue.cardsValue(flush, pos);
    int st = cardsValue.cardsValue(straight, pos);
    int tk = cardsValue.cardsValue(threeOfAKind, pos);
    int tp = cardsValue.cardsValue(twoPair, pos);
    int op = cardsValue.cardsValue(onePair, pos);
    int hc = cardsValue.cardsValue(highCard, pos);
    
    BOOST_CHECK_GT(rf, sf);
    BOOST_CHECK_GT(sf, fk);
    BOOST_CHECK_GT(fk, fh);
    BOOST_CHECK_GT(fh, fl);
    BOOST_CHECK_GT(fl, st);
    BOOST_CHECK_GT(st, tk);
    BOOST_CHECK_GT(tk, tp);
    BOOST_CHECK_GT(tp, op);
    BOOST_CHECK_GT(op, hc);
}

BOOST_AUTO_TEST_CASE(StraightFlush_AllSuits)
{
    int suits[4] = {0, 13, 26, 39};
    for (int suit = 0; suit < 4; suit++) {
        int cards[7] = {suits[suit] + 12, suits[suit] + 11, suits[suit] + 10, 
                        suits[suit] + 9, suits[suit] + 8, 50, 51};
        int position[5];
        int value = cardsValue.cardsValue(cards, position);
        BOOST_CHECK_EQUAL(value / 100000000, 9);
    }
}

BOOST_AUTO_TEST_CASE(BoardOnlyHand)
{
    int board[7] = {0, 1, 2, 3, 4, 50, 51};
    int position[5];
    int value = cardsValue.cardsValue(board, position);
    
    int expectedClass = value / 100000000;
    BOOST_CHECK(expectedClass >= 0 && expectedClass <= 9);
}

BOOST_AUTO_TEST_CASE(PlayerCardsOnBoard)
{
    int mixed[7] = {12, 25, 0, 13, 26, 50, 51};
    int position[5];
    int value = cardsValue.cardsValue(mixed, position);
    
    BOOST_CHECK(position[0] >= 0 && position[0] <= 6);
    BOOST_CHECK(position[1] >= 0 && position[1] <= 6);
}

BOOST_AUTO_TEST_CASE(SameHandStrengthTieBreaker)
{
    int hand1[7] = {12, 25, 0, 13, 26, 27, 28};
    int hand2[7] = {11, 24, 0, 13, 26, 27, 28};
    int pos1[5], pos2[5];
    int value1 = cardsValue.cardsValue(hand1, pos1);
    int value2 = cardsValue.cardsValue(hand2, pos2);
    
    BOOST_CHECK_GT(value1, value2);
}

BOOST_AUTO_TEST_CASE(AllCardsSameRank)
{
    int cards[7] = {0, 13, 26, 39, 12, 25, 38};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 7);
}

BOOST_AUTO_TEST_CASE(SevenCardsAllSameSuit)
{
    int cards[7] = {0, 3, 6, 9, 12, 1, 2};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 5);
}

BOOST_AUTO_TEST_CASE(MultipleStraightPossibilities)
{
    int cards[7] = {12, 11, 10, 9, 8, 7, 6};
    int position[5];
    int value = cardsValue.cardsValue(cards, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 4);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 12);
}

BOOST_AUTO_TEST_CASE(BroadwayStraight)
{
    int broadway[7] = {12, 11, 10, 9, 8, 0, 13};
    int position[5];
    int value = cardsValue.cardsValue(broadway, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 4);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 12);
}

BOOST_AUTO_TEST_CASE(GapStraight)
{
    int gapStraight[7] = {12, 10, 9, 8, 7, 50, 51};
    int position[5];
    int value = cardsValue.cardsValue(gapStraight, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 0);
}

BOOST_AUTO_TEST_CASE(FullHouseWithMultipleTriples)
{
    int tripleFullHouse[7] = {12, 25, 38, 11, 24, 37, 50};
    int position[5];
    int value = cardsValue.cardsValue(tripleFullHouse, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 6);
}

BOOST_AUTO_TEST_CASE(TwoPairsWithMultiplePairs)
{
    int multiPair[7] = {12, 25, 11, 24, 10, 23, 50};
    int position[5];
    int value = cardsValue.cardsValue(multiPair, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 2);
    BOOST_CHECK_EQUAL(value % 1000000 / 1000000, 12);
    BOOST_CHECK_EQUAL(value % 10000 / 100, 11);
}

BOOST_AUTO_TEST_CASE(PositionArrayIntegrity)
{
    int cards[7] = {12, 11, 10, 9, 8, 50, 51};
    int position[5];
    cardsValue.cardsValue(cards, position);
    
    std::set<int> usedPositions;
    for (int i = 0; i < 5; i++) {
        BOOST_CHECK(position[i] >= 0 && position[i] <= 6);
        BOOST_CHECK(usedPositions.find(position[i]) == usedPositions.end());
        usedPositions.insert(position[i]);
    }
}

BOOST_AUTO_TEST_CASE(WheelStraightWithAceAsLow)
{
    int wheel[7] = {3, 12, 11, 10, 9, 5, 6};
    int position[5];
    int value = cardsValue.cardsValue(wheel, position);
    
    BOOST_CHECK_EQUAL(value / 100000000, 4);
}

BOOST_AUTO_TEST_CASE(RoyalFlushAllSuits)
{
    for (int suit = 0; suit < 4; suit++) {
        int cards[7] = {suit * 13 + 12, suit * 13 + 11, suit * 13 + 10,
                        suit * 13 + 9, suit * 13 + 8, 50, 51};
        int position[5];
        int value = cardsValue.cardsValue(cards, position);
        BOOST_CHECK_EQUAL(value / 100000000, 9);
    }
}

BOOST_AUTO_TEST_CASE(FlushBeatsStraight)
{
    int flush[7] = {0, 3, 6, 9, 12, 50, 51};
    int straight[7] = {11, 10, 9, 8, 7, 50, 51};
    int pos1[5], pos2[5];
    int flushVal = cardsValue.cardsValue(flush, pos1);
    int straightVal = cardsValue.cardsValue(straight, pos2);
    
    BOOST_CHECK_GT(flushVal, straightVal);
}

BOOST_AUTO_TEST_CASE(FullHouseBeatsFlush)
{
    int fullHouse[7] = {12, 25, 38, 11, 24, 50, 51};
    int flush[7] = {0, 3, 6, 9, 12, 50, 51};
    int pos1[5], pos2[5];
    int fhVal = cardsValue.cardsValue(fullHouse, pos1);
    int flushVal = cardsValue.cardsValue(flush, pos2);
    
    BOOST_CHECK_GT(fhVal, flushVal);
}

BOOST_AUTO_TEST_CASE(QuadsBeatsFullHouse)
{
    int quads[7] = {12, 25, 38, 51, 10, 50, 51};
    int fullHouse[7] = {12, 25, 38, 11, 24, 50, 51};
    int pos1[5], pos2[5];
    int quadVal = cardsValue.cardsValue(quads, pos1);
    int fhVal = cardsValue.cardsValue(fullHouse, pos2);
    
    BOOST_CHECK_GT(quadVal, fhVal);
}

BOOST_AUTO_TEST_CASE(StraightFlushBeatsQuads)
{
    int straightFlush[7] = {0, 1, 2, 3, 4, 50, 51};
    int quads[7] = {12, 25, 38, 51, 10, 50, 51};
    int pos1[5], pos2[5];
    int sfVal = cardsValue.cardsValue(straightFlush, pos1);
    int quadVal = cardsValue.cardsValue(quads, pos2);
    
    BOOST_CHECK_GT(sfVal, quadVal);
}

BOOST_AUTO_TEST_CASE(StraightAceHighVsKingHigh)
{
    int aceHigh[7] = {12, 11, 10, 9, 8, 50, 51};
    int kingHigh[7] = {11, 10, 9, 8, 7, 50, 51};
    int pos1[5], pos2[5];
    int aceVal = cardsValue.cardsValue(aceHigh, pos1);
    int kingVal = cardsValue.cardsValue(kingHigh, pos2);
    
    BOOST_CHECK_GT(aceVal, kingVal);
}

BOOST_AUTO_TEST_CASE(FullHouseHigherTripleWins)
{
    int acesFull[7] = {12, 25, 38, 11, 24, 50, 51};
    int kingsFull[7] = {11, 24, 37, 10, 23, 50, 51};
    int pos1[5], pos2[5];
    int acesVal = cardsValue.cardsValue(acesFull, pos1);
    int kingsVal = cardsValue.cardsValue(kingsFull, pos2);
    
    BOOST_CHECK_GT(acesVal, kingsVal);
}

BOOST_AUTO_TEST_CASE(FullHouseSameTripleHigherPairWins)
{
    int acesOverKings[7] = {12, 25, 38, 11, 24, 50, 51};
    int acesOverQueens[7] = {12, 25, 38, 10, 23, 50, 51};
    int pos1[5], pos2[5];
    int overKings = cardsValue.cardsValue(acesOverKings, pos1);
    int overQueens = cardsValue.cardsValue(acesOverQueens, pos2);
    
    BOOST_CHECK_GT(overKings, overQueens);
}

BOOST_AUTO_TEST_CASE(TwoPairHigherPairWins)
{
    int acesKings[7] = {12, 25, 11, 24, 10, 50, 51};
    int kingsQueens[7] = {11, 24, 10, 23, 9, 50, 51};
    int pos1[5], pos2[5];
    int akVal = cardsValue.cardsValue(acesKings, pos1);
    int kqVal = cardsValue.cardsValue(kingsQueens, pos2);
    
    BOOST_CHECK_GT(akVal, kqVal);
}

BOOST_AUTO_TEST_CASE(TwoPairSameHigherLowerPairKickerWins)
{
    int acesKingsQueenKicker[7] = {12, 25, 11, 24, 10, 50, 51};
    int acesKingsJackKicker[7] = {12, 25, 11, 24, 9, 50, 51};
    int pos1[5], pos2[5];
    int queenKick = cardsValue.cardsValue(acesKingsQueenKicker, pos1);
    int jackKick = cardsValue.cardsValue(acesKingsJackKicker, pos2);
    
    BOOST_CHECK_GT(queenKick, jackKick);
}

BOOST_AUTO_TEST_CASE(PairHigherPairWins)
{
    int pairAces[7] = {12, 25, 10, 11, 9, 50, 51};
    int pairKings[7] = {11, 24, 10, 12, 9, 50, 51};
    int pos1[5], pos2[5];
    int acesVal = cardsValue.cardsValue(pairAces, pos1);
    int kingsVal = cardsValue.cardsValue(pairKings, pos2);
    
    BOOST_CHECK_GT(acesVal, kingsVal);
}

BOOST_AUTO_TEST_CASE(PairSamePairKickerComparison)
{
    int pairAcesAKQ[7] = {12, 25, 11, 10, 9, 50, 51};
    int pairAcesAKJ[7] = {12, 25, 11, 10, 8, 50, 51};
    int pos1[5], pos2[5];
    int akqVal = cardsValue.cardsValue(pairAcesAKQ, pos1);
    int akjVal = cardsValue.cardsValue(pairAcesAKJ, pos2);
    
    BOOST_CHECK_GT(akqVal, akjVal);
}

BOOST_AUTO_TEST_CASE(HighCardKickerComparison)
{
    int akqj[7] = {12, 11, 10, 9, 8, 50, 51};
    int akqt[7] = {12, 11, 10, 8, 7, 50, 51};
    int pos1[5], pos2[5];
    int akqjVal = cardsValue.cardsValue(akqj, pos1);
    int akqtVal = cardsValue.cardsValue(akqt, pos2);
    
    BOOST_CHECK_GT(akqjVal, akqtVal);
}

BOOST_AUTO_TEST_CASE(RandomHandsConsistency)
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
        int value = cardsValue.cardsValue(cards, position);
        
        BOOST_CHECK(value >= 0);
        BOOST_CHECK(position[0] >= 0 && position[0] <= 6);
    }
}

BOOST_AUTO_TEST_CASE(HoleCardsClass_AA)
{
    int classAA = CardsValue::holeCardsClass(12, 25);
    BOOST_CHECK_EQUAL(classAA, 10);
}

BOOST_AUTO_TEST_CASE(HoleCardsClass_KK)
{
    int classKK = CardsValue::holeCardsClass(11, 24);
    BOOST_CHECK(classKK >= 6 && classKK <= 9);
}

BOOST_AUTO_TEST_CASE(HoleCardsClass_QQ)
{
    int classQQ = CardsValue::holeCardsClass(10, 23);
    BOOST_CHECK(classQQ >= 5 && classQQ <= 8);
}

BOOST_AUTO_TEST_CASE(HoleCardsClass_SuitedConnectors)
{
    int suitedConnectors = CardsValue::holeCardsClass(12, 11);
    BOOST_CHECK(suitedConnectors >= 4 && suitedConnectors <= 10);
}

BOOST_AUTO_TEST_CASE(HoleCardsClass_Offsuit)
{
    int offsuit = CardsValue::holeCardsClass(12, 10);
    BOOST_CHECK(offsuit >= 1 && offsuit <= 9);
}

BOOST_AUTO_TEST_CASE(HoleCardsClass_Gap)
{
    int oneGap = CardsValue::holeCardsClass(12, 10);
    int twoGap = CardsValue::holeCardsClass(12, 9);
    
    BOOST_CHECK(oneGap >= twoGap);
}

BOOST_AUTO_TEST_CASE(HoleCardsToIntCode_Pair)
{
    int pairCards[2] = {12, 25};
    int code = CardsValue::holeCardsToIntCode(pairCards);
    
    BOOST_CHECK_EQUAL(code / 1000, 12);
    BOOST_CHECK_EQUAL(code % 10, 0);
}

BOOST_AUTO_TEST_CASE(HoleCardsToIntCode_Suited)
{
    int suitedCards[2] = {12, 11};
    int code = CardsValue::holeCardsToIntCode(suitedCards);
    
    BOOST_CHECK_EQUAL(code % 10, 1);
}

BOOST_AUTO_TEST_CASE(HoleCardsToIntCode_Offsuit)
{
    int offsuitCards[2] = {12, 11};
    int suitedCode = CardsValue::holeCardsToIntCode(offsuitCards);
    
    int offsuitCards2[2] = {12, 10};
    int offsuitCode = CardsValue::holeCardsToIntCode(offsuitCards2);
    
    BOOST_CHECK_EQUAL(offsuitCode % 10, 0);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
