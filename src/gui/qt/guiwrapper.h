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
#ifndef GUIWRAPPER_H
#define GUIWRAPPER_H

#include <guiinterface.h>

#include <string>
#include <memory>

class Session;
class gameTableImpl;
class startWindowImpl;
class guiLog;
class ConfigFile;


class GuiWrapper : public GuiInterface
{
public:
	GuiWrapper(ConfigFile*, startWindowImpl*);

	~GuiWrapper() noexcept override;

	void initGui(int speed) override;

	boost::shared_ptr<Session> getSession() override;
	void setSession(boost::shared_ptr<Session> session) override;

	gameTableImpl* getMyW() const override
	{
		return myW.get();
	}
	guiLog* getMyGuiLog() const override
	{
		return myGuiLog.get();
	}

	void refreshSet() const override;
	void refreshCash() const override;
	void refreshAction(int =-1, int =-1) const override;
	void refreshChangePlayer() const override;
	void refreshPot() const override;
	void refreshGroupbox(int =-1, int =-1) const override;
	void refreshAll() const override;
	void refreshPlayerName() const override;
	void refreshButton() const override;
	void refreshGameLabels(GameState state) const override;

	void setPlayerAvatar(int myUniqueID, const std::string &myAvatar) const override;

	void waitForGuiUpdateDone() const override;

	void dealBeRoCards(int myBeRoID) override;
	void dealHoleCards() override;
	void dealFlopCards() override;
	void dealTurnCard() override;
	void dealRiverCard() override;

	void nextPlayerAnimation() override;

	void beRoAnimation2(int) override;

	void preflopAnimation1() override;
	void preflopAnimation2() override;

	void flopAnimation1() override;
	void flopAnimation2() override;

	void turnAnimation1() override;
	void turnAnimation2() override;

	void riverAnimation1() override;
	void riverAnimation2() override;

	void postRiverAnimation1() override;
	void postRiverRunAnimation1() override;

	void flipHolecardsAllIn() override;

	void nextRoundCleanGui() override;

	void meInAction() override;
	void disableMyButtons() override;
	void updateMyButtonsState() override;
	void startTimeoutAnimation(int playerNum, int timeoutSec) override;
	void stopTimeoutAnimation(int playerNum) override;

	void startVoteOnKick(unsigned playerId, unsigned voteStarterPlayerId, int timeoutSec, int numVotesNeededToKick) override;
	void changeVoteOnKickButtonsState(bool showHide) override;
	void refreshVotesMonitor(int currentVotes, int numVotesNeededToKick) override;
	void endVoteOnKick() override;

	void logPlayerActionMsg(std::string playerName, int action, int setValue) override;
	void logNewGameHandMsg(int gameID, int handID) override;
	void logNewBlindsSetsMsg(int sbSet, int bbSet, std::string sbName, std::string bbName) override;
	void logPlayerWinsMsg(std::string playerName, int pot, bool main) override;
	void logPlayerSitsOut(std::string playerName) override;
	void logDealBoardCardsMsg(int roundID, int card1, int card2, int card3, int card4 = -1, int card5 = -1) override;
	void logFlipHoleCardsMsg(std::string playerName, int card1, int card2, int cardsValueInt = -1, std::string showHas = "shows") override;
	void logPlayerWinGame(std::string playerName, int gameID) override;
	void flushLogAtGame(int gameID) override;
	void flushLogAtHand() override;

	void SignalNetClientConnect(int actionID) override;
	void SignalNetClientServerListAdd(unsigned serverId) override;
	void SignalNetClientServerListShow() override;
	void SignalNetClientServerListClear() override;
	void SignalNetClientLoginShow() override;
	void SignalNetClientRejoinPossible(unsigned gameId) override;
	void SignalNetClientPostRiverShowCards(unsigned playerId) override;
	void SignalNetClientGameInfo(int actionID) override;
	void SignalNetClientError(int errorID, int osErrorID) override;
	void SignalNetClientNotification(int notificationId) override;
	void SignalNetClientStatsUpdate(const ServerStats &stats) override;
	void SignalNetClientPingUpdate(unsigned minPing, unsigned avgPing, unsigned maxPing) override;
	void SignalNetClientShowTimeoutDialog(NetTimeoutReason reason, unsigned remainingSec) override;
	void SignalNetClientRemovedFromGame(int notificationId) override;
	void SignalNetClientSelfJoined(unsigned playerId, const std::string &playerName, bool isGameAdmin) override;
	void SignalNetClientPlayerJoined(unsigned playerId, const std::string &playerName, bool isGameAdmin) override;
	void SignalNetClientPlayerChanged(unsigned playerId, const std::string &newPlayerName) override;
	void SignalNetClientPlayerLeft(unsigned playerId, const std::string &playerName, int removeReason) override;
	void SignalNetClientNewGameAdmin(unsigned playerId, const std::string &playerName) override;
	void SignalNetClientGameChatMsg(const std::string &playerName, const std::string &msg) override;
	void SignalNetClientLobbyChatMsg(const std::string &playerName, const std::string &msg) override;
	void SignalNetClientPrivateChatMsg(const std::string &playerName, const std::string &msg) override;
	void SignalNetClientMsgBox(const std::string &msg) override;
	void SignalNetClientMsgBox(unsigned msgId) override;

	void SignalNetClientWaitDialog() override;

	void SignalNetClientGameListNew(unsigned gameId) override;
	void SignalNetClientGameListRemove(unsigned gameId) override;
	void SignalNetClientGameListUpdateMode(unsigned gameId, GameMode mode) override;
	void SignalNetClientGameListUpdateAdmin(unsigned gameId, unsigned adminPlayerId) override;
	void SignalNetClientGameListPlayerJoined(unsigned gameId, unsigned playerId) override;
	void SignalNetClientGameListPlayerLeft(unsigned gameId, unsigned playerId) override;

	void SignalNetClientGameStart(boost::shared_ptr<Game> game) override;

	void SignalNetServerSuccess(int actionID) override;
	void SignalNetServerError(int errorID, int osErrorID) override;

	void SignalLobbyPlayerJoined(unsigned playerId, const std::string &nickName) override;
	void SignalLobbyPlayerKicked(const std::string &nickName, const std::string &byWhom, const std::string &reason) override;
	void SignalLobbyPlayerLeft(unsigned playerId) override;

	void SignalSelfGameInvitation(unsigned gameId, unsigned playerIdFrom) override;
	void SignalPlayerGameInvitation(unsigned gameId, unsigned playerIdWho, unsigned playerIdFrom) override;
	void SignalRejectedGameInvitation(unsigned gameId, unsigned playerIdWho, DenyGameInvitationReason reason) override;

private:

	std::unique_ptr<guiLog> myGuiLog;
	std::unique_ptr<gameTableImpl> myW;
	ConfigFile *myConfig;
	startWindowImpl *myStartWindow;

};

#endif
