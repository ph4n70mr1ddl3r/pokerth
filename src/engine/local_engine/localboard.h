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

#ifndef LOCALBOARD_H
#define LOCALBOARD_H

#include <iostream>
#include <vector>
#include <array>
#include <boost/shared_ptr.hpp>

#include <boardinterface.h>

class PlayerInterface;
class HandInterface;


class LocalBoard : public BoardInterface
{
public:
	LocalBoard();
	~LocalBoard() noexcept;

	void setPlayerLists(PlayerList, PlayerList, PlayerList);

	void setMyCards(const std::array<int, 5> &theValue)
	{
		myCards = theValue;
	}
	void getMyCards(std::array<int, 5> &theValue)
	{
		theValue = myCards;
	}

	void setAllInCondition(bool theValue)
	{
		allInCondition = theValue;
	}
	void setLastActionPlayerID(unsigned theValue)
	{
		lastActionPlayerID = theValue;
	}

	int getPot() const
	{
		return pot;
	}
	void setPot(int theValue)
	{
		pot = theValue;
	}
	int getSets() const
	{
		return sets;
	}
	void setSets(int theValue)
	{
		sets = theValue;
	}

	void collectSets() ;
	void collectPot() ;

	void distributePot(unsigned dealerPosition);
	void determinePlayerNeedToShowCards();

	std::list<unsigned> getWinners() const
	{
		return winners;
	}
	void setWinners(const std::list<unsigned> &w)
	{
		winners = w;
	}

	std::list<unsigned> getPlayerNeedToShowCards() const
	{
		return playerNeedToShowCards;
	}
	void setPlayerNeedToShowCards(const std::list<unsigned> &p)
	{
		playerNeedToShowCards = p;
	}


private:
	PlayerList seatsList;
	PlayerList activePlayerList;
	PlayerList runningPlayerList;

	std::list<unsigned> winners;
	std::list<unsigned> playerNeedToShowCards;

	std::array<int, 5> myCards{};
	int pot = 0;
	int sets = 0;
	bool allInCondition = false;
	unsigned lastActionPlayerID = 0;

};

#endif
