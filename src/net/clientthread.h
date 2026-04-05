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
/* Network client thread. */

#ifndef _CLIENTTHREAD_H_
#define _CLIENTTHREAD_H_

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <string>
#include <algorithm>
#include <numeric>
#include <atomic>
#include <mutex>
#include <openssl/ssl.h>

#include <core/thread.h>
#include <net/sessiondatacallback.h>
#include <guiinterface.h>
#include <serverdata.h>
#include <playerdata.h>
#include <gamedata.h>

constexpr size_t MAX_PLAYER_INFO_CACHE_SIZE = 500;

class ClientContext;
class ClientState;
class SenderHelper;
class DownloaderThread;
class Game;
class NetPacket;
class AvatarManager;
class Log;
class QtToolsInterface;
struct Gsasl;

#define SIZE_PING_BACKLOG		20

class PingData
{
public:
	PingData() : pingTimer(boost::posix_time::time_duration(0, 0, 0), boost::timers::portable::microsec_timer::manual_start) {}

	unsigned MinPing()
	{
		return pingValues.empty() ? 0 : *std::min_element(pingValues.begin(), pingValues.end());
	}
	unsigned MaxPing()
	{
		return pingValues.empty() ? 0 : *std::max_element(pingValues.begin(), pingValues.end());
	}
	unsigned AveragePing()
	{
		return pingValues.empty() ? 0 : (std::accumulate(pingValues.begin(), pingValues.end(), 0) / static_cast<unsigned>(pingValues.size()));
	}
	void StartPing()
	{
		pingTimer.start();
	}
	bool EndPing()
	{
		bool retVal = false;
		if (pingTimer.is_running()) {
			pingValues.push_back(static_cast<unsigned>(pingTimer.elapsed().total_milliseconds()));
			if (pingValues.size() > SIZE_PING_BACKLOG) {
				pingValues.pop_front();
			}
			pingTimer.reset();
			retVal = true;
		}
		return retVal;
	}

private:
	std::list<unsigned> pingValues;
	boost::timers::portable::microsec_timer pingTimer;
};

class ClientThread : public Thread, public boost::enable_shared_from_this<ClientThread>, public SessionDataCallback
{
public:
	ClientThread(GuiInterface &gui, AvatarManager &avatarManager, Log *myLog);
	virtual ~ClientThread() noexcept;

	// Set the parameters. Does not do any error checking.
	// Error checking will be done during connect
	// (i.e. after starting the thread).
	void Init(
		const std::string &serverAddress,
		const std::string &serverListUrl,
		const std::string &serverPassword,
		bool useServerList,
		unsigned serverPort,
		bool ipv6,
		bool sctp,
		bool tls,
		const std::string &avatarServerAddress,
		const std::string &playerName,
		const std::string &avatarFile,
		const std::string &cacheDir);
	virtual void SignalTermination() override;

	void SendKickPlayer(unsigned playerId);
	void SendLeaveCurrentGame();
	void SendStartEvent(bool fillUpWithCpuPlayers);
	void SendPlayerAction();
	void SendGameChatMessage(const std::string &msg);
	void SendLobbyChatMessage(const std::string &msg);
	void SendPrivateChatMessage(unsigned targetPlayerId, const std::string &msg);
	void SendJoinFirstGame(const std::string &password, bool autoLeave);
	void SendJoinGame(unsigned gameId, const std::string &password, bool autoLeave);
	void SendRejoinGame(unsigned gameId, bool autoLeave);
	void SendCreateGame(const GameData &gameData, const std::string &name, const std::string &password, bool autoLeave);
	void SendResetTimeout();
	void SendAskKickPlayer(unsigned playerId);
	void SendVoteKick(bool doKick);
	void SendShowMyCards();
	void SendInvitePlayerToCurrentGame(unsigned playerId);
	void SendRejectGameInvitation(unsigned gameId, DenyGameInvitationReason reason);
	void SendReportAvatar(unsigned reportedPlayerId, const std::string &avatarHash);
	void SendReportGameName(unsigned reportedGameId);
	void SendAdminRemoveGame(unsigned removeGameId);
	void SendAdminBanPlayer(unsigned playerId);

	void StartAsyncRead();
	virtual void CloseSession(boost::shared_ptr<SessionData> session) override;
	virtual void SessionError(boost::shared_ptr<SessionData> /*session*/, int /*errorCode*/) override {}
	virtual void SessionTimeoutWarning(boost::shared_ptr<SessionData> /*session*/, unsigned /*remainingSec*/) override {}
	virtual void HandlePacket(boost::shared_ptr<SessionData> session, boost::shared_ptr<NetPacket> packet) override;

	static void SslInfoCallback(const SSL *ssl, int where, int ret);

	void SelectServer(unsigned serverId);
	void SetLogin(const std::string &userName, const std::string &password);
	void SetServerPassword(const std::string &password);
	ServerInfo GetServerInfo(unsigned serverId) const;

	GameInfo GetGameInfo(unsigned gameId) const;
	PlayerInfo GetPlayerInfo(unsigned playerId) const;
	bool GetPlayerIdFromName(const std::string &playerName, unsigned &playerId) const;
	unsigned GetGameIdOfPlayer(unsigned playerId) const;
	ServerStats GetStatData() const;
	unsigned GetGameId() const;
	unsigned GetGuiPlayerId() const;
	int GetOrigGuiPlayerNum() const;

	Gsasl *GetAuthContext();

	ClientCallback &GetCallback();
	GuiInterface &GetGui();
	boost::shared_ptr<Log> GetClientLog();
	AvatarManager &GetAvatarManager();

protected:
	using GameInfoMap = std::map<unsigned, GameInfo>;
	using NetPacketList = std::list<boost::shared_ptr<NetPacket>>;
	using PlayerInfoMap = std::map<unsigned, PlayerInfo>;
	using AvatarFileMap = std::map<unsigned, boost::shared_ptr<AvatarFile>>;
	using ServerInfoMap = std::map<unsigned, ServerInfo>;
	struct LoginData {
		std::string userName;
		std::string password;
	};

	// Main function of the thread.
	virtual void Main() override;
	void RegisterTimers();
	void CancelTimers();

	void InitAuthContext();
	void ClearAuthContext();
	void InitGame();

	void SendSessionPacket(boost::shared_ptr<NetPacket> packet);
	void SendQueuedPackets();

	bool GetCachedPlayerInfo(unsigned id, PlayerInfo &info) const;
	void RequestPlayerInfo(unsigned id, bool requestAvatar = false);
	void RequestPlayerInfo(const std::list<unsigned> &idList, bool requestAvatar = false);
	void SetPlayerInfo(unsigned id, const PlayerInfo &info);
	void SetUnknownPlayer(unsigned id);
	void SetNewGameAdmin(unsigned id);
	void RetrieveAvatarIfNeeded(unsigned id, const PlayerInfo &info);
	std::string GetPlayerName(unsigned id);

	void AddTempAvatarFile(unsigned playerId, unsigned avatarSize, AvatarFileType type);
	void StoreInTempAvatarFile(unsigned playerId, const std::vector<unsigned char> &data);
	void CompleteTempAvatarFile(unsigned playerId);
	void PassAvatarFileToManager(unsigned playerId, boost::shared_ptr<AvatarFile> AvatarFile);
	void SetUnknownAvatar(unsigned playerId);

	void TimerCheckAvatarDownloads(const boost::system::error_code& ec);

	void UnsubscribeLobbyMsg();
	void ResubscribeLobbyMsg();

	const ClientContext &GetContext() const;
	ClientContext &GetContext();
	std::string GetCacheServerListFileName();
	void CreateContextSession();

	ClientState &GetState();
	void SetState(ClientState &newState);
	boost::asio::steady_timer &GetStateTimer();

	SenderHelper &GetSender();

	void SetGameId(unsigned id);
	const GameData &GetGameData() const;
	void SetGameData(const GameData &gameData);
	const StartData &GetStartData() const;
	void SetStartData(const StartData &startData);
	void SetGuiPlayerId(unsigned guiPlayerId);

	boost::shared_ptr<Game> GetGame();

	QtToolsInterface &GetQtToolsInterface();

	boost::shared_ptr<PlayerData> CreatePlayerData(unsigned playerId, bool isGameAdmin);
	void AddPlayerData(boost::shared_ptr<PlayerData> playerData);
	void RemovePlayerData(unsigned playerId, int removeReason);
	void ClearPlayerDataList();
	void MapPlayerDataList();
	const PlayerDataList GetPlayerDataList() const;
	boost::shared_ptr<PlayerData> GetPlayerDataByUniqueId(unsigned id);
	boost::shared_ptr<PlayerData> GetPlayerDataByName(const std::string &name);

	void AddServerInfo(unsigned serverId, const ServerInfo &info);
	void ClearServerInfoMap();
	bool GetSelectedServer(unsigned &serverId) const;
	void UseServer(unsigned serverId);

	bool GetLoginData(LoginData &loginData) const;

	void AddGameInfo(unsigned gameId, const GameInfo &info);
	void UpdateGameInfoMode(unsigned gameId, GameMode mode);
	void UpdateGameInfoAdmin(unsigned gameId, unsigned adminPlayerId);
	void RemoveGameInfo(unsigned gameId);
	void ModifyGameInfoAddPlayer(unsigned gameId, unsigned playerId);
	void ModifyGameInfoRemovePlayer(unsigned gameId, unsigned playerId);
	void ClearGameInfoMap();

	void StartPetition(unsigned petitionId, unsigned proposingPlayerId, unsigned kickPlayerId, int timeoutSec, int numVotesToKick);
	void UpdatePetition(unsigned petitionId, int numVotesAgainstKicking, int numVotesInFavourOfKicking, int numVotesToKick);
	void EndPetition(unsigned petitionId);

	void UpdateStatData(const ServerStats &stats);
	void EndPing();

	bool IsSessionEstablished() const;
	void SetSessionEstablished(bool flag);

	bool IsSynchronized() const;

	void ReadSessionGuidFromFile();
	void WriteSessionGuidToFile() const;

private:

	boost::shared_ptr<boost::asio::io_context> m_ioService;
	boost::shared_ptr<Log> m_clientLog;
	boost::shared_ptr<boost::asio::ssl::context> m_sslContext;

	Gsasl *m_authContext = nullptr;

	NetPacketList m_outPacketList;
	mutable boost::mutex m_outPacketListMutex;

	boost::shared_ptr<ClientContext> m_context;
	ClientState *m_curState;
	mutable boost::mutex m_curStateMutex;
	GuiInterface &m_gui;
	AvatarManager &m_avatarManager;

	boost::shared_ptr<SenderHelper> m_senderHelper;

	boost::shared_ptr<DownloaderThread> m_avatarDownloader;

	GameData m_gameData;
	StartData m_startData;
	PlayerDataList m_playerDataList;
	mutable boost::mutex m_playerDataListMutex;

	ServerInfoMap m_serverInfoMap;
	mutable boost::mutex m_serverInfoMapMutex;

	bool m_isServerSelected = false;
	unsigned m_selectedServerId = 0;
	mutable boost::mutex m_selectServerMutex;

	LoginData m_loginData;
	mutable boost::mutex m_loginDataMutex;

	GameInfoMap m_gameInfoMap;
	mutable boost::mutex m_gameInfoMapMutex;

	boost::shared_ptr<Game> m_game;
	mutable boost::mutex m_gameMutex;
	boost::shared_ptr<QtToolsInterface> myQtToolsInterface;

	PlayerInfoMap m_playerInfoMap;
	mutable boost::mutex m_playerInfoMapMutex;
	PlayerIdList m_playerInfoRequestList;
	mutable boost::mutex m_playerInfoRequestListMutex;
	PlayerIdList m_avatarShouldRequestList;
	mutable boost::mutex m_avatarShouldRequestListMutex;
	PlayerIdList m_avatarHasRequestedList;
	mutable boost::mutex m_avatarHasRequestedListMutex;

	unsigned m_curGameId = 0;
	mutable boost::mutex m_curGameIdMutex;

	unsigned m_curPetitionId = 0;
	mutable boost::mutex m_curPetitionIdMutex;

	AvatarFileMap m_tempAvatarMap;
	mutable boost::mutex m_tempAvatarMapMutex;

	unsigned m_curGameNum = 1;
	unsigned m_guiPlayerId = 0;
	mutable boost::mutex m_guiPlayerIdMutex;
	int m_origGuiPlayerNum = 0;
	std::atomic<bool> m_sessionEstablished{false};

	mutable boost::mutex m_curStatsMutex;
	ServerStats m_curStats;

	mutable boost::mutex m_pingDataMutex;
	PingData m_pingData;

	boost::asio::steady_timer m_stateTimer;
	boost::asio::steady_timer m_avatarTimer;

	friend class AbstractClientStateReceiving;
	friend class ClientStateInit;
	friend class ClientStateStartResolve;
	friend class ClientStateResolving;
	friend class ClientStateStartServerListDownload;
	friend class ClientStateSynchronizingServerList;
	friend class ClientStateDownloadingServerList;
	friend class ClientStateReadingServerList;
	friend class ClientStateWaitChooseServer;
	friend class ClientStateStartConnect;
	friend class ClientStateConnecting;
	friend class ClientStateStartSession;
	friend class ClientStateWaitEnterLogin;
	friend class ClientStateWaitAuthChallenge;
	friend class ClientStateWaitAuthVerify;
	friend class ClientStateWaitSession;
	friend class ClientStateWaitJoin;
	friend class ClientStateWaitGame;
	friend class ClientStateSynchronizeStart;
	friend class ClientStateWaitStart;
	friend class ClientStateWaitHand;
	friend class ClientStateRunHand;
	friend class ClientStateFinal;
};

#endif
