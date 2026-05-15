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

#include "log.h"

#include "configfile.h"
#include "playerinterface.h"
#include "cardsvalue.h"
#include <core/loghelper.h>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDateTime>
#include <QDir>

#include <dirent.h>

using namespace std;

Log::Log(ConfigFile *c) : mySqliteLogDb(), myConnectionName(), mySqliteLogFileName(""), myConfig(c), uniqueGameID(0), currentHandID(0), currentRound(GAME_STATE_PREFLOP), sql("")
{
}

Log::~Log() noexcept
{
    // Acquire mutex to prevent data race with concurrent log operations
    std::lock_guard<std::recursive_mutex> lock(m_logMutex);
    // close Qt SQL database and remove connection
    if (mySqliteLogDb.isValid() && mySqliteLogDb.isOpen()) {
        mySqliteLogDb.close();
    }
    if (!myConnectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(myConnectionName);
    }
}

void
Log::init()
{
	std::lock_guard<std::recursive_mutex> lock(m_logMutex);
	sql.clear();

	if (mySqliteLogDb.isValid() && mySqliteLogDb.isOpen()) {
		mySqliteLogDb.close();
	}
	if (!myConnectionName.isEmpty()) {
		QSqlDatabase::removeDatabase(myConnectionName);
		myConnectionName.clear();
	}

	// SQLITE_LOG wird weiterhin als Konfig-Flag benutzt
	if(SQLITE_LOG) {

        // logging activated
        if(myConfig->readConfigInt("LogOnOff")) {

            DIR *logDir;
            logDir = opendir((myConfig->readConfigString("LogDir")).c_str());
            bool dirExists = logDir != nullptr;
            if (logDir)
                closedir(logDir);

            // check if logging path exist
            if(myConfig->readConfigString("LogDir") != "" && dirExists) {

                // detect current time
                char curDateTime[20];
                char curDate[11];
                char curTime[9];
                time_t now = time(nullptr);
                tm z;
                localtime_r(&now, &z);
                strftime(curDateTime,20,"%Y-%m-%d_%H%M%S",&z);
                strftime(curDate,11,"%Y-%m-%d",&z);
                strftime(curTime,9,"%H:%M:%S",&z);

                mySqliteLogFileName.clear();
                mySqliteLogFileName /= myConfig->readConfigString("LogDir");
                mySqliteLogFileName /= string("pokerth-log-") + curDateTime + ".pdb";

                myConnectionName = QString("pokerth_log_%1").arg(static_cast<qulonglong>(QDateTime::currentMSecsSinceEpoch()));
                mySqliteLogDb = QSqlDatabase::addDatabase("QSQLITE", myConnectionName);
                mySqliteLogDb.setDatabaseName(QString::fromStdString(mySqliteLogFileName.string()));

                if (mySqliteLogDb.open()) {

                    int i = 0;
                    // create session table
                    sql += "CREATE TABLE Session (";
                    sql += "PokerTH_Version TEXT NOT NULL";
                    sql += ",Date TEXT NOT NULL";
                    sql += ",Time TEXT NOT NULL";
                    sql += ",LogVersion INTEGER NOT NULL";
                    sql += ", PRIMARY KEY(Date,Time));";

                    // create game table
                    sql += "CREATE TABLE Game (";
                    sql += "UniqueGameID INTEGER PRIMARY KEY";
                    sql += ",GameID INTEGER NOT NULL";
                    sql += ",Startmoney INTEGER NOT NULL";
                    sql += ",StartSb INTEGER NOT NULL";
                    sql += ",DealerPos INTEGER NOT NULL";
                    sql += ",Winner_Seat INTEGER";
                    sql += ");";

                    // create player table
                    sql += "CREATE TABLE Player (";
                    sql += "UniqueGameID INTEGER NOT NULL";
                    sql += ",Seat INTEGER NOT NULL";
                    sql += ",Player TEXT NOT NULL";
                    sql += ",PRIMARY KEY(UniqueGameID,Seat));";

                    // create hand table
                    sql += "CREATE TABLE Hand (";
                    sql += "HandID INTEGER NOT NULL";
                    sql += ",UniqueGameID INTEGER NOT NULL";
                    sql += ",Dealer_Seat INTEGER";
                    sql += ",Sb_Amount INTEGER NOT NULL";
                    sql += ",Sb_Seat INTEGER NOT NULL";
                    sql += ",Bb_Amount INTEGER NOT NULL";
                    sql += ",Bb_Seat INTEGER NOT NULL";
                    for(i=1; i<=MAX_NUMBER_OF_PLAYERS; i++) {
                        sql += ",Seat_" + std::to_string(i) + "_Cash INTEGER";
                        sql += ",Seat_" + std::to_string(i) + "_Card_1 INTEGER";
                        sql += ",Seat_" + std::to_string(i) + "_Card_2 INTEGER";
                        sql += ",Seat_" + std::to_string(i) + "_Hand_text TEXT";
                        sql += ",Seat_" + std::to_string(i) + "_Hand_int INTEGER";
                    }
                    for(i=1; i<=5; i++) {
                        sql += ",BoardCard_" + std::to_string(i) + " INTEGER";
                    }
                    sql += ",PRIMARY KEY(HandID,UniqueGameID));";

                    // create action table
                    sql += "CREATE TABLE Action (";
                    sql += "ActionID INTEGER PRIMARY KEY AUTOINCREMENT";
                    sql += ",HandID INTEGER NOT NULL";
                    sql += ",UniqueGameID INTEGER NOT NULL";
                    sql += ",BeRo INTEGER NOT NULL";
                    sql += ",Player INTEGER NOT NULL";
                    sql += ",Action TEXT NOT NULL";
                    sql += ",Amount INTEGER";
                    sql += ");";

                    exec_transaction();

                    // Insert session metadata AFTER tables are created
                    {
                        QSqlQuery sessionQuery(mySqliteLogDb);
                        sessionQuery.prepare("INSERT INTO Session (PokerTH_Version, Date, Time, LogVersion) VALUES (?, ?, ?, ?)");
                        sessionQuery.addBindValue(QString::fromUtf8(POKERTH_BETA_RELEASE_STRING));
                        sessionQuery.addBindValue(QString::fromUtf8(curDate));
                        sessionQuery.addBindValue(QString::fromUtf8(curTime));
                        sessionQuery.addBindValue(SQLITE_LOG_VERSION);
                        if (!sessionQuery.exec()) {
                            QSqlError err = sessionQuery.lastError();
                            LOG_ERROR("Failed to insert session: " << err.text().toStdString());
                        }
                    }

                } else {
                    // open failed
                    QSqlError err = mySqliteLogDb.lastError();
                    LOG_ERROR("Failed to open sqlite (Qt): " << err.text().toStdString());
                }
            }
        }
    }
}

void
Log::logNewGameMsg(int gameID, int startCash, int startSmallBlind, unsigned dealerPosition, PlayerList seatsList)
{
	std::lock_guard<std::recursive_mutex> lock(m_logMutex);
	uniqueGameID++;

	if(SQLITE_LOG) {

		if(myConfig->readConfigInt("LogOnOff")) {
			//if write logfiles is enabled

			PlayerListConstIterator it_c;

			if( mySqliteLogDb.isValid() && mySqliteLogDb.isOpen() ) {
				QSqlQuery gameQuery(mySqliteLogDb);
				gameQuery.prepare("INSERT INTO Game (UniqueGameID, GameID, Startmoney, StartSb, DealerPos) VALUES (?, ?, ?, ?, ?)");
				gameQuery.addBindValue(static_cast<qulonglong>(uniqueGameID));
				gameQuery.addBindValue(gameID);
				gameQuery.addBindValue(startCash);
				gameQuery.addBindValue(startSmallBlind);
				gameQuery.addBindValue(dealerPosition);
				if (!gameQuery.exec()) {
					QSqlError err = gameQuery.lastError();
					LOG_ERROR("Failed to insert game: " << err.text().toStdString());
				}

				int i = 1;
				for(it_c = seatsList->begin(); it_c!=seatsList->end(); ++it_c) {
					if((*it_c)->getMyActiveStatus()) {
						QString playerName = QString::fromStdString((*it_c)->getMyName());
						QSqlQuery playerQuery(mySqliteLogDb);
						playerQuery.prepare("INSERT INTO Player (UniqueGameID, Seat, Player) VALUES (?, ?, ?)");
						playerQuery.addBindValue(static_cast<qulonglong>(uniqueGameID));
						playerQuery.addBindValue(i);
						playerQuery.addBindValue(playerName);
						if (!playerQuery.exec()) {
							QSqlError err = playerQuery.lastError();
							LOG_ERROR("Failed to insert player: " << err.text().toStdString());
						}
					}
					i++;
				}

				exec_transaction();
			}
		}
	}
}

void
Log::logNewHandMsg(int handID, unsigned dealerPosition, int smallBlind, unsigned smallBlindPosition, int bigBlind, unsigned bigBlindPosition, PlayerList seatsList)
{
	std::lock_guard<std::recursive_mutex> lock(m_logMutex);

	currentRound = GAME_STATE_PREFLOP;
	currentHandID = handID;
	PlayerListConstIterator it_c;
	for(it_c = seatsList->begin(); it_c!=seatsList->end(); ++it_c) {
		(*it_c)->setLogHoleCardsDone(false);
	}

	if(SQLITE_LOG) {

		if(myConfig->readConfigInt("LogOnOff")) {
			//if write logfiles is enabled

			if( mySqliteLogDb.isValid() && mySqliteLogDb.isOpen() ) {
				QString insertSql = "INSERT INTO Hand (HandID, UniqueGameID, Dealer_Seat, Sb_Amount, Sb_Seat, Bb_Amount, Bb_Seat";
				for(int i=1; i<=MAX_NUMBER_OF_PLAYERS; i++) {
					insertSql += QString(", Seat_%1_Cash").arg(i);
				}
				insertSql += ") VALUES (?, ?, ?, ?, ?, ?, ?";
				for(int i=1; i<=MAX_NUMBER_OF_PLAYERS; i++) {
					insertSql += ", ?";
				}
				insertSql += ")";

				QSqlQuery handQuery(mySqliteLogDb);
				handQuery.prepare(insertSql);
				handQuery.addBindValue(currentHandID);
				handQuery.addBindValue(static_cast<qulonglong>(uniqueGameID));
				handQuery.addBindValue(dealerPosition);
				handQuery.addBindValue(smallBlind);
				handQuery.addBindValue(smallBlindPosition);
				handQuery.addBindValue(bigBlind);
				handQuery.addBindValue(bigBlindPosition);
				for(it_c = seatsList->begin(); it_c!=seatsList->end(); ++it_c) {
					if((*it_c)->getMyActiveStatus()) {
						handQuery.addBindValue((*it_c)->getMyRoundStartCash());
					} else {
						handQuery.addBindValue(QVariant(QVariant::LongLong));
					}
				}
				for(int i = static_cast<int>(seatsList->size()); i < MAX_NUMBER_OF_PLAYERS; i++) {
					handQuery.addBindValue(QVariant(QVariant::LongLong));
				}
				if (!handQuery.exec()) {
					QSqlError err = handQuery.lastError();
					LOG_ERROR("Failed to insert hand: " << err.text().toStdString());
				}

				if(myConfig->readConfigInt("LogInterval") == 0) {
					exec_transaction();
				}

				// !! TODO !! Hack, because button rule is still wrong and dealerPosition sometimes has wrong ID (HeadsUp: dealerPosition=bigBlindPosition <-- wrong)
				bool dealerButtonOnTable = false;
				int countActivePlayer = 0;
				for(it_c = seatsList->begin(); it_c!=seatsList->end(); ++it_c) {
					if((*it_c)->getMyActiveStatus()) {
						countActivePlayer++;
						if((*it_c)->getMyButton()==BUTTON_DEALER && (*it_c)->getMyActiveStatus()) {
							dealerButtonOnTable = true;
						}
					}
				}
				if(countActivePlayer==2) {
					logPlayerActionUnlocked(smallBlindPosition,LOG_ACTION_DEALER);
				} else {
					if(dealerButtonOnTable) {
						logPlayerActionUnlocked(dealerPosition,LOG_ACTION_DEALER);
					}
				}

				// log blinds
				for(it_c = seatsList->begin(); it_c!=seatsList->end(); ++it_c) {
					if((*it_c)->getMyButton() == BUTTON_SMALL_BLIND && (*it_c)->getMySet()>0) {
						logPlayerActionUnlocked(smallBlindPosition,LOG_ACTION_SMALL_BLIND,(*it_c)->getMySet());
					}
				}
				for(it_c = seatsList->begin(); it_c!=seatsList->end(); ++it_c) {
					if((*it_c)->getMyButton() == BUTTON_BIG_BLIND && (*it_c)->getMySet()>0) {
						logPlayerActionUnlocked(bigBlindPosition,LOG_ACTION_BIG_BLIND,(*it_c)->getMySet());
					}
				}

				// (*it_c)->getMySet() ist ein Hack, da es im Internetspiel vorkam, dass ein Spieler zweimal geloggt wurde mit Blind - einmal jedoch mit $0

				// !! TODO !! Hack

			}
		}
	}
}

void
Log::logPlayerAction(string playerName, PlayerActionLog action, int amount)
{
	std::lock_guard<std::recursive_mutex> lock(m_logMutex);
	logPlayerActionUnlocked(playerName, action, amount);
}

void
Log::logPlayerAction(int seat, PlayerActionLog action, int amount)
{
	std::lock_guard<std::recursive_mutex> lock(m_logMutex);
	logPlayerActionUnlocked(seat, action, amount);
}

void
Log::logPlayerActionUnlocked(string playerName, PlayerActionLog action, int amount)
{
    if(SQLITE_LOG) {

        if(myConfig->readConfigInt("LogOnOff")) {
            //if write logfiles is enabled

            if( mySqliteLogDb.isValid() && mySqliteLogDb.isOpen() ) {
                // sqlite-db (Qt) is open

                // read seat using QSqlQuery
                QSqlQuery q(mySqliteLogDb);
                q.prepare(QString::fromUtf8("SELECT Seat FROM Player WHERE UniqueGameID = ? AND Player = ?"));
                q.addBindValue(uniqueGameID);
                q.addBindValue(QString::fromStdString(playerName));
                if(!q.exec()) {
                    QSqlError err = q.lastError();
                    LOG_ERROR("Error in statement: SELECT Seat ... [" << err.text().toStdString() << "]");
                } else {
                    if(q.next()) {
                        int seat = q.value(0).toInt();
                        logPlayerActionUnlocked(seat, action, amount);
                    } else {
                        LOG_ERROR("Implausible information about player " << playerName << " in log-db!");
                    }
                }
            }
        }
    }
}

void
Log::logPlayerActionUnlocked(int seat, PlayerActionLog action, int amount)
{
    if(SQLITE_LOG) {

        if(myConfig->readConfigInt("LogOnOff")) {
            //if write logfiles is enabled

            if( mySqliteLogDb.isValid() && mySqliteLogDb.isOpen() ) {
                if(action!=LOG_ACTION_NONE) {
                    std::string actionText;
                    bool hasAmount = false;
                    switch(action) {
                    case LOG_ACTION_DEALER:
                        actionText = "starts as dealer";
                        break;
                    case LOG_ACTION_SMALL_BLIND:
                        actionText = "posts small blind";
                        hasAmount = true;
                        break;
                    case LOG_ACTION_BIG_BLIND:
                        actionText = "posts big blind";
                        hasAmount = true;
                        break;
                    case LOG_ACTION_FOLD:
                        actionText = "folds";
                        break;
                    case LOG_ACTION_CHECK:
                        actionText = "checks";
                        break;
                    case LOG_ACTION_CALL:
                        actionText = "calls";
                        hasAmount = true;
                        break;
                    case LOG_ACTION_BET:
                        actionText = "bets";
                        hasAmount = true;
                        break;
                    case LOG_ACTION_ALL_IN:
                        actionText = "is all in with";
                        hasAmount = true;
                        break;
                    case LOG_ACTION_SHOW:
                        actionText = "shows";
                        break;
                    case LOG_ACTION_HAS:
                        actionText = "has";
                        break;
                    case LOG_ACTION_WIN:
                        actionText = "wins";
                        hasAmount = true;
                        break;
                    case LOG_ACTION_WIN_SIDE_POT:
                        actionText = "wins (side pot)";
                        hasAmount = true;
                        break;
                    case LOG_ACTION_SIT_OUT:
                        actionText = "sits out";
                        break;
                    case LOG_ACTION_WIN_GAME:
                        actionText = "wins game";
                        break;
                    case LOG_ACTION_LEFT:
                        actionText = "has left the game";
                        break;
                    case LOG_ACTION_KICKED:
                        actionText = "was kicked from the game";
                        break;
                    case LOG_ACTION_ADMIN:
                        actionText = "is game admin now";
                        break;
                    case LOG_ACTION_JOIN:
                        actionText = "has joined the game";
                        break;
                    default:
                        return;
                    }

                    QSqlQuery actionQuery(mySqliteLogDb);
                    actionQuery.prepare("INSERT INTO Action (HandID, UniqueGameID, BeRo, Player, Action, Amount) VALUES (?, ?, ?, ?, ?, ?)");
                    actionQuery.addBindValue(currentHandID);
                    actionQuery.addBindValue(static_cast<qulonglong>(uniqueGameID));
                    actionQuery.addBindValue(currentRound);
                    actionQuery.addBindValue(seat);
                    actionQuery.addBindValue(QString::fromStdString(actionText));
                    if (hasAmount) {
                        actionQuery.addBindValue(amount);
                    } else {
                        actionQuery.addBindValue(QVariant(QVariant::LongLong));
                    }
                    if (!actionQuery.exec()) {
                        QSqlError err = actionQuery.lastError();
                        LOG_ERROR("Failed to insert action: " << err.text().toStdString());
                    }

                     if(myConfig->readConfigInt("LogInterval") == 0) {
                         exec_transaction();
                     }
                }
            }
        }
    }
}

PlayerActionLog
Log::transformPlayerActionLog(PlayerAction action)
{
    switch(action) {
    case PLAYER_ACTION_FOLD:
        return LOG_ACTION_FOLD;
    case PLAYER_ACTION_CHECK:
        return LOG_ACTION_CHECK;
    case PLAYER_ACTION_CALL:
        return LOG_ACTION_CALL;
    case PLAYER_ACTION_BET:
    case PLAYER_ACTION_RAISE:
        return LOG_ACTION_BET;
    case PLAYER_ACTION_ALLIN:
        return LOG_ACTION_ALL_IN;
    default:
        return LOG_ACTION_NONE;
    }
}

void
Log::logBoardCards(std::array<int, 5> boardCards)
{
	std::lock_guard<std::recursive_mutex> lock(m_logMutex);

    if(SQLITE_LOG) {

        if(myConfig->readConfigInt("LogOnOff")) {
            //if write logfiles is enabled

            if( mySqliteLogDb.isValid() && mySqliteLogDb.isOpen() ) {
                // sqlite-db is open - use parameterized query to prevent SQL injection
                QString updateSql;
                switch(currentRound) {
                case GAME_STATE_FLOP:
                    updateSql = "UPDATE Hand SET BoardCard_1=?, BoardCard_2=?, BoardCard_3=? "
                                "WHERE UniqueGameID=? AND HandID=?";
                    break;
                case GAME_STATE_TURN:
                    updateSql = "UPDATE Hand SET BoardCard_4=? "
                                "WHERE UniqueGameID=? AND HandID=?";
                    break;
                case GAME_STATE_RIVER:
                    updateSql = "UPDATE Hand SET BoardCard_5=? "
                                "WHERE UniqueGameID=? AND HandID=?";
                    break;
                default:
                    return;
                }

                QSqlQuery boardQuery(mySqliteLogDb);
                boardQuery.prepare(updateSql);
                switch(currentRound) {
                case GAME_STATE_FLOP:
                    boardQuery.addBindValue(boardCards[0]);
                    boardQuery.addBindValue(boardCards[1]);
                    boardQuery.addBindValue(boardCards[2]);
                    boardQuery.addBindValue(static_cast<qulonglong>(uniqueGameID));
                    boardQuery.addBindValue(currentHandID);
                    break;
                case GAME_STATE_TURN:
                    boardQuery.addBindValue(boardCards[3]);
                    boardQuery.addBindValue(static_cast<qulonglong>(uniqueGameID));
                    boardQuery.addBindValue(currentHandID);
                    break;
                case GAME_STATE_RIVER:
                    boardQuery.addBindValue(boardCards[4]);
                    boardQuery.addBindValue(static_cast<qulonglong>(uniqueGameID));
                    boardQuery.addBindValue(currentHandID);
                    break;
                default:
                    return;
                }
                if (!boardQuery.exec()) {
                    QSqlError err = boardQuery.lastError();
                    LOG_ERROR("Failed to update board cards: " << err.text().toStdString());
                }
                if(myConfig->readConfigInt("LogInterval") == 0) {
                    exec_transaction();
                }
            }
        }
    }
}

void
Log::logHoleCardsHandName(PlayerList activePlayerList)
{
	std::lock_guard<std::recursive_mutex> lock(m_logMutex);

	PlayerListConstIterator it_c;

	for(it_c=activePlayerList->begin(); it_c!=activePlayerList->end(); ++it_c) {

		if( (*it_c)->getMyAction() != PLAYER_ACTION_FOLD && ( ((*it_c)->checkIfINeedToShowCards() && currentRound==GAME_STATE_POST_RIVER ) || ( currentRound!=GAME_STATE_POST_RIVER && !(*it_c)->getLogHoleCardsDone()) ) ) {

			logHoleCardsHandName(activePlayerList, *it_c);

		}
	}
}

void
Log::logHoleCardsHandName(PlayerList activePlayerList, boost::shared_ptr<PlayerInterface> player, bool forceExecLog)
{
	std::lock_guard<std::recursive_mutex> lock(m_logMutex);

	if(SQLITE_LOG) {

		if(myConfig->readConfigInt("LogOnOff")) {
			//if write logfiles is enabled

			if( mySqliteLogDb.isValid() && mySqliteLogDb.isOpen() ) {
                // sqlite-db (Qt) is open - use parameterized query to prevent SQL injection

				std::array<int, 2> myCards;
				player->getMyCards(myCards);
				int seatId = player->getMyID() + 1;

				QString setClause;
				QStringList bindValues;

				if(currentRound==GAME_STATE_POST_RIVER && player->getMyCardsValueInt()>0) {
					std::string handName = CardsValue::determineHandName(player->getMyCardsValueInt(),activePlayerList);
					// Sanitize hand name for SQL safety
					handName.erase(std::remove(handName.begin(), handName.end(), '"'), handName.end());
					handName.erase(std::remove(handName.begin(), handName.end(), '\''), handName.end());
					handName.erase(std::remove(handName.begin(), handName.end(), ';'), handName.end());
					setClause += QString("Seat_%1_Hand_text=?, Seat_%1_Hand_int=?")
						.arg(seatId);
					bindValues << QString::fromStdString(handName)
						<< player->getMyCardsValueInt();
				}

				if(!player->getLogHoleCardsDone()) {
					if(!setClause.isEmpty()) setClause += ", ";
					setClause += QString("Seat_%1_Card_1=?, Seat_%1_Card_2=?")
						.arg(seatId);
					bindValues << myCards[0] << myCards[1];
				}

				if(setClause.isEmpty()) {
					// Nothing to update
				} else {
					QString updateSql = QString("UPDATE Hand SET %1 WHERE UniqueGameID=? AND HandID=?")
						.arg(setClause);
					bindValues << static_cast<qulonglong>(uniqueGameID)
						<< currentHandID;

					QSqlQuery holeCardsQuery(mySqliteLogDb);
					holeCardsQuery.prepare(updateSql);
					for (const auto& val : bindValues) {
						holeCardsQuery.addBindValue(val);
					}
					if (!holeCardsQuery.exec()) {
						QSqlError err = holeCardsQuery.lastError();
						LOG_ERROR("Failed to update hole cards: " << err.text().toStdString());
					}
					if(myConfig->readConfigInt("LogInterval") == 0 || forceExecLog) {
						exec_transaction();
					}
				}

				if(!player->getLogHoleCardsDone()) {
					logPlayerActionUnlocked(player->getMyName(),LOG_ACTION_SHOW);
				} else {
					logPlayerActionUnlocked(player->getMyName(),LOG_ACTION_HAS);
				}

				player->setLogHoleCardsDone(true);

			}
		}
	}
}

void
Log::logHandWinner(PlayerList activePlayerList, int highestCardsValue, std::list<unsigned> winners)
{


	PlayerListConstIterator it_c;

	// log winner
	for(it_c=activePlayerList->begin(); it_c!=activePlayerList->end(); ++it_c) {
		if((*it_c)->getMyAction() != PLAYER_ACTION_FOLD && (*it_c)->getMyCardsValueInt() == highestCardsValue) {
			logPlayerAction((*it_c)->getMyName(),LOG_ACTION_WIN,(*it_c)->getLastMoneyWon());
		}
	}

	// log side pot winner
	for(it_c=activePlayerList->begin(); it_c!=activePlayerList->end(); ++it_c) {
		if((*it_c)->getMyAction() != PLAYER_ACTION_FOLD && (*it_c)->getMyCardsValueInt() != highestCardsValue ) {

			for(auto it_int = winners.begin(); it_int != winners.end(); ++it_int) {
				if((*it_int) == (*it_c)->getMyUniqueID()) {
					logPlayerAction((*it_c)->getMyName(),LOG_ACTION_WIN_SIDE_POT,(*it_c)->getLastMoneyWon());
				}
			}
		}
	}

}

void
Log::logGameWinner(PlayerList activePlayerList)
{

	int playersPositiveCashCounter = 0;
	PlayerListConstIterator it_c;
	for (it_c=activePlayerList->begin(); it_c!=activePlayerList->end(); ++it_c) {
		if ((*it_c)->getMyCash() > 0) playersPositiveCashCounter++;
	}
	if (playersPositiveCashCounter==1) {
		for (it_c=activePlayerList->begin(); it_c!=activePlayerList->end(); ++it_c) {
			if ((*it_c)->getMyCash() > 0) {
				logPlayerAction((*it_c)->getMyName(),LOG_ACTION_WIN_GAME);
			}
		}
		// for log after every game
		logAfterGame();
	}
}

void
Log::logPlayerSitsOut(PlayerList activePlayerList)
{

	PlayerListConstIterator it_c;

	for(it_c=activePlayerList->begin(); it_c!=activePlayerList->end(); ++it_c) {

		if((*it_c)->getMyCash() == 0) {
			logPlayerAction((*it_c)->getMyName(), LOG_ACTION_SIT_OUT);
		}
	}

}

void
Log::logAfterHand()
{
	std::lock_guard<std::recursive_mutex> lock(m_logMutex);
	if(myConfig->readConfigInt("LogInterval") == 1) {
		exec_transaction();
	}
}

void
Log::logAfterGame()
{
	std::lock_guard<std::recursive_mutex> lock(m_logMutex);
	if(myConfig->readConfigInt("LogInterval") == 2) {
		exec_transaction();
	}
}

void
Log::exec_transaction()
{
    // Execute accumulated SQL statements using QSqlQuery inside a Qt transaction.
    if(!(mySqliteLogDb.isValid() && mySqliteLogDb.isOpen())) {
        sql.clear();
        return;
    }

    QSqlError err;
    if(!mySqliteLogDb.transaction()) {
        err = mySqliteLogDb.lastError();
        LOG_ERROR("Failed to begin transaction: " << err.text().toStdString());
        // Try to execute without transaction fallback
    }

    // Split the SQL buffer by ';' and execute each statement separately
    std::string buf = sql;
    sql.clear();

    size_t start = 0;
    while(true) {
        size_t pos = buf.find(';', start);
        std::string stmt;
        if(pos == std::string::npos) {
            stmt = buf.substr(start);
        } else {
            stmt = buf.substr(start, pos - start);
        }
        // trim whitespace
        auto l = stmt.find_first_not_of(" \t\r\n");
        auto r = stmt.find_last_not_of(" \t\r\n");
        if(l != std::string::npos && r != std::string::npos && l <= r) {
            stmt = stmt.substr(l, r - l + 1);
            QSqlQuery q(mySqliteLogDb);
            if(!q.exec(QString::fromStdString(stmt))) {
                QSqlError qe = q.lastError();
                LOG_ERROR("Error in statement: " << stmt << " [" << qe.text().toStdString() << "]");
            }
        }
        if(pos == std::string::npos) break;
        start = pos + 1;
    }

    if(!mySqliteLogDb.commit()) {
        err = mySqliteLogDb.lastError();
        LOG_ERROR("Failed to commit transaction: " << err.text().toStdString());
    }
}

void
Log::setCurrentRound(GameState theValue)
{
	std::lock_guard<std::recursive_mutex> lock(m_logMutex);
	currentRound = theValue;
}
