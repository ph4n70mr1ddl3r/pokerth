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

#ifndef LOCALHAND_H
#define LOCALHAND_H

#include <enginefactory.h>
#include <guiinterface.h>
#include <boardinterface.h>
#include <playerinterface.h>
#include <handinterface.h>
#include <berointerface.h>
#include "localexception.h"
#include "engine_msg.h"

#include <vector>

class Log;

class LocalHand : public HandInterface
{
public:
	LocalHand(boost::shared_ptr<EngineFactory> f, GuiInterface*, boost::shared_ptr<BoardInterface>, Log*, PlayerList, PlayerList, PlayerList, int, int, unsigned, int, int);
	~LocalHand() noexcept override;

	void start() override;

	PlayerList getSeatsList() const override
	{
		return seatsList;
	}
	PlayerList getActivePlayerList() const override
	{
		return activePlayerList;
	}
	PlayerList getRunningPlayerList() const override
	{
		return runningPlayerList;
	}

	boost::shared_ptr<BoardInterface> getBoard() const override
	{
		return myBoard;
	}
	boost::shared_ptr<BeRoInterface> getPreflop() const override
	{
		return myBeRo[GAME_STATE_PREFLOP];
	}
	boost::shared_ptr<BeRoInterface> getFlop() const override
	{
		return myBeRo[GAME_STATE_FLOP];
	}
	boost::shared_ptr<BeRoInterface> getTurn() const override
	{
		return myBeRo[GAME_STATE_TURN];
	}
	boost::shared_ptr<BeRoInterface> getRiver() const override
	{
		return myBeRo[GAME_STATE_RIVER];
	}
	GuiInterface* getGuiInterface() const override
	{
		return myGui;
	}
	boost::shared_ptr<BeRoInterface> getCurrentBeRo() const override
	{
		if (currentRound >= myBeRo.size()) {
			throw LocalException(__FILE__, __LINE__, ERR_BERO_NOT_FOUND);
		}
		return myBeRo[currentRound];
	}

	Log* getLog() const override
	{
		return myLog;
	}

	void setMyID(int theValue) override
	{
		myID = theValue;
	}
	int getMyID() const override
	{
		return myID;
	}

	void setStartQuantityPlayers(int theValue) override
	{
		startQuantityPlayers = theValue;
	}
	int getStartQuantityPlayers() const override
	{
		return startQuantityPlayers;
	}

	void setCurrentRound(GameState theValue) override
	{
		currentRound = theValue;
		if(myLog) myLog->setCurrentRound(currentRound);
	}
	GameState getCurrentRound() const override
	{
		return currentRound;
	}
	GameState getRoundBeforePostRiver() const override
	{
		return roundBeforePostRiver;
	}

	void setDealerPosition(int theValue) override
	{
		dealerPosition = theValue;
	}
	int getDealerPosition() const override
	{
		return dealerPosition;
	}

	void setSmallBlind(int theValue) override
	{
		smallBlind = theValue;
	}
	int getSmallBlind() const override
	{
		return smallBlind;
	}

	void setAllInCondition(bool theValue) override
	{
		allInCondition = theValue;
	}
	bool getAllInCondition() const override
	{
		return allInCondition;
	}

	void setStartCash(int theValue) override
	{
		startCash = theValue;
	}
	int getStartCash() const override
	{
		return startCash;
	}

	void setPreviousPlayerID(unsigned theValue) override
	{
		previousPlayerID = theValue;
	}
	unsigned getPreviousPlayerID() const override
	{
		return previousPlayerID;
	}

	void setLastActionPlayerID ( unsigned theValue ) override;
	unsigned getLastActionPlayerID() const override
	{
		return lastActionPlayerID;
	}

	void setCardsShown(bool theValue) override
	{
		cardsShown = theValue;
	}
	bool getCardsShown() const override
	{
		return cardsShown;
	}

	void assignButtons();
	void setBlinds();

	void switchRounds() override;

protected:
	PlayerListIterator getSeatIt(unsigned) const override;
	PlayerListIterator getActivePlayerIt(unsigned) const override;
	PlayerListIterator getRunningPlayerIt(unsigned) const override;

private:

	boost::shared_ptr<EngineFactory> myFactory;
	GuiInterface *myGui;
	boost::shared_ptr<BoardInterface> myBoard;
	Log *myLog;

	PlayerList seatsList; // all player
	PlayerList activePlayerList; // all player who are not out
	PlayerList runningPlayerList; // all player who are not folded, not all in and not out

	std::vector<boost::shared_ptr<BeRoInterface> > myBeRo;

	int myID = 0;
	int startQuantityPlayers = 0;
	unsigned dealerPosition = 0;
	unsigned smallBlindPosition = 0;
	unsigned bigBlindPosition = 0;
	GameState currentRound = GAME_STATE_PREFLOP;
	GameState roundBeforePostRiver = GAME_STATE_PREFLOP;
	int smallBlind = 0;
	int startCash = 0;

	unsigned previousPlayerID = 0;
	unsigned lastActionPlayerID = 0;

	bool allInCondition = false;
	bool cardsShown = false;
};

#endif


