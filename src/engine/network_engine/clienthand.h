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

#ifndef CLIENTHAND_H
#define CLIENTHAND_H

#include <enginefactory.h>
#include <guiinterface.h>
#include <boardinterface.h>
#include <playerinterface.h>
#include <handinterface.h>
#include <berointerface.h>
#include <log.h>
#include <boost/thread.hpp>

#include <vector>

class ClientHand : public HandInterface
{
public:
	ClientHand ( boost::shared_ptr<EngineFactory> f, GuiInterface*, boost::shared_ptr<BoardInterface>, Log*, PlayerList, PlayerList, PlayerList , int, int, int, int, int );
	~ClientHand() noexcept override;

	void start() override;

	PlayerList getSeatsList() const override;
	PlayerList getActivePlayerList() const override;
	PlayerList getRunningPlayerList() const override;

	boost::shared_ptr<BoardInterface> getBoard() const override;
	boost::shared_ptr<BeRoInterface> getPreflop() const override;
	boost::shared_ptr<BeRoInterface> getFlop() const override;
	boost::shared_ptr<BeRoInterface> getTurn() const override;
	boost::shared_ptr<BeRoInterface> getRiver() const override;
	GuiInterface* getGuiInterface() const override;
	boost::shared_ptr<BeRoInterface> getCurrentBeRo() const override;

	Log* getLog() const override
	{
		return myLog;
	}

	void setMyID ( int theValue ) override;
	int getMyID() const override;

	void setCurrentQuantityPlayers ( int theValue );
	int getCurrentQuantityPlayers() const;

	void setStartQuantityPlayers ( int theValue ) override;
	int getStartQuantityPlayers() const override;

	void setCurrentRound ( GameState theValue ) override;
	GameState getCurrentRound() const override;
	GameState getRoundBeforePostRiver() const override;

	void setDealerPosition ( int theValue ) override;
	int getDealerPosition() const override;

	void setSmallBlind ( int theValue ) override;
	int getSmallBlind() const override;

	void setAllInCondition ( bool theValue ) override;
	bool getAllInCondition() const override;

	void setStartCash ( int theValue ) override;
	int getStartCash() const override;

	void setBettingRoundsPlayed ( int theValue );
	int getBettingRoundsPlayed() const;

	void setPreviousPlayerID ( unsigned theValue ) override;
	unsigned getPreviousPlayerID() const override;

	void setLastActionPlayerID ( unsigned theValue ) override;
	unsigned getLastActionPlayerID() const override;

	void setCardsShown ( bool theValue ) override;
	bool getCardsShown() const override;

	void switchRounds() override;

protected:
	PlayerListIterator getSeatIt(unsigned) const override;
	PlayerListIterator getActivePlayerIt(unsigned) const override;
	PlayerListIterator getRunningPlayerIt(unsigned) const override;


private:
	mutable boost::recursive_mutex m_syncMutex;

	boost::shared_ptr<EngineFactory> myFactory;
	GuiInterface *myGui;
	boost::shared_ptr<BoardInterface> myBoard;
	Log *myLog;

	PlayerList seatsList;
	PlayerList activePlayerList;
	PlayerList runningPlayerList;

	std::vector<boost::shared_ptr<BeRoInterface> > myBeRo;

	int myID = 0;
	int startQuantityPlayers = 0;
	unsigned dealerPosition = 0;
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


