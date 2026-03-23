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

#ifndef CLIENTPLAYER_H
#define CLIENTPLAYER_H

#include <playerinterface.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <string>
#include <array>

class ConfigFile;
class HandInterface;

class ClientPlayer : public PlayerInterface
{
public:
	ClientPlayer(ConfigFile*, int id, unsigned uniqueId, PlayerType type, std::string name, std::string avatar, int sC, bool aS, bool sotS, int mB);
	~ClientPlayer() noexcept override;

	void setHand(HandInterface *) override;

	int getMyID() const override;
	void setMyUniqueID(unsigned newId) override;
	unsigned getMyUniqueID() const override;
	void setMyGuid(const std::string &theValue) override;
	std::string getMyGuid() const override;
	PlayerType getMyType() const override;

	void setMyDude(int theValue) override;
	int getMyDude() const override;

	void setMyDude4(int theValue) override;
	int getMyDude4() const override;

	void setMyName(const std::string& theValue) override;
	std::string getMyName() const override;

	void setMyAvatar(const std::string& theValue) override;
	std::string getMyAvatar() const override;

	void setMyCash(int theValue) override;
	int getMyCash() const override;

	void setMySet(int theValue) override;
	void setMySetAbsolute(int theValue) override;
	void setMySetNull() override;
	int getMySet() const override;
	int getMyLastRelativeSet() const override;

	void setMyAction(PlayerAction theValue, bool human) override;
	PlayerAction getMyAction() const override;

	void setMyButton(int theValue) override;
	int getMyButton() const override;

	void setMyActiveStatus(bool theValue) override;
	bool getMyActiveStatus() const override;

	void setMyStayOnTableStatus(bool theValue) override;
	bool getMyStayOnTableStatus() const override;

	void setMyCards(const std::array<int, 2> &theValue) override;
	void getMyCards(std::array<int, 2> &theValue) const override;

	void setMyTurn(bool theValue) override;
	bool getMyTurn() const override;

	void setMyCardsFlip(bool theValue, int state) override;
	bool getMyCardsFlip() const override;

	void setMyCardsValueInt(int theValue) override;
	int getMyCardsValueInt() const override;

	void setLogHoleCardsDone(bool theValue) override;
	bool getLogHoleCardsDone() const override;

	void setMyBestHandPosition(const std::array<int, 5> &theValue) override;
	void getMyBestHandPosition(std::array<int, 5> &theValue) const override;

	void setMyRoundStartCash(int theValue) override;
	int getMyRoundStartCash() const override;

	void setLastMoneyWon ( int theValue ) override;
	int getLastMoneyWon() const override;

	void setMyAverageSets(int theValue) override;
	int getMyAverageSets() const override;

	void setMyAggressive(bool theValue) override;
	int getMyAggressive() const override;

	void setSBluff (int theValue) override;
	int getSBluff() const override;

	void setSBluffStatus (bool theValue) override;
	bool getSBluffStatus() const override;

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

	int preflopCardsValue(int*);
	int flopCardsValue(int*);
	int turnCardsValue(int*);

	void readFile();

	void evaluation(int, int);

	void setIsSessionActive(bool active) override;
	bool isSessionActive() const override;
	void setIsKicked(bool kicked) override;
	bool isKicked() const override;
	void setIsMuted(bool muted) override;
	bool isMuted() const override;

	bool checkIfINeedToShowCards() override;

	void markRemoteAction() override {}
	unsigned getTimeSecSinceLastRemoteAction() const override
	{
		return 0;
	}

private:
	mutable boost::recursive_mutex m_syncMutex;

	ConfigFile *myConfig;
	HandInterface *currentHand;

	// Konstanten
	const int myID;
	unsigned myUniqueID;
	std::string myGuid;
	const PlayerType myType;
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

	bool myIsSessionActive;
	bool myIsKicked;
	bool myIsMuted;
};

#endif
