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

#ifndef LOG_H
#define LOG_H

#include "engine_defs.h"
#include "game_defs.h"

#include <string>
#include <array>
#include <filesystem>
#include <mutex>

#include <QSqlDatabase>
#include <QString>

class ConfigFile;

class Log
{

public:
    Log(ConfigFile *c);

    ~Log() noexcept;

    void init();
    void logNewGameMsg(int gameID, int startCash, int startSmallBlind, unsigned dealerPosition, PlayerList seatsList);
    void logNewHandMsg(int handID, unsigned dealerPosition, int smallBlind, unsigned smallBlindPosition, int bigBlind, unsigned bigBlindPosition, PlayerList seatsList);
    void logPlayerAction(std::string playerName, PlayerActionLog action, int amount = 0);
    void logPlayerAction(int seat, PlayerActionLog action, int amount = 0);
    PlayerActionLog transformPlayerActionLog(PlayerAction action);
    void logBoardCards(std::array<int, 5> boardCards);
    void logHoleCardsHandName(PlayerList activePlayerList);
    void logHoleCardsHandName(PlayerList activePlayerList, boost::shared_ptr<PlayerInterface> player, bool forceExecLog = false);
    void logHandWinner(PlayerList activePlayerList, int highestCardsValue, std::list<unsigned> winners);
    void logGameWinner(PlayerList activePlayerList);
    void logPlayerSitsOut(PlayerList activePlayerList);
    void logAfterHand();
    void logAfterGame();

    void setCurrentRound(GameState theValue);

    [[nodiscard]] std::string getMySqliteLogFileName() const
    {
        return mySqliteLogFileName.string();
    }

private:

    void exec_transaction();
    void logPlayerActionUnlocked(std::string playerName, PlayerActionLog action, int amount = 0);
    void logPlayerActionUnlocked(int seat, PlayerActionLog action, int amount = 0);

    mutable std::recursive_mutex m_logMutex;
    QSqlDatabase mySqliteLogDb;
    QString myConnectionName;

    std::filesystem::path mySqliteLogFileName;
    ConfigFile *myConfig;
    int uniqueGameID;
    int currentHandID;
    GameState currentRound;
    std::string sql;
};

#endif // LOG_H
