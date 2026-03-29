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

#include <net/clientthread.h>
#include <net/socket_helper.h>
#include <net/clientstate.h>
#include <net/clientcontext.h>
#include <net/senderhelper.h>
#include <net/downloaderthread.h>
#include <net/clientexception.h>
#include <net/socket_msg.h>
#include <net/net_helper.h>
#include <net/asioreceivebuffer.h>
#include <core/avatarmanager.h>
#include <core/loghelper.h>
#include <clientenginefactory.h>
#include <game.h>
#include <log.h>
#include <qttoolsinterface.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <filesystem>

#include <QDebug>
#include <sstream>
#include <fstream>
#include <memory>
#include <cassert>
#include <typeinfo>
#include <openssl/ssl.h>

#define TEMP_AVATAR_FILENAME	"avatar.tmp"
#define TEMP_GUID_FILENAME		"guid.tmp"
#define CLIENT_GUID_SIZE		16
#define CLIENT_AVATAR_LOOP_MSEC	100
#define CLIENT_SEND_LOOP_MSEC	50

using boost::asio::ip::tcp;

ClientThread::ClientThread(GuiInterface &gui, AvatarManager &avatarManager, Log *myLog)
	: m_ioService(new boost::asio::io_context), m_clientLog(myLog), m_curState(nullptr), m_gui(gui),
	  m_avatarManager(avatarManager), m_stateTimer(*m_ioService), m_avatarTimer(*m_ioService)
{
	m_context.reset(new ClientContext);
	myQtToolsInterface.reset(CreateQtToolsWrapper());
	m_senderHelper.reset(new SenderHelper(m_ioService));
}

ClientThread::~ClientThread() noexcept
{
}

void
ClientThread::Init(
	const string &serverAddress, const string &serverListUrl,
	const string &serverPassword,
	bool useServerList, unsigned serverPort, 
	bool ipv6, bool sctp, bool tls,
	const string &avatarServerAddress, const string &playerName,
	const string &avatarFile, const string &cacheDir)
{
	if (IsRunning()) {
		throw ClientException(__FILE__, __LINE__, ERR_SOCK_INVALID_STATE, 0);
	}

	ClientContext &context = GetContext();

	context.SetSctp(sctp);
	context.SetTls(tls);
	context.SetAddrFamily(ipv6 ? AF_INET6 : AF_INET);
	context.SetServerAddr(serverAddress);
	context.SetServerListUrl(serverListUrl);
	context.SetServerPassword(serverPassword);
	context.SetUseServerList(useServerList);
	context.SetServerPort(serverPort);
	context.SetAvatarServerAddr(avatarServerAddress);
	context.SetPlayerName(playerName);
	context.SetAvatarFile(avatarFile);
	context.SetCacheDir(cacheDir);

	ReadSessionGuidFromFile();
}

void
ClientThread::SignalTermination()
{
	Thread::SignalTermination();
	m_ioService->stop();
}

void
ClientThread::SendKickPlayer(unsigned playerId)
{
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_KickPlayerRequestMessage);
	KickPlayerRequestMessage *netKick = packet->GetMsg()->mutable_kickplayerrequestmessage();
	netKick->set_gameid(GetGameId());
	netKick->set_playerid(playerId);
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendLeaveCurrentGame()
{
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_LeaveGameRequestMessage);
	LeaveGameRequestMessage *netLeave = packet->GetMsg()->mutable_leavegamerequestmessage();
	netLeave->set_gameid(GetGameId());
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendStartEvent(bool fillUpWithCpuPlayers)
{
	// Warning: This function is called in the context of the GUI thread.
	// Create a network packet for the server start event.
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_StartEventMessage);
	StartEventMessage *netStartEvent = packet->GetMsg()->mutable_starteventmessage();
	netStartEvent->set_starteventtype(StartEventMessage::startEvent);
	netStartEvent->set_gameid(GetGameId());
	netStartEvent->set_fillwithcomputerplayers(fillUpWithCpuPlayers);
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendPlayerAction()
{
	// Warning: This function is called in the context of the GUI thread.
	// Create a network packet containing the current player action.
	{
		boost::mutex::scoped_lock lock(m_pingDataMutex);
		m_pingData.StartPing();
	}
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_MyActionRequestMessage);
	MyActionRequestMessage *netMyAction = packet->GetMsg()->mutable_myactionrequestmessage();
	netMyAction->set_gameid(GetGameId());
	auto game = GetGame();
	if (!game) {
		return;
	}
	auto seatsList = game->getSeatsList();
	if (!seatsList || seatsList->empty()) {
		return;
	}
	boost::shared_ptr<PlayerInterface> myPlayer = seatsList->front();
	if (!myPlayer) {
		return;
	}
	netMyAction->set_handnum(game->getCurrentHandID());
	netMyAction->set_gamestate(static_cast<NetGameState>(game->getCurrentHand()->getCurrentRound()));
	netMyAction->set_myaction(static_cast<NetPlayerAction>(myPlayer->getMyAction()));
	// Only send last bet if not fold/checked.
	if (myPlayer->getMyAction() != PLAYER_ACTION_FOLD && myPlayer->getMyAction() != PLAYER_ACTION_CHECK)
		netMyAction->set_myrelativebet(myPlayer->getMyLastRelativeSet());
	else
		netMyAction->set_myrelativebet(0);
	// Just dump the packet.
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendGameChatMessage(const std::string &msg)
{
	// Warning: This function is called in the context of the GUI thread.
	// Create a network packet containing the chat message.
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_ChatRequestMessage);
	ChatRequestMessage *netChat = packet->GetMsg()->mutable_chatrequestmessage();
	netChat->set_targetgameid(GetGameId());
	netChat->set_chattext(msg);

	// Just dump the packet.
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendLobbyChatMessage(const std::string &msg)
{
	// Warning: This function is called in the context of the GUI thread.
	// Create a network packet containing the chat message.
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_ChatRequestMessage);
	ChatRequestMessage *netChat = packet->GetMsg()->mutable_chatrequestmessage();
	netChat->set_chattext(msg);

	// Just dump the packet.
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendPrivateChatMessage(unsigned targetPlayerId, const std::string &msg)
{
	// Warning: This function is called in the context of the GUI thread.
	// Create a network packet containing the chat message.
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_ChatRequestMessage);
	ChatRequestMessage *netChat = packet->GetMsg()->mutable_chatrequestmessage();
	netChat->set_targetplayerid(targetPlayerId);
	netChat->set_chattext(msg);

	// Just dump the packet.
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendJoinFirstGame(const std::string &password, bool autoLeave)
{
	// Warning: This function is called in the context of the GUI thread.
	// Create a network packet to request joining a game.
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_JoinExistingGameMessage);
	JoinExistingGameMessage *netJoinGame = packet->GetMsg()->mutable_joinexistinggamemessage();
	netJoinGame->set_gameid(1);
	netJoinGame->set_autoleave(autoLeave);

	if (!password.empty()) {
		netJoinGame->set_password(password);
	}
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendJoinGame(unsigned gameId, const std::string &password, bool autoLeave)
{
	// Warning: This function is called in the context of the GUI thread.
	// Create a network packet to request joining a game.
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_JoinExistingGameMessage);
	JoinExistingGameMessage *netJoinGame = packet->GetMsg()->mutable_joinexistinggamemessage();
	netJoinGame->set_gameid(gameId);
	netJoinGame->set_autoleave(autoLeave);

	if (!password.empty()) {
		netJoinGame->set_password(password);
	}
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendRejoinGame(unsigned gameId, bool autoLeave)
{
	// Warning: This function is called in the context of the GUI thread.
	// Create a network packet to request rejoining a running game.
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_RejoinExistingGameMessage);
	RejoinExistingGameMessage *netJoinGame = packet->GetMsg()->mutable_rejoinexistinggamemessage();
	netJoinGame->set_gameid(gameId);
	netJoinGame->set_autoleave(autoLeave);

	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendCreateGame(const GameData &gameData, const std::string &name, const std::string &password, bool autoLeave)
{
	// Warning: This function is called in the context of the GUI thread.
	// Create a network packet to request creating a new game.
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_JoinNewGameMessage);
	JoinNewGameMessage *netJoinGame = packet->GetMsg()->mutable_joinnewgamemessage();
	netJoinGame->set_autoleave(autoLeave);
	NetGameInfo *gameInfo = netJoinGame->mutable_gameinfo();
	NetPacket::SetGameData(gameData, *gameInfo);
	gameInfo->set_gamename(name);

	if (!password.empty()) {
		netJoinGame->set_password(password);
	}
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendResetTimeout()
{
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_ResetTimeoutMessage);
	packet->GetMsg()->mutable_resettimeoutmessage();
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendAskKickPlayer(unsigned playerId)
{
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_AskKickPlayerMessage);
	AskKickPlayerMessage *netAsk = packet->GetMsg()->mutable_askkickplayermessage();
	netAsk->set_gameid(GetGameId());
	netAsk->set_playerid(playerId);
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendVoteKick(bool doKick)
{
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_VoteKickRequestMessage);
	VoteKickRequestMessage *netVote = packet->GetMsg()->mutable_votekickrequestmessage();
	netVote->set_gameid(GetGameId());
	{
		boost::mutex::scoped_lock lock(m_curPetitionIdMutex);
		netVote->set_petitionid(m_curPetitionId);
	}
	netVote->set_votekick(doKick);
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendShowMyCards()
{
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_ShowMyCardsRequestMessage);
	packet->GetMsg()->mutable_showmycardsrequestmessage();
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendInvitePlayerToCurrentGame(unsigned playerId)
{
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_InvitePlayerToGameMessage);
	InvitePlayerToGameMessage *netInvite = packet->GetMsg()->mutable_inviteplayertogamemessage();
	netInvite->set_gameid(GetGameId());
	netInvite->set_playerid(playerId);
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendRejectGameInvitation(unsigned gameId, DenyGameInvitationReason reason)
{
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_RejectGameInvitationMessage);
	RejectGameInvitationMessage *netReject = packet->GetMsg()->mutable_rejectgameinvitationmessage();
	netReject->set_gameid(gameId);
	netReject->set_myrejectreason(static_cast<RejectGameInvitationMessage::RejectGameInvReason>(reason));
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendReportAvatar(unsigned reportedPlayerId, const std::string &avatarHash)
{
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_ReportAvatarMessage);
	ReportAvatarMessage *netReport = packet->GetMsg()->mutable_reportavatarmessage();
	netReport->set_reportedplayerid(reportedPlayerId);
	MD5Buf tmpMD5;
	if (tmpMD5.FromString(avatarHash) && !tmpMD5.IsZero()) {
		netReport->set_reportedavatarhash(tmpMD5.GetData(), MD5_DATA_SIZE);

		auto self = shared_from_this();
		boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
	}
}

void
ClientThread::SendReportGameName(unsigned reportedGameId)
{
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_ReportGameMessage);
	ReportGameMessage *netReport = packet->GetMsg()->mutable_reportgamemessage();
	netReport->set_reportedgameid(reportedGameId);
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendAdminRemoveGame(unsigned removeGameId)
{
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_AdminRemoveGameMessage);
	AdminRemoveGameMessage *netRemove = packet->GetMsg()->mutable_adminremovegamemessage();
	netRemove->set_removegameid(removeGameId);
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::SendAdminBanPlayer(unsigned playerId)
{
	boost::shared_ptr<NetPacket> packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_AdminBanPlayerMessage);
	AdminBanPlayerMessage *netBan = packet->GetMsg()->mutable_adminbanplayermessage();
	netBan->set_banplayerid(playerId);
	auto self = shared_from_this();
	boost::asio::post(*m_ioService, [self, packet]() { self->SendSessionPacket(packet); });
}

void
ClientThread::StartAsyncRead()
{
	GetContext().GetSessionData()->GetReceiveBuffer().StartAsyncRead(GetContext().GetSessionData());
}

void
ClientThread::CloseSession(boost::shared_ptr<SessionData> /*session*/)
{
	throw NetException(__FILE__, __LINE__, ERR_SOCK_CONN_RESET, 0);
}

void
ClientThread::HandlePacket(boost::shared_ptr<SessionData> /*session*/, boost::shared_ptr<NetPacket> packet)
{
	GetState().HandlePacket(shared_from_this(), packet);
}

void
ClientThread::SelectServer(unsigned serverId)
{
	boost::mutex::scoped_lock lock(m_selectServerMutex);
	m_isServerSelected = true;
	m_selectedServerId = serverId;
}

void
ClientThread::SetLogin(const std::string &userName, const std::string &password)
{
	boost::mutex::scoped_lock lock(m_loginDataMutex);
	m_loginData.userName = userName;
	m_loginData.password = password;
}

ServerInfo
ClientThread::GetServerInfo(unsigned serverId) const
{
	ServerInfo tmpInfo{};
	boost::mutex::scoped_lock lock(m_serverInfoMapMutex);
	ServerInfoMap::const_iterator pos = m_serverInfoMap.find(serverId);
	if (pos != m_serverInfoMap.end()) {
		tmpInfo = pos->second;
	}
	return tmpInfo;
}

GameInfo
ClientThread::GetGameInfo(unsigned gameId) const
{
	GameInfo tmpInfo{};
	boost::mutex::scoped_lock lock(m_gameInfoMapMutex);
	GameInfoMap::const_iterator pos = m_gameInfoMap.find(gameId);
	if (pos != m_gameInfoMap.end()) {
		tmpInfo = pos->second;
	}
	return tmpInfo;
}

PlayerInfo
ClientThread::GetPlayerInfo(unsigned playerId) const
{
	PlayerInfo info{};
	if (!GetCachedPlayerInfo(playerId, info)) {
		std::ostringstream name;
		name << "#" << playerId;

		info.playerName = name.str();
	}
	return info;
}

bool
ClientThread::GetPlayerIdFromName(const string &playerName, unsigned &playerId) const
{
	bool retVal = false;

	boost::mutex::scoped_lock lock(m_playerInfoMapMutex);
	PlayerInfoMap::const_reverse_iterator i = m_playerInfoMap.rbegin();
	PlayerInfoMap::const_reverse_iterator end = m_playerInfoMap.rend();

	while (i != end) {
		if (i->second.playerName == playerName) {
			playerId = i->first;
			retVal = true;
			break;
		}
		++i;
	}
	return retVal;
}

unsigned
ClientThread::GetGameIdOfPlayer(unsigned playerId) const
{
	unsigned gameId = 0; // Default: no game (invalid id).

	// Iterate through all games to find the player.
	boost::mutex::scoped_lock lock(m_gameInfoMapMutex);
	GameInfoMap::const_iterator i = m_gameInfoMap.begin();
	GameInfoMap::const_iterator i_end = m_gameInfoMap.end();
	while (i != i_end) {
		PlayerIdList::const_iterator j = (*i).second.players.begin();
		PlayerIdList::const_iterator j_end = (*i).second.players.end();
		while (j != j_end) {
			if (playerId == *j) {
				gameId = (*i).first;
				break;
			}
			++j;
		}
		if (gameId)
			break;
		++i;
	}
	return gameId;
}

ClientCallback &
ClientThread::GetCallback()
{
	return m_gui;
}

GuiInterface &
ClientThread::GetGui()
{
	return m_gui;
}

boost::shared_ptr<Log>
ClientThread::GetClientLog()
{
	return m_clientLog;
}

AvatarManager &
ClientThread::GetAvatarManager()
{
	return m_avatarManager;
}

void
ClientThread::Main()
{
	// Main loop.
	try {
		InitAuthContext();
		// Start sub-threads.
		m_avatarDownloader.reset(new DownloaderThread);
		m_avatarDownloader->Run();
		SetState(CLIENT_INITIAL_STATE::Instance());
		RegisterTimers();

		m_ioService->run(); // Will only be aborted asynchronously.

	} catch (const PokerTHException &e) {
		// Delete the cached server list, as it may be outdated.
		std::filesystem::path tmpServerListPath(GetCacheServerListFileName());
		if (std::filesystem::exists(tmpServerListPath)) {
			std::filesystem::remove(tmpServerListPath);
		}
		GetCallback().SignalNetClientError(e.GetErrorId(), e.GetOsErrorCode());
	}
	// Close the socket.
	boost::system::error_code ec;
	auto sessionData = GetContext().GetSessionData();
	if (sessionData) {
		if (GetContext().GetTls()) {
			auto sslStream = sessionData->GetSslStream();
			if (sslStream) {
				sslStream->lowest_layer().close(ec);
			}
		} else {
			auto socket = sessionData->GetAsioSocket();
			if (socket) {
				socket->close(ec);
			}
		}
	}
	// Set a state which does not do anything.
	SetState(CLIENT_FINAL_STATE::Instance());
	// Cancel timers.
	GetStateTimer().cancel();
	CancelTimers();
	// Terminate sub-threads.
	m_avatarDownloader->SignalTermination();
	m_avatarDownloader->Join(DOWNLOADER_THREAD_TERMINATE_TIMEOUT);

	ClearAuthContext();
}

void
ClientThread::RegisterTimers()
{
	m_avatarTimer.expires_after(milliseconds(CLIENT_AVATAR_LOOP_MSEC));
	m_avatarTimer.async_wait(
		boost::bind(
			&ClientThread::TimerCheckAvatarDownloads, shared_from_this(), boost::asio::placeholders::error));
}

void
ClientThread::CancelTimers()
{
	m_avatarTimer.cancel();
}

void
ClientThread::InitAuthContext()
{
    m_authContext = nullptr;
}

void
ClientThread::ClearAuthContext()
{
    // GSASL entfernt: nichts zu räumen.
    m_authContext = nullptr;
}

void
ClientThread::InitGame()
{
	try {
		// Store current session guid, in case we need to rejoin the game.
		WriteSessionGuidToFile();

		// EngineFactory erstellen
		boost::shared_ptr<EngineFactory> factory(new ClientEngineFactory); // LocalEngine erstellen

		MapPlayerDataList();
		m_startData.numberOfPlayers = static_cast<int>(GetPlayerDataList().size());
		{
			boost::mutex::scoped_lock lock(m_gameMutex);
			m_game.reset(new Game(&m_gui, factory, GetPlayerDataList(), GetGameData(), GetStartData(), m_curGameNum++, m_clientLog.get()));
		}
		// Initialize Minimum GUI speed.
		int minimumGuiSpeed = 1;
		if(GetGameData().delayBetweenHandsSec < 11) {
			minimumGuiSpeed = 12-GetGameData().delayBetweenHandsSec;
		}
		GetGui().initGui(minimumGuiSpeed);
		// Signal start of game to GUI.
		GetCallback().SignalNetClientGameStart(m_game);
	} catch (const std::exception& e) {
		LOG_ERROR("Failed to initialize game: " << e.what());
		throw ClientException(__FILE__, __LINE__, ERR_GAME_INIT_FAILED, 0);
	}
}

void
ClientThread::SendSessionPacket(boost::shared_ptr<NetPacket> packet)
{
	if (IsSessionEstablished()) {
		GetSender().Send(GetContext().GetSessionData(), packet);
	} else {
		boost::mutex::scoped_lock lock(m_outPacketListMutex);
		m_outPacketList.push_back(packet);
	}
}

void
ClientThread::SendQueuedPackets()
{
	boost::mutex::scoped_lock lock(m_outPacketListMutex);
	if (!m_outPacketList.empty()) {
		NetPacketList::iterator i = m_outPacketList.begin();
		NetPacketList::iterator end = m_outPacketList.end();

		while (i != end) {
			GetSender().Send(GetContext().GetSessionData(), *i);
			++i;
		}
		m_outPacketList.clear();
	}
}

bool
ClientThread::GetCachedPlayerInfo(unsigned id, PlayerInfo &info) const
{
	bool retVal = false;

	boost::mutex::scoped_lock lock(m_playerInfoMapMutex);
	PlayerInfoMap::const_iterator pos = m_playerInfoMap.find(id);
	if (pos != m_playerInfoMap.end()) {
		info = pos->second;
		retVal = true;
	}
	return retVal;
}

void
ClientThread::RequestPlayerInfo(unsigned id, bool requestAvatar)
{
	list<unsigned> idList;
	idList.push_back(id);
	RequestPlayerInfo(idList, requestAvatar);
}

void
ClientThread::RequestPlayerInfo(const list<unsigned> &idList, bool requestAvatar)
{
	auto packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_PlayerInfoRequestMessage);
	PlayerInfoRequestMessage *netPlayerInfo = packet->GetMsg()->mutable_playerinforequestmessage();
	{
		boost::mutex::scoped_lock lock(m_playerInfoRequestListMutex);
		for(unsigned playerId : idList) {
			if (find(m_playerInfoRequestList.begin(), m_playerInfoRequestList.end(), playerId) == m_playerInfoRequestList.end()) {
				netPlayerInfo->add_playerid(playerId);
				m_playerInfoRequestList.push_back(playerId);
				if (m_playerInfoRequestList.size() > 1000) {
					m_playerInfoRequestList.pop_front();
				}
			}
		}
	}
	{
		boost::mutex::scoped_lock lock(m_avatarShouldRequestListMutex);
		for(unsigned playerId : idList) {
			if (requestAvatar) {
				m_avatarShouldRequestList.push_back(playerId);
				if (m_avatarShouldRequestList.size() > 1000) {
					m_avatarShouldRequestList.pop_front();
				}
			}
		}
	}
	if (netPlayerInfo->playerid_size() > 0) {
		GetSender().Send(GetContext().GetSessionData(), packet);
	}
}

void
ClientThread::SetPlayerInfo(unsigned id, const PlayerInfo &info)
{
	{
		boost::mutex::scoped_lock lock(m_playerInfoMapMutex);
		if (info.playerName.substr(0, sizeof(SERVER_COMPUTER_PLAYER_NAME) - 1) != SERVER_COMPUTER_PLAYER_NAME) {
			PlayerInfoMap::iterator i = m_playerInfoMap.begin();
			PlayerInfoMap::iterator end = m_playerInfoMap.end();
			while (i != end) {
				if (i->first != id && i->second.playerName == info.playerName) {
					m_playerInfoMap.erase(i);
					break;
				}
				++i;
			}
		}
		m_playerInfoMap[id] = info;
		while (m_playerInfoMap.size() > MAX_PLAYER_INFO_CACHE_SIZE) {
			m_playerInfoMap.erase(m_playerInfoMap.begin());
		}
	}

	// Update player data for current game.
	boost::shared_ptr<PlayerData> playerData(GetPlayerDataByUniqueId(id));
	if (playerData) {
		playerData->SetName(info.playerName);
		playerData->SetType(info.ptype);
		if (info.hasAvatar) {
			string avatarFile;
			if (GetAvatarManager().GetAvatarFileName(info.avatar, avatarFile)) {
				playerData->SetAvatarFile(GetQtToolsInterface().stringToUtf8(avatarFile));
			}
		}
	}
	auto game = GetGame();
	if (game) {
		boost::shared_ptr<PlayerInterface> clientPlayer(game->getPlayerByUniqueId(id));
		if (clientPlayer)
			clientPlayer->setMyName(info.playerName);
	}

	{
		boost::mutex::scoped_lock lock(m_avatarShouldRequestListMutex);
		if (find(m_avatarShouldRequestList.begin(), m_avatarShouldRequestList.end(), id) != m_avatarShouldRequestList.end()) {
			m_avatarShouldRequestList.remove(id);
			lock.unlock();
			RetrieveAvatarIfNeeded(id, info);
		}
	}

	{
		boost::mutex::scoped_lock lock(m_playerInfoRequestListMutex);
		m_playerInfoRequestList.remove(id);
	}

	// Notify GUI
	GetCallback().SignalNetClientPlayerChanged(id, info.playerName);

}

void
ClientThread::SetUnknownPlayer(unsigned id)
{
	{
		boost::mutex::scoped_lock lock(m_playerInfoRequestListMutex);
		m_playerInfoRequestList.remove(id);
	}
	{
		boost::mutex::scoped_lock lock(m_avatarShouldRequestListMutex);
		m_avatarShouldRequestList.remove(id);
	}
	LOG_ERROR("Server reported unknown player id: " << id);
}

void
ClientThread::SetNewGameAdmin(unsigned id)
{
	// Update player data for current game.
	boost::shared_ptr<PlayerData> playerData = GetPlayerDataByUniqueId(id);
	if (playerData.get()) {
		playerData->SetGameAdmin(true);
		GetCallback().SignalNetClientNewGameAdmin(id, playerData->GetName());
		auto game = GetGame();
		if(game) {
			m_clientLog->logPlayerAction(playerData->GetName(),LOG_ACTION_ADMIN);
		}
	}
}

void
ClientThread::RetrieveAvatarIfNeeded(unsigned id, const PlayerInfo &info)
{
	{
		boost::mutex::scoped_lock lock(m_avatarHasRequestedListMutex);
		if (find(m_avatarHasRequestedList.begin(), m_avatarHasRequestedList.end(), id) != m_avatarHasRequestedList.end()) {
			return;
		}
		if (info.hasAvatar && !info.avatar.IsZero() && !GetAvatarManager().HasAvatar(info.avatar)) {
			m_avatarHasRequestedList.push_back(id);
			if (m_avatarHasRequestedList.size() > 1000) {
				m_avatarHasRequestedList.pop_front();
			}
		} else {
			return;
		}
	}

	string avatarServerAddress(GetContext().GetAvatarServerAddr());
	if (!avatarServerAddress.empty() && m_avatarDownloader) {
		string serverFileName(info.avatar.ToString() + AvatarManager::GetAvatarFileExtension(info.avatarType));
		m_avatarDownloader->QueueDownload(
			id, avatarServerAddress + serverFileName, GetContext().GetCacheDir() + TEMP_AVATAR_FILENAME);
	} else {
		auto packet = boost::make_shared<NetPacket>();
		packet->GetMsg()->set_messagetype(PokerTHMessage::Type_AvatarRequestMessage);
		AvatarRequestMessage *netAvatar = packet->GetMsg()->mutable_avatarrequestmessage();
		netAvatar->set_requestid(id);
		netAvatar->set_avatarhash(info.avatar.GetData(), MD5_DATA_SIZE);
		GetSender().Send(GetContext().GetSessionData(), packet);
	}
}

std::string
ClientThread::GetPlayerName(unsigned id)
{
	PlayerInfo info{};
	if (!GetCachedPlayerInfo(id, info)) {
		std::ostringstream name;
		name << "#" << id;
		info.playerName = name.str();
		RequestPlayerInfo(id);
	}
	return info.playerName;
}

void
ClientThread::AddTempAvatarFile(unsigned playerId, unsigned avatarSize, AvatarFileType type)
{
	if (avatarSize > MAX_AVATAR_FILE_SIZE) {
		LOG_ERROR("Client rejected oversized avatar file: " << avatarSize << " bytes (max: " << MAX_AVATAR_FILE_SIZE << ")");
		return;
	}
	boost::shared_ptr<AvatarFile> tmpAvatar = boost::make_shared<AvatarFile>();
	tmpAvatar->fileData.reserve(avatarSize);
	tmpAvatar->fileType = type;
	tmpAvatar->reportedSize = avatarSize;

	boost::mutex::scoped_lock lock(m_tempAvatarMapMutex);
	m_tempAvatarMap[playerId] = tmpAvatar;
}

void
ClientThread::StoreInTempAvatarFile(unsigned playerId, const vector<unsigned char> &data)
{
	boost::mutex::scoped_lock lock(m_tempAvatarMapMutex);
	AvatarFileMap::iterator pos = m_tempAvatarMap.find(playerId);
	if (pos == m_tempAvatarMap.end())
		throw ClientException(__FILE__, __LINE__, ERR_NET_INVALID_REQUEST_ID, 0);
	if (pos->second->fileData.size() + data.size() > pos->second->reportedSize ||
		pos->second->fileData.size() + data.size() > MAX_AVATAR_FILE_SIZE) {
		LOG_ERROR("Avatar data exceeds reported size or max limit, discarding");
		m_tempAvatarMap.erase(pos);
		return;
	}
	std::copy(data.begin(), data.end(), back_inserter(pos->second->fileData));
}

void
ClientThread::CompleteTempAvatarFile(unsigned playerId)
{
	boost::mutex::scoped_lock lock(m_tempAvatarMapMutex);
	AvatarFileMap::iterator pos = m_tempAvatarMap.find(playerId);
	if (pos == m_tempAvatarMap.end())
		throw ClientException(__FILE__, __LINE__, ERR_NET_INVALID_REQUEST_ID, 0);
	boost::shared_ptr<AvatarFile> tmpAvatar = pos->second;
	unsigned avatarSize = static_cast<unsigned>(tmpAvatar->fileData.size());
	if (avatarSize != tmpAvatar->reportedSize)
		LOG_ERROR("Client received invalid avatar file size!");
	else
		PassAvatarFileToManager(playerId, tmpAvatar);

	m_tempAvatarMap.erase(pos);
}

void
ClientThread::PassAvatarFileToManager(unsigned playerId, boost::shared_ptr<AvatarFile> AvatarFile)
{
	PlayerInfo tmpPlayerInfo;
	if (!GetCachedPlayerInfo(playerId, tmpPlayerInfo))
		LOG_ERROR("Client received invalid player id!");
	else {
		if (AvatarFile->fileType == AVATAR_FILE_TYPE_UNKNOWN)
			AvatarFile->fileType = tmpPlayerInfo.avatarType;
		if (!AvatarFile->fileData.empty() && !GetAvatarManager().StoreAvatarInCache(tmpPlayerInfo.avatar, AvatarFile->fileType, AvatarFile->fileData.data(), AvatarFile->reportedSize, false))
			LOG_ERROR("Failed to store avatar in cache directory.");

		// Update player info, but never re-request avatar.
		SetPlayerInfo(playerId, tmpPlayerInfo);

		string fileName;
		if (GetAvatarManager().GetAvatarFileName(tmpPlayerInfo.avatar, fileName)) {
			// Dynamically update avatar in GUI.
			GetGui().setPlayerAvatar(playerId, GetQtToolsInterface().stringToUtf8(fileName));
		}
	}
}

void
ClientThread::SetUnknownAvatar(unsigned playerId)
{
	boost::mutex::scoped_lock lock(m_tempAvatarMapMutex);
	m_tempAvatarMap.erase(playerId);
	LOG_ERROR("Server reported unknown avatar for player: " << playerId);
}

void
ClientThread::TimerCheckAvatarDownloads(const boost::system::error_code& ec)
{
	if (!ec) {
		if (m_avatarDownloader && m_avatarDownloader->HasDownloadResult()) {
			unsigned playerId = 0;
			auto tmpAvatar = boost::make_shared<AvatarFile>();
			m_avatarDownloader->GetDownloadResult(playerId, tmpAvatar->fileData);
			tmpAvatar->reportedSize = tmpAvatar->fileData.size();
			PassAvatarFileToManager(playerId, tmpAvatar);
		}
		m_avatarTimer.expires_after(milliseconds(CLIENT_AVATAR_LOOP_MSEC));
		m_avatarTimer.async_wait(
			[self = shared_from_this()](const boost::system::error_code& ec) {
				self->TimerCheckAvatarDownloads(ec);
			});
	}
}

void
ClientThread::UnsubscribeLobbyMsg()
{
	if (GetContext().GetSubscribeLobbyMsg()) {
		// Send unsubscribe request.
		auto packet = boost::make_shared<NetPacket>();
		packet->GetMsg()->set_messagetype(PokerTHMessage::Type_SubscriptionRequestMessage);
		SubscriptionRequestMessage *netRequest = packet->GetMsg()->mutable_subscriptionrequestmessage();
		netRequest->set_subscriptionaction(SubscriptionRequestMessage::unsubscribeGameList);
		GetSender().Send(GetContext().GetSessionData(), packet);
		GetContext().SetSubscribeLobbyMsg(false);
	}
}

void
ClientThread::ResubscribeLobbyMsg()
{
	if (!GetContext().GetSubscribeLobbyMsg()) {
		// Clear game info map as it is outdated.
		ClearGameInfoMap();
		// Send resubscribe request.
		auto packet = boost::make_shared<NetPacket>();
		packet->GetMsg()->set_messagetype(PokerTHMessage::Type_SubscriptionRequestMessage);
		SubscriptionRequestMessage *netRequest = packet->GetMsg()->mutable_subscriptionrequestmessage();
		netRequest->set_subscriptionaction(SubscriptionRequestMessage::resubscribeGameList);
		GetSender().Send(GetContext().GetSessionData(), packet);
		GetContext().SetSubscribeLobbyMsg(true);
	}
}

const ClientContext &
ClientThread::GetContext() const
{
	if (!m_context.get())
		throw NetException(__FILE__, __LINE__, ERR_SOCK_INVALID_STATE, 0);
	return *m_context;
}

ClientContext &
ClientThread::GetContext()
{
	if (!m_context.get())
		throw NetException(__FILE__, __LINE__, ERR_SOCK_INVALID_STATE, 0);
	return *m_context;
}

string
ClientThread::GetCacheServerListFileName()
{
	string fileName;
	path tmpServerListPath(GetContext().GetCacheDir());
	string serverListUrl(GetContext().GetServerListUrl());
	// Retrieve the file name from the URL.
	size_t pos = serverListUrl.find_last_of('/');
	if (!GetContext().GetCacheDir().empty() && !serverListUrl.empty() && pos != string::npos && ++pos < serverListUrl.length()) {
		tmpServerListPath /= serverListUrl.substr(pos);
		fileName = tmpServerListPath.string();
	}
	return fileName;
}

// Implementierung der statischen SslInfoCallback Methode
void
ClientThread::SslInfoCallback(const SSL *ssl, int where, int ret)
{
    const char *state = SSL_state_string_long(ssl);
    
    if (where & SSL_CB_LOOP) {
        qDebug() << "[TLS-HANDSHAKE] Loop:" << (state ? state : "unknown");
    }
    else if (where & SSL_CB_ALERT) {
        const char *alert_type = (where & SSL_CB_READ) ? "read" : "write";
        qDebug() << "[TLS-HANDSHAKE] Alert" << alert_type << ":" 
                 << SSL_alert_type_string_long(ret) << "/"
                 << SSL_alert_desc_string_long(ret);
    }
    else if (where & SSL_CB_EXIT) {
        if (ret == 0) {
            qDebug() << "[TLS-HANDSHAKE] Exit failed in:" << (state ? state : "unknown");
        }
        else if (ret < 0) {
            qDebug() << "[TLS-HANDSHAKE] Exit error in:" << (state ? state : "unknown") << "ret:" << ret;
        }
    }
    else if (where & SSL_CB_HANDSHAKE_START) {
        qDebug() << "[TLS-HANDSHAKE] Handshake START";
    }
    else if (where & SSL_CB_HANDSHAKE_DONE) {
        qDebug() << "[TLS-HANDSHAKE] Handshake DONE successfully!";
    }
}

void
ClientThread::CreateContextSession()
{
    ClientContext &context = GetContext();

    auto resolver = boost::make_shared<boost::asio::ip::tcp::resolver>(*m_ioService);
    context.SetResolver(resolver);

    if (context.GetTls()) {
        auto sslCtx = boost::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23_client);
        
        if (context.GetTlsVerifyPeer()) {
            sslCtx->set_verify_mode(boost::asio::ssl::verify_peer);
            sslCtx->set_default_verify_paths();
        } else {
            LOG_ERROR("WARNING: TLS peer verification is disabled - connection is vulnerable to MITM attacks");
            sslCtx->set_verify_mode(boost::asio::ssl::verify_none);
        }

        SSL_CTX_set_info_callback(sslCtx->native_handle(), &ClientThread::SslInfoCallback);

        boost::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> sslStream(
            new boost::asio::ssl::stream<boost::asio::ip::tcp::socket>(*m_ioService, *sslCtx));

        SSL_set_info_callback(sslStream->native_handle(), &ClientThread::SslInfoCallback);

        auto session = boost::make_shared<SessionData>(sslStream, SESSION_ID_GENERIC, *this, *m_ioService, 0);
        context.SetSessionData(session);
    } else {
        auto sock = boost::make_shared<boost::asio::ip::tcp::socket>(*m_ioService);
        auto session = boost::make_shared<SessionData>(sock, SESSION_ID_GENERIC, *this, *m_ioService);
        context.SetSessionData(session);
    }
}

ClientState &
ClientThread::GetState()
{
	boost::mutex::scoped_lock lock(m_curStateMutex);
	if (!m_curState)
		throw NetException(__FILE__, __LINE__, ERR_SOCK_INVALID_STATE, 0);
	return *m_curState;
}

void
ClientThread::SetState(ClientState &newState)
{
	ClientState *oldState = nullptr;
	ClientState *newStatePtr = &newState;
	{
		boost::mutex::scoped_lock lock(m_curStateMutex);
		oldState = m_curState;
		m_curState = newStatePtr;
	}
	if (oldState) {
		oldState->Exit(shared_from_this());
	}
	newStatePtr->Enter(shared_from_this());
}

boost::asio::steady_timer &
ClientThread::GetStateTimer()
{
	return m_stateTimer;
}

SenderHelper &
ClientThread::GetSender()
{
	if (!m_senderHelper)
		throw NetException(__FILE__, __LINE__, ERR_SOCK_INVALID_STATE, 0);
	return *m_senderHelper;
}

unsigned
ClientThread::GetGameId() const
{
	boost::mutex::scoped_lock lock(m_curGameIdMutex);
	return m_curGameId;
}

void
ClientThread::SetGameId(unsigned id)
{
	boost::mutex::scoped_lock lock(m_curGameIdMutex);
	m_curGameId = id;
}

Gsasl *
ClientThread::GetAuthContext()
{
	return m_authContext;
}

const GameData &
ClientThread::GetGameData() const
{
	return m_gameData;
}

void
ClientThread::SetGameData(const GameData &gameData)
{
	m_gameData = gameData;
}

const StartData &
ClientThread::GetStartData() const
{
	return m_startData;
}

void
ClientThread::SetStartData(const StartData &startData)
{
	m_startData = startData;
}

unsigned
ClientThread::GetGuiPlayerId() const
{
	boost::mutex::scoped_lock lock(m_guiPlayerIdMutex);
	return m_guiPlayerId;
}

int
ClientThread::GetOrigGuiPlayerNum() const
{
	return m_origGuiPlayerNum;
}

void
ClientThread::SetGuiPlayerId(unsigned guiPlayerId)
{
	boost::mutex::scoped_lock lock(m_guiPlayerIdMutex);
	m_guiPlayerId = guiPlayerId;
}

boost::shared_ptr<Game>
ClientThread::GetGame()
{
	boost::mutex::scoped_lock lock(m_gameMutex);
	return m_game;
}

QtToolsInterface &
ClientThread::GetQtToolsInterface()
{
	if (!myQtToolsInterface.get())
		throw NetException(__FILE__, __LINE__, ERR_SOCK_INVALID_STATE, 0);
	return *myQtToolsInterface;
}

	boost::shared_ptr<PlayerData>
ClientThread::CreatePlayerData(unsigned playerId, bool isGameAdmin)
{
	boost::shared_ptr<PlayerData> playerData;
	PlayerInfo info{};
	if (GetCachedPlayerInfo(playerId, info)) {
		playerData.reset(
			new PlayerData(playerId, 0, info.ptype,
						   PLAYER_RIGHTS_NORMAL, isGameAdmin));
		playerData->SetName(info.playerName);
		if (info.hasAvatar) {
			string avatarFile;
			if (GetAvatarManager().GetAvatarFileName(info.avatar, avatarFile))
				playerData->SetAvatarFile(GetQtToolsInterface().stringToUtf8(avatarFile));
			else
				RetrieveAvatarIfNeeded(playerId, info);
		}
	} else {
		std::ostringstream name;
		name << "#" << playerId;

		// Request player info.
		RequestPlayerInfo(playerId, true);
		// Use temporary data until the PlayerInfo request is completed.
		playerData.reset(
			new PlayerData(playerId, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, isGameAdmin));
		playerData->SetName(name.str());
	}
	return playerData;
}

void
ClientThread::AddPlayerData(boost::shared_ptr<PlayerData> playerData)
{
	if (playerData.get() && !playerData->GetName().empty()) {
		boost::mutex::scoped_lock lock(m_playerDataListMutex);
		m_playerDataList.push_back(playerData);
		lock.unlock();
		if (playerData->GetUniqueId() == GetGuiPlayerId())
			GetCallback().SignalNetClientSelfJoined(playerData->GetUniqueId(), playerData->GetName(), playerData->IsGameAdmin());
		else {
			GetCallback().SignalNetClientPlayerJoined(playerData->GetUniqueId(), playerData->GetName(), playerData->IsGameAdmin());
		}
	}
}

void
ClientThread::RemovePlayerData(unsigned playerId, int removeReason)
{
	boost::shared_ptr<PlayerData> tmpData;

	{
		boost::mutex::scoped_lock lock(m_playerDataListMutex);
		PlayerDataList::iterator i = m_playerDataList.begin();
		PlayerDataList::iterator end = m_playerDataList.end();
		while (i != end) {
			if ((*i)->GetUniqueId() == playerId) {
				tmpData = *i;
				m_playerDataList.erase(i);
				break;
			}
			++i;
		}
	}

	if (tmpData.get()) {
		auto game = GetGame();
		if (game) {
			boost::shared_ptr<PlayerInterface> tmpPlayer(game->getPlayerByUniqueId(tmpData->GetUniqueId()));
			if (tmpPlayer) {
				tmpPlayer->setMyStayOnTableStatus(false);
			}
		}
		GetCallback().SignalNetClientPlayerLeft(tmpData->GetUniqueId(), tmpData->GetName(), removeReason);

		if(game) {
			if(removeReason == NTF_NET_REMOVED_KICKED) {
				m_clientLog->logPlayerAction(tmpData->GetName(),LOG_ACTION_KICKED);
			} else {
				m_clientLog->logPlayerAction(tmpData->GetName(),LOG_ACTION_LEFT);
			}
		}

	}
}

void
ClientThread::ClearPlayerDataList()
{
	boost::mutex::scoped_lock lock(m_playerDataListMutex);
	m_playerDataList.clear();
}

void
ClientThread::MapPlayerDataList()
{
	// Retrieve the GUI player.
	boost::shared_ptr<PlayerData> guiPlayer = GetPlayerDataByUniqueId(GetGuiPlayerId());
	if (!guiPlayer.get())
		throw ClientException(__FILE__, __LINE__, ERR_NET_UNKNOWN_PLAYER_ID, 0);
	m_origGuiPlayerNum = guiPlayer->GetNumber();

	// Create a copy of the player list so that the GUI player
	// is player 0. This is mapped because the GUI depends on it.
	PlayerDataList mappedList;
	
	{
		boost::mutex::scoped_lock lock(m_playerDataListMutex);
		PlayerDataList::const_iterator i = m_playerDataList.begin();
		PlayerDataList::const_iterator end = m_playerDataList.end();
		int numPlayers = GetStartData().numberOfPlayers;

		while (i != end) {
			auto tmpData = boost::make_shared<PlayerData>(*(*i));
			int numberDiff = numPlayers - m_origGuiPlayerNum;
			tmpData->SetNumber((tmpData->GetNumber() + numberDiff) % numPlayers);
			mappedList.push_back(tmpData);
			++i;
		}
	}

	// Sort the list by player number.
	mappedList.sort([](const boost::shared_ptr<PlayerData>& a, const boost::shared_ptr<PlayerData>& b) {
		return a->GetNumber() < b->GetNumber();
	});

	boost::mutex::scoped_lock lock(m_playerDataListMutex);
	m_playerDataList = mappedList;
}

const PlayerDataList
ClientThread::GetPlayerDataList() const
{
	boost::mutex::scoped_lock lock(m_playerDataListMutex);
	return m_playerDataList;
}

boost::shared_ptr<PlayerData>
ClientThread::GetPlayerDataByUniqueId(unsigned id)
{
	boost::shared_ptr<PlayerData> tmpPlayer;

	boost::mutex::scoped_lock lock(m_playerDataListMutex);
	PlayerDataList::const_iterator i = m_playerDataList.begin();
	PlayerDataList::const_iterator end = m_playerDataList.end();

	while (i != end) {
		if ((*i)->GetUniqueId() == id) {
			tmpPlayer = *i;
			break;
		}
		++i;
	}
	return tmpPlayer;
}

boost::shared_ptr<PlayerData>
ClientThread::GetPlayerDataByName(const std::string &name)
{
	boost::shared_ptr<PlayerData> tmpPlayer;

	if (!name.empty()) {
		boost::mutex::scoped_lock lock(m_playerDataListMutex);
		PlayerDataList::const_iterator i = m_playerDataList.begin();
		PlayerDataList::const_iterator end = m_playerDataList.end();

		while (i != end) {
			if ((*i)->GetName() == name) {
				tmpPlayer = *i;
				break;
			}
			++i;
		}
	}
	return tmpPlayer;
}

void
ClientThread::AddServerInfo(unsigned serverId, const ServerInfo &info)
{
	{
		boost::mutex::scoped_lock lock(m_serverInfoMapMutex);
		m_serverInfoMap.insert(ServerInfoMap::value_type(serverId, info));
	}
	GetCallback().SignalNetClientServerListAdd(serverId);
}

void
ClientThread::ClearServerInfoMap()
{
	{
		boost::mutex::scoped_lock lock(m_serverInfoMapMutex);
		m_serverInfoMap.clear();
	}
	GetCallback().SignalNetClientServerListClear();
}

bool
ClientThread::GetSelectedServer(unsigned &serverId) const
{
	bool retVal = false;
	boost::mutex::scoped_lock lock(m_selectServerMutex);
	if (m_isServerSelected) {
		retVal = true;
		serverId = m_selectedServerId;
	}
	return retVal;
}

void
ClientThread::UseServer(unsigned serverId)
{
	ClientContext &context = GetContext();
	ServerInfo useInfo(GetServerInfo(serverId));

	if (context.GetAddrFamily() == AF_INET6)
		context.SetServerAddr(useInfo.ipv6addr);
	else
		context.SetServerAddr(useInfo.ipv4addr);

	context.SetServerPort(static_cast<unsigned>(useInfo.port));
	context.SetAvatarServerAddr(useInfo.avatarServerAddr);
}

bool
ClientThread::GetLoginData(LoginData &loginData) const
{
	bool retVal = false;
	boost::mutex::scoped_lock lock(m_loginDataMutex);
	if (!m_loginData.userName.empty()) {
		loginData = m_loginData;
		retVal = true;
	}
	return retVal;
}

void
ClientThread::AddGameInfo(unsigned gameId, const GameInfo &info)
{
	{
		boost::mutex::scoped_lock lock(m_gameInfoMapMutex);
		m_gameInfoMap.insert(GameInfoMap::value_type(gameId, info));
	}
	GetCallback().SignalNetClientGameListNew(gameId);
}

void
ClientThread::UpdateGameInfoMode(unsigned gameId, GameMode mode)
{
	bool found = false;
	{
		boost::mutex::scoped_lock lock(m_gameInfoMapMutex);
		GameInfoMap::iterator pos = m_gameInfoMap.find(gameId);
		if (pos != m_gameInfoMap.end()) {
			found = true;
			(*pos).second.mode = mode;
		}
	}
	if (found)
		GetCallback().SignalNetClientGameListUpdateMode(gameId, mode);
}

void
ClientThread::UpdateGameInfoAdmin(unsigned gameId, unsigned adminPlayerId)
{
	bool found = false;
	{
		boost::mutex::scoped_lock lock(m_gameInfoMapMutex);
		GameInfoMap::iterator pos = m_gameInfoMap.find(gameId);
		if (pos != m_gameInfoMap.end()) {
			found = true;
			(*pos).second.adminPlayerId = adminPlayerId;
		}
	}
	if (found)
		GetCallback().SignalNetClientGameListUpdateAdmin(gameId, adminPlayerId);
}

void
ClientThread::RemoveGameInfo(unsigned gameId)
{
	bool found = false;
	{
		boost::mutex::scoped_lock lock(m_gameInfoMapMutex);
		GameInfoMap::iterator pos = m_gameInfoMap.find(gameId);
		if (pos != m_gameInfoMap.end()) {
			found = true;
			m_gameInfoMap.erase(pos);
		}
	}
	if (found)
		GetCallback().SignalNetClientGameListRemove(gameId);
}

void
ClientThread::ModifyGameInfoAddPlayer(unsigned gameId, unsigned playerId)
{
	bool playerAdded = false;
	{
		boost::mutex::scoped_lock lock(m_gameInfoMapMutex);
		GameInfoMap::iterator pos = m_gameInfoMap.find(gameId);
		if (pos != m_gameInfoMap.end()) {
			pos->second.players.push_back(playerId);
			playerAdded = true;
		}
	}
	if (playerAdded)
		GetCallback().SignalNetClientGameListPlayerJoined(gameId, playerId);
}

void
ClientThread::ModifyGameInfoRemovePlayer(unsigned gameId, unsigned playerId)
{
	bool playerRemoved = false;
	{
		boost::mutex::scoped_lock lock(m_gameInfoMapMutex);
		GameInfoMap::iterator pos = m_gameInfoMap.find(gameId);
		if (pos != m_gameInfoMap.end()) {
			pos->second.players.remove(playerId);
			playerRemoved = true;
		}
	}
	if (playerRemoved)
		GetCallback().SignalNetClientGameListPlayerLeft(gameId, playerId);
}

void
ClientThread::ClearGameInfoMap()
{
	boost::mutex::scoped_lock lock(m_gameInfoMapMutex);
	m_gameInfoMap.clear();
}

void
ClientThread::StartPetition(unsigned petitionId, unsigned proposingPlayerId, unsigned kickPlayerId, int timeoutSec, int numVotesToKick)
{
	{
		boost::mutex::scoped_lock lock(m_curPetitionIdMutex);
		m_curPetitionId = petitionId;
	}
	GetGui().startVoteOnKick(kickPlayerId, proposingPlayerId, timeoutSec, numVotesToKick);
	if (GetGuiPlayerId() != kickPlayerId
			&& GetGuiPlayerId() != proposingPlayerId) {
		GetGui().changeVoteOnKickButtonsState(true);
	}
}

void
ClientThread::UpdatePetition(unsigned petitionId, int /*numVotesAgainstKicking*/, int numVotesInFavourOfKicking, int numVotesToKick)
{
	bool isCurPetition = false;
	{
		boost::mutex::scoped_lock lock(m_curPetitionIdMutex);
		isCurPetition = m_curPetitionId == petitionId;
	}
	if (isCurPetition) {
		GetGui().refreshVotesMonitor(numVotesInFavourOfKicking, numVotesToKick);
	}
}

void
ClientThread::EndPetition(unsigned petitionId)
{
	bool isCurPetition = false;
	{
		boost::mutex::scoped_lock lock(m_curPetitionIdMutex);
		isCurPetition = m_curPetitionId == petitionId;
	}
	if (isCurPetition)
		GetGui().endVoteOnKick();
}

void
ClientThread::UpdateStatData(const ServerStats &stats)
{
	boost::mutex::scoped_lock lock(m_curStatsMutex);
	if (stats.numberOfPlayersOnServer)
		m_curStats.numberOfPlayersOnServer = stats.numberOfPlayersOnServer;

	if (stats.totalPlayersEverLoggedIn)
		m_curStats.totalPlayersEverLoggedIn = stats.totalPlayersEverLoggedIn;

	if (stats.totalGamesEverCreated)
		m_curStats.totalGamesEverCreated = stats.totalGamesEverCreated;

	GetCallback().SignalNetClientStatsUpdate(m_curStats);
}

void
ClientThread::EndPing()
{
	boost::mutex::scoped_lock lock(m_pingDataMutex);
	if (m_pingData.EndPing()) {
		GetCallback().SignalNetClientPingUpdate(m_pingData.MinPing(), m_pingData.AveragePing(), m_pingData.MaxPing());
	}
}

ServerStats
ClientThread::GetStatData() const
{
	boost::mutex::scoped_lock lock(m_curStatsMutex);
	return m_curStats;
}

bool
ClientThread::IsSessionEstablished() const
{
	return m_sessionEstablished.load(std::memory_order_acquire);
}

void
ClientThread::SetSessionEstablished(bool flag)
{
	bool wasEstablished = m_sessionEstablished.exchange(flag);
	if (!wasEstablished && flag) {
		SendQueuedPackets();
	}
}

bool
ClientThread::IsSynchronized() const
{
	boost::mutex::scoped_lock lock(m_playerInfoRequestListMutex);
	return m_playerInfoRequestList.empty();
}

void
ClientThread::ReadSessionGuidFromFile()
{
	string guidFileName(GetContext().GetCacheDir() + TEMP_GUID_FILENAME);
	std::ifstream guidStream(guidFileName.c_str(), ios::in | ios::binary);
	if (guidStream.good()) {
		std::vector<char> tmpGuid(CLIENT_GUID_SIZE);
		guidStream.read(&tmpGuid[0], CLIENT_GUID_SIZE);
		if (guidStream.good() || guidStream.gcount() == CLIENT_GUID_SIZE) {
			GetContext().SetSessionGuid(string(tmpGuid.begin(), tmpGuid.end()));
		}
	}
}

void
ClientThread::WriteSessionGuidToFile() const
{
	string guidFileName(GetContext().GetCacheDir() + TEMP_GUID_FILENAME);
	std::ofstream guidStream(guidFileName.c_str(), ios::out | ios::trunc | ios::binary);
	if (guidStream.good()) {
		guidStream.write(GetContext().GetSessionGuid().c_str(), GetContext().GetSessionGuid().size());
	}
}

