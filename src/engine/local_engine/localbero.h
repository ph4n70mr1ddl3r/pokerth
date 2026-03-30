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

#ifndef LOCALBERO_H
#define LOCALBERO_H


#include "game_defs.h"

#include "berointerface.h"
#include "handinterface.h"

class LocalBeRo : public BeRoInterface
{
public:
	LocalBeRo(HandInterface* hi, unsigned dP, int sB, GameState gS);
	~LocalBeRo() noexcept override;

	GameState getMyBeRoID() const override
	{
		return myBeRoID;
	}

	int getHighestCardsValue() const override;
	void setHighestCardsValue(int /*theValue*/) override { }

	void setMinimumRaise ( int theValue ) override
	{
		minimumRaise = theValue;
	}
	int getMinimumRaise() const override
	{
		return minimumRaise;
	}

	void setFullBetRule ( bool theValue ) override
	{
		fullBetRule = theValue;
	}
	bool getFullBetRule() const override
	{
		return fullBetRule;
	}

	void skipFirstRunGui() override
	{
		firstRunGui = false;
	}

	void nextPlayer() override;
	void run() override;

	void postRiverRun() override {};


protected:

	HandInterface* getMyHand() const
	{
		return myHand;
	}

	unsigned getDealerPosition() const
	{
		return dealerPosition;
	}

	void setCurrentPlayersTurnId(unsigned theValue)
	{
		currentPlayersTurnId = theValue;
	}
	unsigned getCurrentPlayersTurnId() const
	{
		return currentPlayersTurnId;
	}

	void setFirstRoundLastPlayersTurnId(unsigned theValue)
	{
		firstRoundLastPlayersTurnId = theValue;
	}
	unsigned getFirstRoundLastPlayersTurnId() const
	{
		return firstRoundLastPlayersTurnId;
	}

	void setCurrentPlayersTurnIt(PlayerListIterator theValue)
	{
		currentPlayersTurnIt = theValue;
	}
	PlayerListIterator getCurrentPlayersTurnIt() const
	{
		return currentPlayersTurnIt;
	}

	void setLastPlayersTurnIt(PlayerListIterator theValue)
	{
		lastPlayersTurnIt = theValue;
	}
	PlayerListIterator getLastPlayersTurnIt() const
	{
		return lastPlayersTurnIt;
	}

	void setHighestSet(int theValue)
	{
		highestSet = theValue;
	}
	int getHighestSet() const
	{
		return highestSet;
	}

	void setFirstRun(bool theValue)
	{
		firstRound = theValue;
	}
	bool getFirstRun() const
	{
		return firstRound;
	}

	void setFirstRound(bool theValue)
	{
		firstRound = theValue;
	}
	bool getFirstRound() const
	{
		return firstRound;
	}

	void setSmallBlindPositionId(unsigned theValue)
	{
		smallBlindPositionId = theValue;
	}
	unsigned getSmallBlindPositionId() const
	{
		return smallBlindPositionId;
	}

	void setBigBlindPositionId(unsigned theValue)
	{
		bigBlindPositionId = theValue;
	}
	unsigned getBigBlindPositionId() const
	{
		return bigBlindPositionId;
	}


	void setSmallBlindPosition(int theValue)
	{
		smallBlindPosition = theValue;
	}
	int getSmallBlindPosition() const
	{
		return smallBlindPosition;
	}

	void setSmallBlind(int theValue)
	{
		smallBlind = theValue;
	}
	int getSmallBlind() const
	{
		return smallBlind;
	}




private:

	HandInterface* myHand = nullptr;

	const GameState myBeRoID;
	unsigned dealerPosition = 0;
	int smallBlindPosition = 0;

	unsigned smallBlindPositionId = 0;
	unsigned bigBlindPositionId = 0;


	int smallBlind = 0;
	int highestSet = 0;
	int minimumRaise = 0;
	bool fullBetRule = false;

	bool firstRound = true;
	bool firstRunGui = false;
	bool firstHeadsUpRound = true;

	PlayerListIterator currentPlayersTurnIt;
	PlayerListIterator lastPlayersTurnIt;

	unsigned currentPlayersTurnId = 0;
	unsigned firstRoundLastPlayersTurnId = 0;

	bool logBoardCardsDone = false;


};

#endif
