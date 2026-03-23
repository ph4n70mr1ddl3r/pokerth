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

#ifndef CLIENTBERO_H
#define CLIENTBERO_H

#include <boost/thread.hpp>
#include "berointerface.h"

class HandInterface;

/**
	@author FThauer FHammer <webmaster@pokerth.net>
*/
class ClientBeRo : public BeRoInterface
{
public:
	ClientBeRo(HandInterface* hi, unsigned dP, int sB, GameState gS);
	~ClientBeRo() noexcept override;

	GameState getMyBeRoID() const override;

	int getHighestCardsValue() const override;
	void setHighestCardsValue(int theValue) override;

	void setLastActionPlayer ( unsigned theValue );
	unsigned getLastActionPlayer() const;

	void setSmallBlindPositionId(unsigned theValue) override;
	unsigned getSmallBlindPositionId() const override;

	void setBigBlindPositionId(unsigned theValue) override;
	unsigned getBigBlindPositionId() const override;

	void setCurrentPlayersTurnId(unsigned theValue) override;
	unsigned getCurrentPlayersTurnId() const override;

	void setFirstRoundLastPlayersTurnId(unsigned theValue);
	unsigned getFirstRoundLastPlayersTurnId() const;

	void setCurrentPlayersTurnIt(PlayerListIterator theValue) override;
	PlayerListIterator getCurrentPlayersTurnIt() const override;

	void setLastPlayersTurnIt(PlayerListIterator theValue);
	PlayerListIterator getLastPlayersTurnIt() const;

	void setHighestSet(int theValue) override;
	int getHighestSet() const override;

	void setFirstRound(bool theValue);
	bool getFirstRound() const override;

	void setSmallBlindPosition(int theValue);
	int getSmallBlindPosition() const;

	void setSmallBlind(int theValue);
	int getSmallBlind() const;

	void setMinimumRaise ( int theValue ) override;
	int getMinimumRaise() const override;

	void setFullBetRule ( bool theValue ) override;
	bool getFullBetRule() const override;

	void skipFirstRunGui() override;

	void nextPlayer() override;
	void run() override;

	void postRiverRun() override;

private:

	const GameState myBeRoID;

	mutable boost::recursive_mutex m_syncMutex;

	HandInterface *myHand;

	int highestCardsValue = 0;

	PlayerListIterator currentPlayersTurnIt;
	PlayerListIterator lastPlayersTurnIt;

	unsigned smallBlindPositionId = 0;
	unsigned bigBlindPositionId = 0;

	unsigned currentPlayersTurnId = 0;
	unsigned firstRoundLastPlayersTurnId = 0;


	int highestSet = 0;
	bool firstRound = true;
	int smallBlindPosition = 0;
	int smallBlind = 0;

	int minimumRaise = 0;
	bool fullBetRule = false;

	int lastActionPlayer = 0;
};

#endif
