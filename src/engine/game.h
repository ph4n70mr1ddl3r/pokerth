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

#ifndef GAME_H
#define GAME_H

#include <engine_defs.h>
#include "gamedata.h"
#include "playerdata.h"

#include <third_party/boost/timers.hpp>

class GuiInterface;
class Log;
class HandInterface;
class BoardInterface;
class EngineFactory;
struct GameData;
struct StartData;


class Game
{

public:
	Game(GuiInterface *gui, boost::shared_ptr<EngineFactory> factory,
		 const PlayerDataList &playerDataList, const GameData &gameData,
		 const StartData &startData, int gameId, Log *myLog);

	~Game() noexcept;

	void initHand();
	void startHand();

	[[nodiscard]] boost::shared_ptr<HandInterface> getCurrentHand();
	[[nodiscard]] const boost::shared_ptr<HandInterface> getCurrentHand() const;

	[[nodiscard]] PlayerList getSeatsList() const
	{
		return seatsList;
	}
	[[nodiscard]] PlayerList getActivePlayerList() const
	{
		return activePlayerList;
	}
	[[nodiscard]] PlayerList getRunningPlayerList() const
	{
		return runningPlayerList;
	}

	void setStartQuantityPlayers(int theValue)
	{
		startQuantityPlayers = theValue;
	}
	[[nodiscard]] int getStartQuantityPlayers() const
	{
		return startQuantityPlayers;
	}

	void setStartSmallBlind(int theValue)
	{
		startSmallBlind = theValue;
	}
	[[nodiscard]] int getStartSmallBlind() const
	{
		return startSmallBlind;
	}

	void setStartCash(int theValue)
	{
		startCash = theValue;
	}
	[[nodiscard]] int getStartCash() const
	{
		return startCash;
	}

	[[nodiscard]] int getMyGameID() const
	{
		return myGameID;
	}

	void setCurrentSmallBlind(int theValue)
	{
		currentSmallBlind = theValue;
	}
	[[nodiscard]] int getCurrentSmallBlind() const
	{
		return currentSmallBlind;
	}

	void setCurrentHandID(int theValue)
	{
		currentHandID = theValue;
	}
	[[nodiscard]] int getCurrentHandID() const
	{
		return currentHandID;
	}

	[[nodiscard]] unsigned getDealerPosition() const
	{
		return dealerPosition;
	}

	void replaceDealer(unsigned oldDealer, unsigned newDealer)
	{
		if (dealerPosition == oldDealer)
			dealerPosition = newDealer;
	}

	[[nodiscard]] boost::shared_ptr<PlayerInterface> getPlayerByUniqueId(unsigned id);
	[[nodiscard]] boost::shared_ptr<PlayerInterface> getPlayerByNumber(int number);
	[[nodiscard]] boost::shared_ptr<PlayerInterface> getPlayerByName(const std::string &name);
	[[nodiscard]] boost::shared_ptr<PlayerInterface> getCurrentPlayer();

	void raiseBlinds();

private:
	boost::shared_ptr<EngineFactory> myFactory;

	GuiInterface *myGui;
	Log *myLog;
	boost::shared_ptr<HandInterface> currentHand;
	boost::shared_ptr<BoardInterface> currentBoard;

	PlayerList seatsList;
	PlayerList activePlayerList; // used seats
	PlayerList runningPlayerList; // nonfolded and nonallin active players

	// start variables
	int startQuantityPlayers = 0;
	int startCash = 0;
	int startSmallBlind = 0;
	int myGameID = 0;

	int currentSmallBlind = 0;
	int currentHandID = 0;
	unsigned dealerPosition = 0;
	int lastHandBlindsRaised = 0;
	int lastTimeBlindsRaised = 0;
	const GameData myGameData;
	std::list<int> blindsList;

	//timer
	boost::timers::portable::second_timer blindsTimer;
};

#endif
