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
 *                                                                           *
 *                                                                           *
 * Additional permission under GNU AGPL version 3 section 7                  *
 *                                                                           *
 * If you modify this program, or any covered work, by linking or            *
 * combining it with the OpenSSL project's OpenSSL library (or a             *
 * modified version of that library), containing parts covered by the        *
 * terms of the OpenSSL or SSLeay licenses, the authors of PokerTH           *
 * (Felix Hammer, Florian Thauer, Lothar May) grant you additional           *
 * permission to convey the resulting work.                                  *
 * Corresponding Source for a non-source form of such a combination          *
 * shall include the source code for the parts of OpenSSL used as well       *
 * as that of the covered work.                                              *
 *****************************************************************************/

#ifndef LOCALPLAYER_H
#define LOCALPLAYER_H

#include <playerinterface.h>

#include <boost/shared_ptr.hpp>
#include <string>
#include <array>


class ConfigFile;
class HandInterface;

class LocalPlayer : public PlayerInterface
{
public:
	LocalPlayer(ConfigFile*, int id, unsigned uniqueId, PlayerType type, std::string name, std::string avatar, int sC, bool aS, bool sotS, int mB);

	~LocalPlayer() noexcept override;

	void setHand(HandInterface *) override;

	int getMyID() const override
	{
		return myID;
	}
	unsigned getMyUniqueID() const override
	{
		return myUniqueID;
	}
	void setMyUniqueID(unsigned newId) override
	{
		myUniqueID = newId;
	}

	void setMyGuid(const std::string &theValue) override
	{
		myGuid = theValue;
	}

	std::string getMyGuid() const override
	{
		return myGuid;
	}

	PlayerType getMyType() const override
	{
		return myType;
	}

	void setMyDude(int theValue) override
	{
		myDude = theValue;
	}
	int getMyDude() const override
	{
		return myDude;
	}

	void setMyDude4(int theValue) override
	{
		myDude4 = theValue;
	}
	int getMyDude4() const override
	{
		return myDude4;
	}

	void setMyName(const std::string& theValue) override
	{
		myName = theValue;
	}
	std::string getMyName() const override
	{
		return myName;
	}

	void setMyAvatar(const std::string& theValue) override
	{
		myAvatar = theValue;
	}
	std::string getMyAvatar() const override
	{
		return myAvatar;
	}

	void setMyCash(int theValue) override
	{
		myCash = theValue;
	}
	int getMyCash() const override
	{
		return myCash;
	}

	void setMySet(int theValue) override
	{
		myLastRelativeSet = theValue;
		mySet += theValue;
		myCash -= theValue;
	}
	void setMySetAbsolute(int theValue) override
	{
		mySet = theValue;
	}
	void setMySetNull() override
	{
		mySet = 0;
		myLastRelativeSet = 0;
	}
	int getMySet() const override
	{
		return mySet;
	}
	int getMyLastRelativeSet() const override
	{
		return myLastRelativeSet;
	}

	void setMyAction(PlayerAction theValue, bool human = 0) override
	{
		myAction = theValue;
		if(myAction && human && currentHand && currentHand->getGuiInterface()) currentHand->getGuiInterface()->logPlayerActionMsg(myName, myAction, myLastRelativeSet);
	}
	PlayerAction getMyAction() const override
	{
		return myAction;
	}

	void setMyButton(int theValue) override
	{
		myButton = theValue;
	}
	int getMyButton() const override
	{
		return myButton;
	}

	void setMyActiveStatus(bool theValue) override
	{
		myActiveStatus = theValue;
	}
	bool getMyActiveStatus() const override
	{
		return myActiveStatus;
	}

	void setMyStayOnTableStatus(bool theValue) override
	{
		myStayOnTableStatus = theValue;
	}
	bool getMyStayOnTableStatus() const override
	{
		return myStayOnTableStatus;
	}

	void setMyCards(const std::array<int, 2> &theValue) override
	{
		myCards = theValue;
	}
	void getMyCards(std::array<int, 2> &theValue) const override
	{
		theValue = myCards;
	}

	void setMyTurn(bool theValue) override
	{
		myTurn = theValue;
	}
	bool getMyTurn() const override
	{
		return myTurn;
	}

	void setMyCardsFlip(bool theValue, int state) override
	{
		myCardsFlip = theValue;
		if(myCardsFlip && currentHand && currentHand->getGuiInterface()) {
			switch(state) {
			case 1:
				currentHand->getGuiInterface()->logFlipHoleCardsMsg(myName, myCards[0], myCards[1], myCardsValueInt);
				break;
			case 2:
				currentHand->getGuiInterface()->logFlipHoleCardsMsg(myName, myCards[0], myCards[1]);
				break;
			case 3:
				currentHand->getGuiInterface()->logFlipHoleCardsMsg(myName, myCards[0], myCards[1], myCardsValueInt, "has");
				break;
			default:
				;
			}
		}
	}
	bool getMyCardsFlip() const override
	{
		return myCardsFlip;
	}

	void setMyCardsValueInt(int theValue) override
	{
		myCardsValueInt = theValue;
	}
	int getMyCardsValueInt() const override
	{
		return myCardsValueInt;
	}

	void setLogHoleCardsDone(bool theValue) override
	{
		logHoleCardsDone = theValue;
	}

	bool getLogHoleCardsDone() const override
	{
		return logHoleCardsDone;
	}

	void setMyBestHandPosition(const std::array<int, 5> &theValue) override
	{
		myBestHandPosition = theValue;
	}
	void getMyBestHandPosition(std::array<int, 5> &theValue) const override
	{
		theValue = myBestHandPosition;
	}

	void setMyRoundStartCash(int theValue) override
	{
		myRoundStartCash = theValue;
	}
	int getMyRoundStartCash() const override
	{
		return myRoundStartCash;
	}

	void setLastMoneyWon ( int theValue ) override
	{
		myLastMoneyWon = theValue;
	}
	int getLastMoneyWon() const override
	{
		return myLastMoneyWon;
	}

	void setMyAverageSets(int theValue) override
	{
		myAverageSets[0] = myAverageSets[1];
		myAverageSets[1] = myAverageSets[2];
		myAverageSets[2] = myAverageSets[3];
		myAverageSets[3] = theValue;
	}
	int getMyAverageSets() const override
	{
		return (myAverageSets[0]+myAverageSets[1]+myAverageSets[2]+myAverageSets[3])/4;
	}

	void setMyAggressive(bool theValue) override
	{
		int i;
		for(i=0; i<6; i++) {
			myAggressive[i] = myAggressive[i+1];
		}
		myAggressive[6] = theValue;
	}
	int getMyAggressive() const override
	{
		int i, sum = 0;
		for(i=0; i<7; i++) {
			sum += myAggressive[i];
		}
		return sum;
	}

	void setSBluff ( int theValue ) override
	{
		mySBluff = theValue;
	}
	int getSBluff() const override
	{
		return mySBluff;
	}

	void setSBluffStatus ( bool theValue ) override
	{
		mySBluffStatus = theValue;
	}
	bool getSBluffStatus() const override
	{
		return mySBluffStatus;
	}

	void action() override;

	int checkMyAction(int targetAction, int targetBet, int highestSet, int minimumRaise, int smallBlind) override;

	void preflopEngine() override;
	void flopEngine() override;
	void turnEngine() override;
	void riverEngine() override;

	void preflopEngine3();
	void flopEngine3();
	void turnEngine3();
	void riverEngine3();

	int flopCardsValue(int*);
	int turnCardsValue(int*);

	void calcMyOdds();

	void evaluation(int, int);

	void setIsSessionActive(bool active) override;
	bool isSessionActive() const override;
	void setIsKicked(bool kicked) override;
	bool isKicked() const override;
	void setIsMuted(bool muted) override;
	bool isMuted() const override;

	bool checkIfINeedToShowCards() override;

	void markRemoteAction() override;
	unsigned getTimeSecSinceLastRemoteAction() const override;

private:

	ConfigFile *myConfig;
	HandInterface *currentHand;

	// Konstanten
	int myID;
	unsigned myUniqueID;
	std::string myGuid;
	PlayerType myType;
	std::string myName;
	std::string myAvatar;
	int myDude;
	int myDude4;


	// Laufvariablen
	int myCardsValueInt;
	std::array<int, 5> myBestHandPosition;
	double myOdds;
	std::array<int, 3> myNiveau;
	bool logHoleCardsDone;

	std::array<int, 2> myCards;
	int myCash;
	int mySet;
	int myLastRelativeSet;
	PlayerAction myAction;
	int myButton; // 0 = none, 1 = dealer, 2 =small, 3 = big
	bool myActiveStatus; // 0 = inactive, 1 = active
	bool myStayOnTableStatus; // 0 = left, 1 = stay
	bool myTurn; // 0 = no, 1 = yes
	bool myCardsFlip; // 0 = cards are not fliped, 1 = cards are already flipped,
	int myRoundStartCash;
	int myLastMoneyWon;

	std::array<int, 4> myAverageSets;
	std::array<bool, 7> myAggressive;

	int mySBluff;
	bool mySBluffStatus;

	unsigned myActionTimeoutCounter;
	bool myIsSessionActive;
	bool myIsKicked;
	bool myIsMuted;
	boost::timers::portable::microsec_timer m_lastRemoteActionTimer;
};

#endif
