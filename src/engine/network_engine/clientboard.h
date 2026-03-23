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

#ifndef CLIENTBOARD_H
#define CLIENTBOARD_H

#include <boardinterface.h>
#include <boost/thread.hpp>
#include <vector>
#include <array>


class PlayerInterface;
class HandInterface;


class ClientBoard : public BoardInterface
{
public:
	ClientBoard();
	~ClientBoard() noexcept override;

	void setPlayerLists(PlayerList, PlayerList, PlayerList) override;

	void setMyCards(const std::array<int, 5> &theValue) override;
	void getMyCards(std::array<int, 5> &theValue) override;

	int getPot() const override;
	void setPot(int theValue) override;
	int getSets() const override;
	void setSets(int theValue) override;

	void setAllInCondition(bool theValue) override;
	void setLastActionPlayerID(unsigned theValue) override;

	void collectSets() override;
	void collectPot() override;

	void distributePot(unsigned) override;
	void determinePlayerNeedToShowCards() override;

	std::list<unsigned> getWinners() const override;
	void setWinners(const std::list<unsigned> &winners) override;

	std::list<unsigned> getPlayerNeedToShowCards() const override;
	void setPlayerNeedToShowCards(const std::list<unsigned> &playerNeedToShowCards) override;

private:
	mutable boost::recursive_mutex m_syncMutex;

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
