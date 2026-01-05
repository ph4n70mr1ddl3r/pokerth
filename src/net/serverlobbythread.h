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
/* Network server lobby thread. */

#ifndef _SERVERLOBBYTHREAD_H_
#define _SERVERLOBBYTHREAD_H_

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <boost/uuid/uuid_generators.hpp>

#include <net/sessionmanager.h>
#include <net/netpacket.h>
#include <db/serverdbcallback.h>
#include <gui/guiinterface.h>
#include <gamedata.h>

#define NET_LOBBY_THREAD_TERMINATE_TIMEOUT_MSEC		20000
#define NET_ADMIN_IRC_TERMINATE_TIMEOUT_MSEC		4000


class SenderHelper;
class InternalServerCallback;
class ServerGame;
class ServerBanManager;
class ConfigFile;
class AvatarManager;
class ChatCleanerManager;
class ServerDBInterface;
struct GameData;
class Game;
struct Gsasl;

class ServerLobbyThread : public Thread, public std::enable_shared_from_this<ServerLobbyThread>
{
public:
	ServerLobbyThread(GuiInterface &gui, ServerMode mode, ConfigFile &serverConfig, AvatarManager &avatarManager,
					  std::shared_ptr<boost::asio::io_context> ioService);
	virtual ~ServerLobbyThread();

	void Init(const std::string &logDir);
	virtual void SignalTermination();

	void AddConnection(std::shared_ptr<SessionData> sessionData);
	void ReAddSession(std::shared_ptr<SessionData> session, int reason, unsigned gameId);
	void MoveSessionToGame(std::shared_ptr<ServerGame> game, std::shared_ptr<SessionData> session, bool autoLeave, bool spectateOnly);
	void SessionError(std::shared_ptr<SessionData> session, int errorCode);
	void ResubscribeLobbyMsg(std::shared_ptr<SessionData> session);
	void NotifyPlayerJoinedLobby(unsigned playerId);
	void NotifyPlayerLeftLobby(unsigned playerId);
	void NotifyPlayerJoinedGame(unsigned gameId, unsigned playerId);
	void NotifyPlayerLeftGame(unsigned gameId, unsigned playerId);
	void NotifySpectatorJoinedGame(unsigned gameId, unsigned playerId);
	void NotifySpectatorLeftGame(unsigned gameId, unsigned playerId);
	void NotifyGameAdminChanged(unsigned gameId, unsigned newAdminPlayerId);
	void NotifyStartingGame(unsigned gameId);
	void NotifyReopeningGame(unsigned gameId);

	void DispatchPacket(std::shared_ptr<SessionData> session, std::shared_ptr<NetPacket> packet);
	void HandleGameRetrievePlayerInfo(std::shared_ptr<SessionData> session, const PlayerInfoRequestMessage &playerInfoRequest);
	void HandleGameRetrieveAvatar(std::shared_ptr<SessionData> session, const AvatarRequestMessage &retrieveAvatar);
	void HandleGameReportGame(std::shared_ptr<SessionData> session, const ReportGameMessage &reportGame);
	void HandleChatRequest(std::shared_ptr<SessionData> session, const ChatRequestMessage &chatRequest);
	void HandleAdminRemoveGame(std::shared_ptr<SessionData> session, const AdminRemoveGameMessage &removeGame);
	void HandleAdminBanPlayer(std::shared_ptr<SessionData> session, const AdminBanPlayerMessage &banPlayer);

	bool KickPlayerByName(const std::string &playerName);
	bool RemoveGameByPlayerName(const std::string &playerName);
	std::string GetPlayerIPAddress(const std::string &playerName) const;
	std::string GetPlayerNameFromId(unsigned playerId) const;
	void RemovePlayer(unsigned playerId, unsigned errorCode);
	void MutePlayerInGame(unsigned playerId);

	void SendGlobalChat(const std::string &message);
	void SendGlobalMsgBox(const std::string &message);
	void SendChatBotMsg(const std::string &message);
	void SendChatBotMsg(unsigned gameId, const std::string &message);
	void ReconnectChatBot();

	void AddComputerPlayer(std::shared_ptr<PlayerData> player);
	void RemoveComputerPlayer(std::shared_ptr<PlayerData> player);

	bool SendToLobbyPlayer(unsigned playerId, std::shared_ptr<NetPacket> packet);

	u_int32_t GetNextSessionId();
	u_int32_t GetNextUniquePlayerId();
	u_int32_t GetNextGameId();
	ServerCallback &GetCallback();

	AvatarManager &GetAvatarManager();
	ChatCleanerManager &GetChatCleaner();

	ServerStats GetStats() const;
	boost::posix_time::ptime GetStartTime() const;
	ServerMode GetServerMode() const;

	SenderHelper &GetSender();
	boost::asio::io_context &GetIOService();
	std::shared_ptr<ServerDBInterface> GetDatabase();
	ServerBanManager &GetBanManager();

	SessionDataCallback &GetSessionDataCallback();

protected:

	typedef std::deque<std::shared_ptr<boost::asio::ip::tcp::socket> > ConnectQueue;
	typedef std::list<std::shared_ptr<SessionData> > SessionList;
	typedef std::list<SessionId> SessionIdList;
	typedef std::map<SessionId, boost::timers::portable::microsec_timer> TimerSessionMap;
	typedef std::map<unsigned, std::shared_ptr<ServerGame> > GameMap;
	typedef std::map<std::string, boost::timers::portable::microsec_timer> TimerClientAddressMap;
	typedef std::list<unsigned> RemoveGameList;

	// Main function of the thread.
	virtual void Main();
	void RegisterTimers();
	void CancelTimers();
	void InitAuthContext();
	void ClearAuthContext();
	void InitChatCleaner();

	void HandlePacket(std::shared_ptr<SessionData> session, std::shared_ptr<NetPacket> packet);
	void HandleNetPacketInit(std::shared_ptr<SessionData> session, const InitMessage &initMessage);
	void HandleNetPacketAuthClientResponse(std::shared_ptr<SessionData> session, const AuthClientResponseMessage &clientResponse);
	void HandleNetPacketAvatarHeader(std::shared_ptr<SessionData> session, const AvatarHeaderMessage &avatarHeader);
	void HandleNetPacketUnknownAvatar(std::shared_ptr<SessionData> session, const UnknownAvatarMessage &unknownAvatar);
	void HandleNetPacketAvatarFile(std::shared_ptr<SessionData> session, const AvatarDataMessage &avatarData);
	void HandleNetPacketAvatarEnd(std::shared_ptr<SessionData> session, const AvatarEndMessage &avatarEnd);
	void HandleNetPacketRetrievePlayerInfo(std::shared_ptr<SessionData> session, const PlayerInfoRequestMessage &playerInfoRequest);
	void HandleNetPacketRetrieveAvatar(std::shared_ptr<SessionData> session, const AvatarRequestMessage &retrieveAvatar);
	void HandleNetPacketCreateGame(std::shared_ptr<SessionData> session, const JoinNewGameMessage &newGame);
	void HandleNetPacketJoinGame(std::shared_ptr<SessionData> session, const JoinExistingGameMessage &joinGame);
	void HandleNetPacketRejoinGame(std::shared_ptr<SessionData> session, const RejoinExistingGameMessage &rejoinGame);
	void HandleNetPacketChatRequest(std::shared_ptr<SessionData> session, const ChatRequestMessage &chatRequest);
	void HandleNetPacketRejectGameInvitation(std::shared_ptr<SessionData> session, const RejectGameInvitationMessage &reject);
	void HandleNetPacketReportGame(std::shared_ptr<SessionData> session, const ReportGameMessage &report);
	void HandleNetPacketAdminRemoveGame(std::shared_ptr<SessionData> session, const AdminRemoveGameMessage &removeGame);
	void HandleNetPacketAdminBanPlayer(std::shared_ptr<SessionData> session, const AdminBanPlayerMessage &banPlayer);
	void AuthChallenge(std::shared_ptr<SessionData> session, const std::string &secret);
	void CheckAvatarBlacklist(std::shared_ptr<SessionData> session);
	void AvatarBlacklisted(unsigned playerId);
	void AvatarOK(unsigned playerId);
	void InitAfterLogin(std::shared_ptr<SessionData> session);
	void EstablishSession(std::shared_ptr<SessionData> session);
	void AuthenticatePlayer(std::shared_ptr<SessionData> session);
	void UserValid(unsigned playerId, const DBPlayerData &dbPlayerData);
	void UserInvalid(unsigned playerId);
	void UserBlocked(unsigned playerId);

	void SendReportAvatarResult(unsigned byPlayerId, unsigned reportedPlayerId, bool success);
	void SendReportGameResult(unsigned byPlayerId, unsigned reportedGameId, bool success);
	void SendAdminBanPlayerResult(unsigned byPlayerId, unsigned reportedPlayerId, bool success);
	void RequestPlayerAvatar(std::shared_ptr<SessionData> session);
	void TimerRemoveGame(const boost::system::error_code &ec);
	void TimerRemovePlayer(const boost::system::error_code &ec);
	void TimerUpdateClientLoginLock(const boost::system::error_code &ec);
	void TimerCleanupAvatarCache(const boost::system::error_code &ec);

	bool IsGameNameInUse(const std::string &gameName) const;
	std::shared_ptr<ServerGame> InternalGetGameFromId(unsigned gameId);
	void InternalAddGame(std::shared_ptr<ServerGame> game);
	void InternalRemoveGame(std::shared_ptr<ServerGame> game);
	void InternalRemovePlayer(unsigned playerId, unsigned errorCode);
	void InternalMutePlayerInGame(unsigned playerId);
	void InternalResubscribeMsg(std::shared_ptr<SessionData> session);

	void HandleReAddedSession(std::shared_ptr<SessionData> session);

	void SessionTimeoutWarning(std::shared_ptr<SessionData> session, unsigned remainingSec);

	void CleanupSessionMap();

	void CloseSession(std::shared_ptr<SessionData> session);
	void SendError(std::shared_ptr<SessionData> s, int errorCode);
	void SendJoinGameFailed(std::shared_ptr<SessionData> s, unsigned gameId, int reason);
	void SendPlayerList(std::shared_ptr<SessionData> s);
	void SendGameList(std::shared_ptr<SessionData> s);
	void UpdateStatisticsNumberOfPlayers();
	void BroadcastStatisticsUpdate(const ServerStats &stats);

	void ReadStatisticsFile();
	void TimerSaveStatisticsFile(const boost::system::error_code &ec);

	InternalServerCallback &GetSenderCallback();
	GuiInterface &GetGui();

	unsigned GetPlayerId(const std::string &name) const;

	static std::shared_ptr<NetPacket> CreateNetPacketPlayerListNew(unsigned playerId);
	static std::shared_ptr<NetPacket> CreateNetPacketPlayerListLeft(unsigned playerId);
	static std::shared_ptr<NetPacket> CreateNetPacketGameListNew(const ServerGame &game);
	static std::shared_ptr<NetPacket> CreateNetPacketGameListUpdate(unsigned gameId, GameMode mode);

	u_int32_t GetRejoinGameIdForPlayer(const std::string &playerName, const std::string &guid, unsigned &outPlayerUniqueId);

	// Rate limiter for chat messages to prevent spam
	class ChatRateLimiter {
	public:
		ChatRateLimiter(unsigned maxMessages = 5, unsigned windowSec = 1)
			: m_maxMessages(maxMessages), m_windowSec(windowSec) {}

		bool IsAllowed(unsigned playerId) {
			boost::mutex::scoped_lock lock(m_mutex);
			auto now = boost::posix_time::microsec_clock::universal_time();
			auto &entry = m_playerMap[playerId];

			// Remove old entries outside the window
			auto windowStart = now - boost::posix_time::seconds(m_windowSec);
			auto it = entry.begin();
			while (it != entry.end() && *it < windowStart) {
				it = entry.erase(it);
			}

			// Check if under limit
			if (entry.size() >= m_maxMessages) {
				return false;
			}

			// Record this message
			entry.push_back(now);
			return true;
		}

	private:
		unsigned m_maxMessages;
		unsigned m_windowSec;
		std::map<unsigned, std::list<boost::posix_time::ptime>> m_playerMap;
		boost::mutex m_mutex;
	};

private:

	std::shared_ptr<boost::asio::io_context> m_ioService;

	std::shared_ptr<InternalServerCallback> m_internalServerCallback;
	std::shared_ptr<SenderHelper> m_sender;

	SessionManager m_sessionManager;
	SessionManager m_gameSessionManager;

	Gsasl *m_authContext;

	TimerClientAddressMap m_timerClientAddressMap;
	mutable boost::mutex m_timerClientAddressMapMutex;

	RemoveGameList m_removeGameList;
	mutable boost::mutex m_removeGameListMutex;

	PlayerDataMap m_computerPlayers;
	mutable boost::mutex m_computerPlayersMutex;

	GameMap m_gameMap;

	GuiInterface &m_gui;
	AvatarManager &m_avatarManager;

	const ServerMode m_mode;
	std::string m_statisticsFileName;
	ConfigFile &m_serverConfig;
	u_int32_t m_curGameId;

	u_int32_t m_curUniquePlayerId;
	u_int32_t m_curSessionId;
	mutable boost::mutex m_curUniquePlayerIdMutex;

	ServerStats m_statData;
	bool m_statDataChanged;
	mutable boost::mutex m_statMutex;

	std::shared_ptr<ServerBanManager> m_banManager;
	std::shared_ptr<ChatCleanerManager> m_chatCleanerManager;
	std::shared_ptr<ServerDBInterface> m_database;

	ChatRateLimiter m_chatRateLimiter;

	boost::asio::steady_timer m_removeGameTimer;
	boost::asio::steady_timer m_saveStatisticsTimer;
	boost::asio::steady_timer m_loginLockTimer;

	boost::uuids::random_generator m_sessionIdGenerator;

	const boost::posix_time::ptime m_startTime;

	friend class InternalServerCallback;
};

#endif
