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

#include <net/servergame.h>
#include <net/servergamestate.h>
#include <net/serverlobbythread.h>
#include <net/serverexception.h>
#include <net/senderhelper.h>
#include <net/socket_msg.h>
#include <core/loghelper.h>
#include <db/serverdbinterface.h>
#include <db/serverdbnoaction.h>
#include <game.h>
#include <localenginefactory.h>
#include <tools.h>
#include <configfile.h>
#include <third_party/boost/timers.hpp>

#include <boost/asio.hpp>
#include <algorithm>

#include <random>


#define SERVER_CHECK_VOTE_KICK_INTERVAL_MSEC	500
#define SERVER_KICK_TIMEOUT_ADD_DELAY_SEC		2

using namespace std;

#ifdef BOOST_ASIO_HAS_STD_CHRONO
using namespace std::chrono;
#else
using namespace boost::chrono;
#endif

static bool LessThanPlayerHandStartMoney(const boost::shared_ptr<PlayerInterface> p1, const boost::shared_ptr<PlayerInterface> p2)
{
	return p1->getMyRoundStartCash() < p2->getMyRoundStartCash();
}


ServerGame::ServerGame(boost::shared_ptr<ServerLobbyThread> lobbyThread, uint32_t id, const string &name, const string &pwd, const GameData &gameData,
					   unsigned adminPlayerId, unsigned creatorPlayerDBId, GuiInterface &gui, ConfigFile &playerConfig)
	: m_adminPlayerId(adminPlayerId), m_lobbyThread(lobbyThread), m_gui(gui),
	  m_gameData(gameData), m_curState(nullptr), m_id(id), m_name(name),
	  m_password(pwd), m_creatorPlayerDBId(creatorPlayerDBId), m_playerConfig(playerConfig),
	  m_curPetitionId(1), m_voteKickTimer(lobbyThread->GetIOService()),
	  m_stateTimer1(lobbyThread->GetIOService()), m_stateTimer2(lobbyThread->GetIOService()),
	  m_isNameReported(false)
{
	LOG_VERBOSE("Game object " << GetId() << " created.");
}

ServerGame::~ServerGame() noexcept
{
	LOG_VERBOSE("Game object " << GetId() << " destructed.");
}

void
ServerGame::Init()
{
	SetState(SERVER_INITIAL_STATE::Instance());
}

void
ServerGame::Exit()
{
	m_voteKickTimer.cancel();
	m_stateTimer1.cancel();
	m_stateTimer2.cancel();
	// Skip SetState if already in Final state (RemoveAllSessions sets it first)
	{
		boost::mutex::scoped_lock lock(m_curStateMutex);
		if (m_curState == &ServerGameStateFinal::Instance())
			return;
	}
	SetState(ServerGameStateFinal::Instance());
}

uint32_t
ServerGame::GetId() const
{
	return m_id;
}

const std::string &
ServerGame::GetName() const
{
	return m_name;
}

unsigned
ServerGame::GetCreatorDBId() const
{
	return m_creatorPlayerDBId;
}

void
ServerGame::AddSession(boost::shared_ptr<SessionData> session)
{
	if (session) {
		GetState().HandleNewPlayer(shared_from_this(), session);
	}
}

void
ServerGame::RemovePlayer(unsigned playerId, unsigned errorCode)
{
	boost::shared_ptr<SessionData> tmpSession = GetSessionManager().GetSessionByUniquePlayerId(playerId);
	// Only kick if the player was found.
	if (tmpSession)
		SessionError(tmpSession, errorCode);
}

void
ServerGame::MutePlayer(unsigned playerId, bool mute)
{
	boost::mutex::scoped_lock lock(m_gameMutex);
	if (m_game) {
		boost::shared_ptr<PlayerInterface> tmpPlayer(m_game->getPlayerByUniqueId(playerId));
		if (tmpPlayer) {
			tmpPlayer->setIsMuted(mute);
		}
	}
}

void
ServerGame::MarkPlayerAsInactive(unsigned playerId)
{
	boost::mutex::scoped_lock lock(m_gameMutex);
	if (m_game) {
		boost::shared_ptr<PlayerInterface> tmpPlayer(m_game->getPlayerByUniqueId(playerId));
		if (tmpPlayer) {
			tmpPlayer->setIsSessionActive(false);
		}
	}
}

void
ServerGame::MarkPlayerAsKicked(unsigned playerId)
{
	boost::mutex::scoped_lock lock(m_gameMutex);
	if (m_game) {
		boost::shared_ptr<PlayerInterface> tmpPlayer(m_game->getPlayerByUniqueId(playerId));
		if (tmpPlayer) {
			tmpPlayer->setIsKicked(true);
			tmpPlayer->setMyGuid("");
		}
	}
}

void
ServerGame::HandlePacket(boost::shared_ptr<SessionData> session, boost::shared_ptr<NetPacket> packet)
{
	if (session && packet)
		GetState().ProcessPacket(shared_from_this(), session, packet);
}

GameState
ServerGame::GetCurRound() const
{
	boost::mutex::scoped_lock lock(m_gameMutex);
	if (!m_game)
		return GAME_STATE_PREFLOP;
	auto hand = m_game->getCurrentHand();
	if (!hand)
		return GAME_STATE_PREFLOP;
	return static_cast<GameState>(hand->getCurrentRound());
}

void
ServerGame::SendToAllPlayers(boost::shared_ptr<NetPacket> packet, int state)
{
	GetSessionManager().SendToAllSessions(GetLobbyThread().GetSender(), packet, state);
}

void
ServerGame::SendToAllButOnePlayers(boost::shared_ptr<NetPacket> packet, SessionId except, int state)
{
	GetSessionManager().SendToAllButOneSessions(GetLobbyThread().GetSender(), packet, except, state);
}

void
ServerGame::RemoveAllSessions()
{
	// Clean up ALL sessions which are left.
	GetSessionManager().ForEach(&SessionData::Close);
	GetSessionManager().Clear();
	SetState(ServerGameStateFinal::Instance());
}

void
ServerGame::TimerVoteKick(const boost::system::error_code &ec)
{
	bool isFinal = false;
	{
		boost::mutex::scoped_lock lock(m_curStateMutex);
		isFinal = (m_curState == &ServerGameStateFinal::Instance());
	}
	if (!ec && !isFinal) {
		// Check whether someone should be kicked, or whether a vote kick should be aborted.
		// Only one vote kick can be active at a time.
		unsigned petitionId = 0;
		unsigned kickPlayerId = 0;
		int numVotesToKick = 0;
		int numVotesInFavourOfKicking = 0;
		int numVotesAgainstKicking = 0;
		PlayerIdList votedPlayerIds;
		boost::timers::portable::microsec_timer voteTimer;
		int timeLimitSec = 0;
		bool hasActivePetition = false;
		
		{
			boost::mutex::scoped_lock lock(m_voteKickDataMutex);
			if (m_voteKickData) {
				hasActivePetition = true;
				petitionId = m_voteKickData->petitionId;
				kickPlayerId = m_voteKickData->kickPlayerId;
				numVotesToKick = m_voteKickData->numVotesToKick;
				numVotesInFavourOfKicking = m_voteKickData->numVotesInFavourOfKicking;
				numVotesAgainstKicking = m_voteKickData->numVotesAgainstKicking;
				votedPlayerIds = m_voteKickData->votedPlayerIds;
				voteTimer = m_voteKickData->voteTimer;
				timeLimitSec = m_voteKickData->timeLimitSec;
			}
		}
		
		if (hasActivePetition) {
			// Prepare some values.
			const PlayerIdList playerIds(GetPlayerIdList());
			int votesRequiredToKick = numVotesToKick - numVotesInFavourOfKicking;
			int playersAllowedToVote = 0;
			// We need to count the number of players which are still allowed to vote.
			PlayerIdList::const_iterator player_i = playerIds.begin();
			PlayerIdList::const_iterator player_end = playerIds.end();
			while (player_i != player_end) {
				if (find(votedPlayerIds.begin(), votedPlayerIds.end(), *player_i) == votedPlayerIds.end())
					playersAllowedToVote++;
				++player_i;
			}
			bool abortPetition = false;
			bool doKick = false;
			EndPetitionReason reason;

			// 1. Enough votes to kick the player.
			if (numVotesInFavourOfKicking >= numVotesToKick) {
				reason = PETITION_END_ENOUGH_VOTES;
				abortPetition = true;
				doKick = true;
			}
			// 2. Several players left the game, so a kick is no longer possible.
			else if (votesRequiredToKick > playersAllowedToVote) {
				reason = PETITION_END_NOT_ENOUGH_PLAYERS;
				abortPetition = true;
			}
			// 3. The kick has become invalid because the player to be kicked left.
			else if (!IsValidPlayer(kickPlayerId)) {
				reason = PETITION_END_PLAYER_LEFT;
				abortPetition = true;
			}
			// 4. A kick request timed out (because not everyone voted).
			else if (voteTimer.elapsed().total_seconds() >= timeLimitSec) {
				reason = PETITION_END_TIMEOUT;
				abortPetition = true;
			}
			if (abortPetition) {
				auto packet = boost::make_shared<NetPacket>();
				packet->GetMsg()->set_messagetype(PokerTHMessage::Type_EndKickPetitionMessage);
				EndKickPetitionMessage *netEndPetition = packet->GetMsg()->mutable_endkickpetitionmessage();
				netEndPetition->set_gameid(GetId());
				netEndPetition->set_petitionid(petitionId);
				netEndPetition->set_numvotesagainstkicking(numVotesAgainstKicking);
				netEndPetition->set_numvotesinfavourofkicking(numVotesInFavourOfKicking);
				netEndPetition->set_resultplayerkicked(doKick);
				netEndPetition->set_petitionendreason(static_cast<EndKickPetitionMessage::PetitionEndReason>(reason));
				SendToAllPlayers(packet, SessionData::Game);

				if (doKick)
					KickPlayer(kickPlayerId);
				
				boost::mutex::scoped_lock lock(m_voteKickDataMutex);
				m_voteKickData.reset();
			} else {
				m_voteKickTimer.expires_after(milliseconds(SERVER_CHECK_VOTE_KICK_INTERVAL_MSEC));
				auto self = shared_from_this();
				m_voteKickTimer.async_wait(
					[self](const boost::system::error_code& ec) { self->TimerVoteKick(ec); });
			}
		}
	}
}

PlayerDataList
ServerGame::InternalStartGame()
{
	LOG_VERBOSE("InternalStartGame() entered.");
	// Initialize the game.
	PlayerDataList playerData(GetFullPlayerDataList());

	if (playerData.size() >= 2) {
		// Set DB Backend.
		// @TODO: check for wec or bbc game with bbcbot as creator
		if (GetGameData().gameType == GAME_TYPE_RANKING)
			m_database = GetLobbyThread().GetDatabase();
		else
			m_database = boost::make_shared<ServerDBNoAction>();

		vector<boost::shared_ptr<PlayerData> > tmpData(playerData.begin(), playerData.end());
		std::random_device rd;
		std::array<unsigned int, std::mt19937::state_size> seedData;
		std::generate(seedData.begin(), seedData.end(), std::ref(rd));
		std::seed_seq seed(seedData.begin(), seedData.end());
		mt19937 rng(seed);
		shuffle(tmpData.begin(), tmpData.end(), rng);
		copy(tmpData.begin(), tmpData.end(), playerData.begin());

		// Set order of players.
		AssignPlayerNumbers(playerData);

		// Create EngineFactory
		auto factory = boost::make_shared<LocalEngineFactory>(&m_playerConfig);

		// Set start data.
		StartData startData;
		startData.numberOfPlayers = static_cast<int>(playerData.size());

		int tmpDealerPos = 0;
		if (startData.numberOfPlayers > 0) {
			Tools::GetRand(0, startData.numberOfPlayers-1, 1, &tmpDealerPos);
		}
		// The Player Id is not continuous. Therefore, the start dealer position
		// needs to be converted to a player Id, and cannot be directly generated
		// as player Id.
		PlayerDataList::const_iterator player_i = playerData.begin();
		PlayerDataList::const_iterator player_end = playerData.end();

		int tmpPos = 0;
		while (player_i != player_end) {
			startData.startDealerPlayerId = static_cast<unsigned>((*player_i)->GetUniqueId());
			if (tmpPos == tmpDealerPos)
				break;
			++tmpPos;
			++player_i;
		}
		if (player_i == player_end)
			throw ServerException(__FILE__, __LINE__, ERR_NET_DEALER_NOT_FOUND, 0);

		SetStartData(startData);

		GuiInterface &gui = GetGui();
		{
			boost::mutex::scoped_lock lock(m_gameMutex);
			if (!factory) {
				throw ServerException(__FILE__, __LINE__, ERR_SOCK_INTERNAL, 0);
			}
			m_game = boost::make_shared<Game>(&gui, factory, playerData, GetGameData(), GetStartData(), GetNextGameNum(), nullptr);
		}

		GetDatabase().AsyncCreateGame(GetId(), GetName());
		InitRankingMap(playerData);

		if (GetGameData().gameType == GAME_TYPE_RANKING)
			StoreLastGames(playerData);
		
	}
	return playerData;
}

void
ServerGame::InitRankingMap(const PlayerDataList &playerDataList)
{
	boost::mutex::scoped_lock lock(m_rankingMapMutex);
	PlayerDataList::const_iterator i = playerDataList.begin();
	PlayerDataList::const_iterator end = playerDataList.end();
	while (i != end) {
		boost::shared_ptr<PlayerData> tmpPlayer(*i);
		RankingData tmpData(tmpPlayer->GetDBId());
		m_rankingMap[tmpPlayer->GetUniqueId()] = tmpData;
		++i;
	}
}

void
ServerGame::UpdateRankingMap()
{
	// Snapshot player data under m_gameMutex to prevent race conditions
	// with concurrent game engine modifications to player cash/roundStartCash.
	struct PlayerRankingInfo {
		unsigned uniqueId;
		int cash;
		int roundStartCash;
	};
	std::vector<PlayerRankingInfo> allPlayers;
	int activeCount;
	{
		boost::mutex::scoped_lock lock(m_gameMutex);
		if (!m_game)
			return;
		list<boost::shared_ptr<PlayerInterface>> activePlayers = *m_game->getActivePlayerList();
		activeCount = static_cast<int>(activePlayers.size());
		allPlayers.reserve(activePlayers.size());
		for (const auto& p : activePlayers) {
			if (p) {
				allPlayers.push_back({p->getMyUniqueID(), p->getMyCash(), p->getMyRoundStartCash()});
			}
		}
	}

	// Separate removed (cash < 1) from active using the snapshot
	std::vector<PlayerRankingInfo> removedPlayers;
	std::vector<PlayerRankingInfo> stillActive;
	for (auto& info : allPlayers) {
		if (info.cash < 1) {
			removedPlayers.push_back(info);
		} else {
			stillActive.push_back(info);
		}
	}

	boost::mutex::scoped_lock lock(m_rankingMapMutex);
	int currentRank = static_cast<int>(removedPlayers.size() + stillActive.size());
	if (!removedPlayers.empty()) {
		std::sort(removedPlayers.begin(), removedPlayers.end(),
			[](const PlayerRankingInfo& a, const PlayerRankingInfo& b) {
				return a.roundStartCash < b.roundStartCash;
			});
		int currentRankCounter = 0;
		for (size_t i = 0; i < removedPlayers.size(); ++i) {
			SetPlayerPlace(removedPlayers[i].uniqueId, currentRank);
			++currentRankCounter;
			size_t next = i + 1;
			if (next < removedPlayers.size() && removedPlayers[i].roundStartCash < removedPlayers[next].roundStartCash) {
				currentRank -= currentRankCounter;
				if (currentRank < 1) currentRank = 1;
				currentRankCounter = 0;
			}
		}
		if (currentRankCounter > 0) {
			currentRank -= currentRankCounter;
			if (currentRank < 1) currentRank = 1;
		}
	}
	if (stillActive.size() == 1) {
		SetPlayerPlace(stillActive[0].uniqueId, 1);
	}
}

void
ServerGame::SetPlayerPlace(unsigned playerId, int place)
{
	RankingMap::iterator pos = m_rankingMap.find(playerId);
	if (pos != m_rankingMap.end()) {
		(*pos).second.place = place;
	}
}

void
ServerGame::ReplaceRankingPlayer(unsigned oldPlayerId, unsigned newPlayerId)
{
	boost::mutex::scoped_lock lock(m_rankingMapMutex);
	RankingMap::iterator pos = m_rankingMap.find(oldPlayerId);
	if (pos != m_rankingMap.end()) {
		RankingData tmpData((*pos).second);
		m_rankingMap[newPlayerId] = tmpData;
		m_rankingMap.erase(pos);
	}
}

void
ServerGame::StoreAndResetRanking()
{
	boost::mutex::scoped_lock lock(m_rankingMapMutex);
	// Store players in database.
	RankingMap::const_iterator i = m_rankingMap.begin();
	RankingMap::const_iterator end = m_rankingMap.end();
	while (i != end) {
		if ((*i).second.dbid != DB_ID_INVALID) {
			GetDatabase().SetGamePlayerPlace(GetId(), (*i).second.dbid, (*i).second.place);
		}
		++i;
	}
	GetDatabase().EndGame(GetId());
	m_rankingMap.clear();
}

void
ServerGame::StoreLastGames(const PlayerDataList &playerDataList)
{
	// Store players lastgames in database.
	PlayerDataList::const_iterator i = playerDataList.begin();
	PlayerDataList::const_iterator end = playerDataList.end();
	while (i != end) {
		boost::shared_ptr<PlayerData> tmpPlayer(*i);
		// tmpPlayer->GetUniqueId()
		tmpPlayer->AddPlayerLastGame(static_cast<long long>(time(nullptr)));
		std::vector<long long> last_games = tmpPlayer->GetPlayerLastGames();
		if(!last_games.empty()) {
			LOG_VERBOSE("TimeStamp stored: " << last_games.back());
			LOG_VERBOSE("Ready for storing vector for player " << tmpPlayer->GetDBId() << " - lastGameTs " << last_games.back());
			if(tmpPlayer->GetDBId() != DB_ID_INVALID) {
				boost::shared_ptr<SessionData> session = GetSessionManager().GetSessionByUniquePlayerId(tmpPlayer->GetUniqueId());
				if (session) {
					GetDatabase().SetPlayerLastGames(GetId(), tmpPlayer->GetDBId(), last_games, session->GetClientAddr());
				}
			}
		}
		++i;
	}
}

void
ServerGame::RemoveAutoLeavePlayers()
{
	PlayerIdList tmpList;
	{
		boost::mutex::scoped_lock lock(m_autoLeavePlayerListMutex);
		tmpList = m_autoLeavePlayerList;
		m_autoLeavePlayerList.clear();
	}
	PlayerIdList::const_iterator i = tmpList.begin();
	PlayerIdList::const_iterator end = tmpList.end();
	while (i != end) {
		boost::shared_ptr<SessionData> tmpSession = GetSessionManager().GetSessionByUniquePlayerId(*i);
		// Only remove if the player was found.
		if (tmpSession)
			MoveSessionToLobby(tmpSession, NTF_NET_REMOVED_ON_REQUEST);
		++i;
	}
}

void
ServerGame::InternalEndGame()
{
	{
		boost::mutex::scoped_lock lock(m_voteKickDataMutex);
		m_voteKickData.reset();
	}
	m_voteKickTimer.cancel();
	m_isNameReported.store(false);
	{
		boost::mutex::scoped_lock lock(m_reportedAvatarListMutex);
		m_reportedAvatarList.clear();
	}
	StoreAndResetRanking();
	{
		boost::mutex::scoped_lock lock(m_gameMutex);
		m_game.reset();
	}
	{
		boost::mutex::scoped_lock lock(m_numJoinsPerPlayerMutex);
		m_numJoinsPerPlayer.clear();
	}
}

void
ServerGame::KickPlayer(unsigned playerId)
{
	MarkPlayerAsKicked(playerId);

	// Kick the network session from this game.
	boost::shared_ptr<SessionData> tmpSession(GetSessionManager().GetSessionByUniquePlayerId(playerId));
	// Only kick if the player was found.
	if (tmpSession) {
		MoveSessionToLobby(tmpSession, NTF_NET_REMOVED_KICKED);
	}

}

void
ServerGame::InternalAskVoteKick(boost::shared_ptr<SessionData> byWhom, unsigned playerIdWho, unsigned timeoutSec)
{
	if (IsRunning() && byWhom->GetPlayerData()) {
		unsigned playerIdByWhom = byWhom->GetPlayerData()->GetUniqueId();
		if (playerIdByWhom == playerIdWho) {
			InternalDenyAskVoteKick(byWhom, playerIdWho, KICK_DENIED_CANNOT_KICK_SELF);
			return;
		}
		size_t numPlayers = GetSessionManager().GetPlayerIdList(SessionData::Game).size();
		if (numPlayers > 2) {
			if (IsValidPlayer(playerIdWho)) {
				bool petitionStarted = false;
				bool otherInProgress = false;
				unsigned petitionId = 0;
				unsigned kickPlayerId = 0;
				int numVotesNeeded = 0;
				{
					boost::mutex::scoped_lock lock(m_voteKickDataMutex);
					if (!m_voteKickData) {
						m_voteKickData = boost::make_shared<VoteKickData>();
						// Skip 0 as petition ID - it could be confused with unset/invalid
						unsigned nextId = m_curPetitionId.fetch_add(1, std::memory_order_relaxed);
						if (nextId == 0) nextId = m_curPetitionId.fetch_add(1, std::memory_order_relaxed);
						m_voteKickData->petitionId = nextId;
						m_voteKickData->kickPlayerId = playerIdWho;
						m_voteKickData->numVotesToKick = static_cast<int>(ceil(numPlayers * 2.0 / 3.0));
						m_voteKickData->timeLimitSec = timeoutSec + SERVER_KICK_TIMEOUT_ADD_DELAY_SEC;
						m_voteKickData->numVotesInFavourOfKicking = 1;
						m_voteKickData->votedPlayerIds.push_back(playerIdByWhom);
						petitionId = m_voteKickData->petitionId;
						kickPlayerId = m_voteKickData->kickPlayerId;
						numVotesNeeded = m_voteKickData->numVotesToKick;

						m_voteKickTimer.expires_after(milliseconds(SERVER_CHECK_VOTE_KICK_INTERVAL_MSEC));
						auto self = shared_from_this();
						m_voteKickTimer.async_wait(
							[self](const boost::system::error_code& ec) { self->TimerVoteKick(ec); });

						petitionStarted = true;
					} else {
						otherInProgress = true;
					}
				}
				if (petitionStarted) {
					auto packet = boost::make_shared<NetPacket>();
					packet->GetMsg()->set_messagetype(PokerTHMessage::Type_StartKickPetitionMessage);
					StartKickPetitionMessage *netStartPetition = packet->GetMsg()->mutable_startkickpetitionmessage();
					netStartPetition->set_gameid(GetId());
					netStartPetition->set_petitionid(petitionId);
					netStartPetition->set_proposingplayerid(playerIdByWhom);
					netStartPetition->set_kickplayerid(kickPlayerId);
					netStartPetition->set_kicktimeoutsec(timeoutSec);
					netStartPetition->set_numvotesneededtokick(numVotesNeeded);
					SendToAllPlayers(packet, SessionData::Game);
				} else if (otherInProgress) {
					InternalDenyAskVoteKick(byWhom, playerIdWho, KICK_DENIED_OTHER_IN_PROGRESS);
				}
			} else
				InternalDenyAskVoteKick(byWhom, playerIdWho, KICK_DENIED_INVALID_PLAYER_ID);
		} else
			InternalDenyAskVoteKick(byWhom, playerIdWho, KICK_DENIED_TOO_FEW_PLAYERS);
	} else
		InternalDenyAskVoteKick(byWhom, playerIdWho, KICK_DENIED_INVALID_STATE);
}

void
ServerGame::InternalDenyAskVoteKick(boost::shared_ptr<SessionData> byWhom, unsigned playerIdWho, DenyKickPlayerReason reason)
{
	auto packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_AskKickDeniedMessage);
	AskKickDeniedMessage *netKickDenied = packet->GetMsg()->mutable_askkickdeniedmessage();
	netKickDenied->set_gameid(GetId());
	netKickDenied->set_playerid(playerIdWho);
	netKickDenied->set_kickdeniedreason(static_cast<AskKickDeniedMessage::KickDeniedReason>(reason));
	GetLobbyThread().GetSender().Send(byWhom, packet);
}

void
ServerGame::InternalVoteKick(boost::shared_ptr<SessionData> byWhom, unsigned petitionId, KickVote vote)
{
	if (IsRunning() && byWhom->GetPlayerData()) {
		DenyVoteReason denyReason = VOTE_DENIED_INVALID_PETITION;
		bool shouldSendUpdate = false;
		int numVotesAgainst = 0;
		int numVotesInFavour = 0;
		int numVotesNeeded = 0;
		{
			boost::mutex::scoped_lock lock(m_voteKickDataMutex);
			if (m_voteKickData && m_voteKickData->petitionId == petitionId) {
				unsigned playerId = byWhom->GetPlayerData()->GetUniqueId();
				if (find(m_voteKickData->votedPlayerIds.begin(), m_voteKickData->votedPlayerIds.end(), playerId) == m_voteKickData->votedPlayerIds.end()) {
					m_voteKickData->votedPlayerIds.push_back(playerId);
					if (vote == KICK_VOTE_IN_FAVOUR)
						m_voteKickData->numVotesInFavourOfKicking++;
					else
						m_voteKickData->numVotesAgainstKicking++;
					numVotesAgainst = m_voteKickData->numVotesAgainstKicking;
					numVotesInFavour = m_voteKickData->numVotesInFavourOfKicking;
					numVotesNeeded = m_voteKickData->numVotesToKick;
					shouldSendUpdate = true;
				} else {
					denyReason = VOTE_DENIED_ALREADY_VOTED;
				}
			}
		}
		if (shouldSendUpdate) {
			auto packet = boost::make_shared<NetPacket>();
			packet->GetMsg()->set_messagetype(PokerTHMessage::Type_KickPetitionUpdateMessage);
			KickPetitionUpdateMessage *netKickUpdate = packet->GetMsg()->mutable_kickpetitionupdatemessage();
			netKickUpdate->set_gameid(GetId());
			netKickUpdate->set_petitionid(petitionId);
			netKickUpdate->set_numvotesagainstkicking(numVotesAgainst);
			netKickUpdate->set_numvotesinfavourofkicking(numVotesInFavour);
			netKickUpdate->set_numvotesneededtokick(numVotesNeeded);
			SendToAllPlayers(packet, SessionData::Game);
		} else {
			InternalDenyVoteKick(byWhom, petitionId, denyReason);
		}
	} else
		InternalDenyVoteKick(byWhom, petitionId, VOTE_DENIED_INVALID_PETITION);
}

void
ServerGame::InternalDenyVoteKick(boost::shared_ptr<SessionData> byWhom, unsigned petitionId, DenyVoteReason reason)
{
	auto packet = boost::make_shared<NetPacket>();
	packet->GetMsg()->set_messagetype(PokerTHMessage::Type_VoteKickReplyMessage);
	VoteKickReplyMessage *netVoteReply = packet->GetMsg()->mutable_votekickreplymessage();
	netVoteReply->set_gameid(GetId());
	netVoteReply->set_petitionid(petitionId);
	switch(reason) {
	case VOTE_DENIED_ALREADY_VOTED :
		netVoteReply->set_votekickreplytype(VoteKickReplyMessage::voteKickDeniedAlreadyVoted);
		break;
	default:
		netVoteReply->set_votekickreplytype(VoteKickReplyMessage::voteKickDeniedInvalid);
		break;
	}
	GetLobbyThread().GetSender().Send(byWhom, packet);
}

PlayerDataList
ServerGame::GetFullPlayerDataList() const
{
	PlayerDataList playerList(GetSessionManager().GetPlayerDataList());
	boost::mutex::scoped_lock lock(m_computerPlayerListMutex);
	copy(m_computerPlayerList.begin(), m_computerPlayerList.end(), back_inserter(playerList));

	return playerList;
}

boost::shared_ptr<PlayerData>
ServerGame::GetPlayerDataByUniqueId(unsigned playerId) const
{
	boost::shared_ptr<PlayerData> tmpPlayer;
	boost::shared_ptr<SessionData> session = GetSessionManager().GetSessionByUniquePlayerId(playerId);
	if (session) {
		tmpPlayer = session->GetPlayerData();
	}
	if (!tmpPlayer) {
		boost::mutex::scoped_lock lock(m_computerPlayerListMutex);
		PlayerDataList::const_iterator i = m_computerPlayerList.begin();
		PlayerDataList::const_iterator end = m_computerPlayerList.end();
		while (i != end) {
			if ((*i)->GetUniqueId() == playerId) {
				tmpPlayer = *i;
				break;
			}
			++i;
		}
	}
	return tmpPlayer;
}

PlayerIdList
ServerGame::GetPlayerIdList() const
{
	PlayerIdList idList(GetSessionManager().GetPlayerIdList(SessionData::Game));
	boost::mutex::scoped_lock lock(m_computerPlayerListMutex);
	PlayerDataList::const_iterator i = m_computerPlayerList.begin();
	PlayerDataList::const_iterator end = m_computerPlayerList.end();
	while (i != end) {
		idList.push_back((*i)->GetUniqueId());
		++i;
	}

	return idList;
}

bool
ServerGame::IsPlayerConnected(const std::string &name) const
{
	return GetSessionManager().IsPlayerConnected(name);
}

bool
ServerGame::IsPlayerConnected(unsigned playerId) const
{
	return GetSessionManager().IsPlayerConnected(playerId);
}

bool
ServerGame::IsClientAddressConnected(const std::string &clientAddress) const
{
	return GetSessionManager().IsClientAddressConnected(clientAddress);
}

boost::shared_ptr<PlayerInterface>
ServerGame::GetPlayerInterfaceFromGame(const std::string &playerName)
{
	boost::mutex::scoped_lock lock(m_gameMutex);
	boost::shared_ptr<PlayerInterface> tmpPlayer;
	if (m_game) {
		tmpPlayer = m_game->getPlayerByName(playerName);
	}
	return tmpPlayer;
}

boost::shared_ptr<PlayerInterface>
ServerGame::GetPlayerInterfaceFromGame(unsigned playerId)
{
	boost::mutex::scoped_lock lock(m_gameMutex);
	boost::shared_ptr<PlayerInterface> tmpPlayer;
	if (m_game) {
		tmpPlayer = m_game->getPlayerByUniqueId(playerId);
	}
	return tmpPlayer;
}

bool
ServerGame::IsRunning() const
{
	boost::mutex::scoped_lock lock(m_gameMutex);
	return m_game.get() != nullptr;
}

unsigned
ServerGame::GetAdminPlayerId() const
{
	boost::mutex::scoped_lock lock(m_adminPlayerIdMutex);
	return m_adminPlayerId;
}

void
ServerGame::SetAdminPlayerId(unsigned playerId)
{
	boost::mutex::scoped_lock lock(m_adminPlayerIdMutex);
	m_adminPlayerId = playerId;
}

void
ServerGame::AddPlayerInvitation(unsigned playerId)
{
	boost::mutex::scoped_lock lock(m_playerInvitationListMutex);
	m_playerInvitationList.push_back(playerId);
	m_playerInvitationList.sort();
	m_playerInvitationList.unique();
}

void
ServerGame::RemovePlayerInvitation(unsigned playerId)
{
	boost::mutex::scoped_lock lock(m_playerInvitationListMutex);
	m_playerInvitationList.remove(playerId);
}

bool
ServerGame::IsPlayerInvited(unsigned playerId) const
{
	bool retVal = false;
	boost::mutex::scoped_lock lock(m_playerInvitationListMutex);
	PlayerIdList::const_iterator pos = find(m_playerInvitationList.begin(), m_playerInvitationList.end(), playerId);
	if (pos != m_playerInvitationList.end())
		retVal = true;
	return retVal;
}

void
ServerGame::SetPlayerAutoLeaveOnFinish(unsigned playerId)
{
	boost::mutex::scoped_lock lock(m_autoLeavePlayerListMutex);
	m_autoLeavePlayerList.push_back(playerId);
}

void
ServerGame::AddRejoinPlayer(unsigned playerId)
{
	boost::mutex::scoped_lock lock(m_rejoinPlayerListMutex);
	m_rejoinPlayerList.push_back(playerId);
}

PlayerIdList
ServerGame::GetAndResetRejoinPlayers()
{
	boost::mutex::scoped_lock lock(m_rejoinPlayerListMutex);
	PlayerIdList tmpList(m_rejoinPlayerList);
	m_rejoinPlayerList.clear();
	return tmpList;
}

void
ServerGame::AddReactivatePlayer(unsigned playerId)
{
	boost::mutex::scoped_lock lock(m_reactivatePlayerListMutex);
	m_reactivatePlayerList.push_back(playerId);
}

PlayerIdList
ServerGame::GetAndResetReactivatePlayers()
{
	boost::mutex::scoped_lock lock(m_reactivatePlayerListMutex);
	PlayerIdList tmpList(m_reactivatePlayerList);
	m_reactivatePlayerList.clear();
	return tmpList;
}

void
ServerGame::SetNameReported()
{
	m_isNameReported.store(true);
}

bool
ServerGame::IsNameReported() const
{
	return m_isNameReported.load();
}

void
ServerGame::AddComputerPlayer(boost::shared_ptr<PlayerData> player)
{
	{
		boost::mutex::scoped_lock lock(m_computerPlayerListMutex);
		m_computerPlayerList.push_back(player);
	}
	GetLobbyThread().AddComputerPlayer(player);
}

boost::shared_ptr<PlayerData>
ServerGame::RemoveComputerPlayer(unsigned playerId)
{
	boost::shared_ptr<PlayerData> tmpPlayer;
	{
		boost::mutex::scoped_lock lock(m_computerPlayerListMutex);
		PlayerDataList::iterator i = m_computerPlayerList.begin();
		PlayerDataList::iterator end = m_computerPlayerList.end();
		while (i != end) {
			if ((*i)->GetUniqueId() == playerId) {
				tmpPlayer = *i;
				m_computerPlayerList.erase(i);
				break;
			}
			++i;
		}
	}
	if (tmpPlayer)
		GetLobbyThread().RemoveComputerPlayer(tmpPlayer);
	return tmpPlayer;
}

bool
ServerGame::IsComputerPlayerActive(unsigned playerId) const
{
	bool retVal = false;
	boost::mutex::scoped_lock lock(m_computerPlayerListMutex);
	PlayerDataList::const_iterator i = m_computerPlayerList.begin();
	PlayerDataList::const_iterator end = m_computerPlayerList.end();
	while (i != end) {
		if ((*i)->GetUniqueId() == playerId)
			retVal = true;
		++i;
	}
	return retVal;
}

void
ServerGame::ResetComputerPlayerList()
{
	PlayerDataList tmpList;
	{
		boost::mutex::scoped_lock lock(m_computerPlayerListMutex);
		tmpList = m_computerPlayerList;
		m_computerPlayerList.clear();
	}

	PlayerDataList::iterator i = tmpList.begin();
	PlayerDataList::iterator end = tmpList.end();

	while (i != end) {
		GetLobbyThread().RemoveComputerPlayer(*i);
		RemovePlayerData(*i, NTF_NET_REMOVED_ON_REQUEST);
		++i;
	}
}

void
ServerGame::RemoveSession(boost::shared_ptr<SessionData> session, int reason)
{
	if (!session)
		throw ServerException(__FILE__, __LINE__, ERR_NET_INVALID_SESSION, 0);

	if (GetSessionManager().RemoveSession(session->GetId())) {
		boost::shared_ptr<PlayerData> tmpPlayerData = session->GetPlayerData();
		if (tmpPlayerData && !tmpPlayerData->GetName().empty()) {
			RemovePlayerData(tmpPlayerData, reason);
		}
	}
}

void
ServerGame::RemovePlayerData(boost::shared_ptr<PlayerData> player, int reason)
{
	if (!player)
		return;

	if (player->IsGameAdmin()) {
		// Find new admin for the game
		PlayerDataList playerList(GetSessionManager().GetPlayerDataList());
		playerList.remove_if([&player](const boost::shared_ptr<PlayerData> &p) { return p->GetUniqueId() == player->GetUniqueId(); });
		if (!playerList.empty()) {
			boost::shared_ptr<PlayerData> newAdmin = playerList.front();
			SetAdminPlayerId(newAdmin->GetUniqueId());
			newAdmin->SetGameAdmin(true);
			// Notify game state on admin change
			GetState().NotifyGameAdminChanged(shared_from_this());
			// Send "Game Admin Changed" to clients.
			auto adminChanged = boost::make_shared<NetPacket>();
			adminChanged->GetMsg()->set_messagetype(PokerTHMessage::Type_GameAdminChangedMessage);
			GameAdminChangedMessage *netGameAdmin = adminChanged->GetMsg()->mutable_gameadminchangedmessage();
			netGameAdmin->set_gameid(GetId());
			netGameAdmin->set_newadminplayerid(newAdmin->GetUniqueId()); // Choose next player as admin.
			GetSessionManager().SendToAllSessions(GetLobbyThread().GetSender(), adminChanged, SessionData::Game);

			GetLobbyThread().NotifyGameAdminChanged(GetId(), newAdmin->GetUniqueId());
		}
	}
	// Reset player rights.
	player->SetGameAdmin(false);

	// Send "Player Left" to clients.
	auto thisPlayerLeft = boost::make_shared<NetPacket>();
	GamePlayerLeftMessage::GamePlayerLeftReason netReason = GamePlayerLeftMessage::leftError;
	switch (reason) {
	case NTF_NET_REMOVED_ON_REQUEST :
		netReason = GamePlayerLeftMessage::leftOnRequest;
		break;
	case NTF_NET_REMOVED_KICKED :
		netReason = GamePlayerLeftMessage::leftKicked;
		break;
	}

	thisPlayerLeft->GetMsg()->set_messagetype(PokerTHMessage::Type_GamePlayerLeftMessage);
	GamePlayerLeftMessage *netPlayerLeft = thisPlayerLeft->GetMsg()->mutable_gameplayerleftmessage();
	netPlayerLeft->set_gameid(GetId());
	netPlayerLeft->set_playerid(player->GetUniqueId());
	netPlayerLeft->set_gameplayerleftreason(netReason);

	GetSessionManager().SendToAllSessions(GetLobbyThread().GetSender(), thisPlayerLeft, SessionData::Game);

	GetState().NotifySessionRemoved(shared_from_this());
	GetLobbyThread().NotifyPlayerLeftGame(GetId(), player->GetUniqueId());
}

void
ServerGame::SessionError(boost::shared_ptr<SessionData> session, int errorCode)
{
	if (!session)
		throw ServerException(__FILE__, __LINE__, ERR_NET_INVALID_SESSION, 0);
	RemoveSession(session, NTF_NET_INTERNAL);
	GetLobbyThread().SessionError(session, errorCode);
}

void
ServerGame::MoveSessionToLobby(boost::shared_ptr<SessionData> session, int reason)
{
	RemoveSession(session, reason);
	// Reset ready flag - just in case it is set, player may leave at any time.
	session->ResetReadyFlag();
	GetLobbyThread().ReAddSession(session, reason, GetId());
}

void
ServerGame::RemoveDisconnectedPlayers()
{
	boost::mutex::scoped_lock gameLock(m_gameMutex);
	// This should only be called between hands.
	if (m_game) {
		PlayerList tmpList(m_game->getSeatsList());
		PlayerListIterator i = tmpList->begin();
		PlayerListIterator end = tmpList->end();
		while (i != end) {
			boost::shared_ptr<PlayerInterface> tmpPlayer = *i;
			if ((tmpPlayer->getMyType() == PLAYER_TYPE_HUMAN && !GetSessionManager().IsPlayerConnected(tmpPlayer->getMyUniqueID()))
					|| (tmpPlayer->getMyType() == PLAYER_TYPE_COMPUTER && !IsComputerPlayerActive(tmpPlayer->getMyUniqueID()))) {
				// Setting player cash to 0 will deactivate the player.
				// The player should only be deactivated if rejoin is not possible.
				if (tmpPlayer->isKicked() || tmpPlayer->getMyGuid().empty()) {
					tmpPlayer->setMyCash(0);
					tmpPlayer->setMyGuid("");
				}
				tmpPlayer->setIsSessionActive(false);
			}
			++i;
		}
	}
}

int
ServerGame::GetCurNumberOfPlayers() const
{
	return static_cast<int>(GetFullPlayerDataList().size());
}

void
ServerGame::AssignPlayerNumbers(PlayerDataList &playerList)
{
	int playerNumber = 0;

	PlayerDataList::iterator player_i = playerList.begin();
	PlayerDataList::iterator player_end = playerList.end();

	while (player_i != player_end) {
		(*player_i)->SetNumber(playerNumber);
		++playerNumber;
		++player_i;
	}
}

bool
ServerGame::IsValidPlayer(unsigned playerId) const
{
	bool retVal = false;
	const PlayerIdList list(GetPlayerIdList());
	if (find(list.begin(), list.end(), playerId) != list.end())
		retVal = true;
	return retVal;
}

void
ServerGame::AddReportedAvatar(unsigned playerId)
{
	boost::mutex::scoped_lock lock(m_reportedAvatarListMutex);
	m_reportedAvatarList.push_back(playerId);
	m_reportedAvatarList.sort();
	m_reportedAvatarList.unique();
}

bool
ServerGame::IsAvatarReported(unsigned playerId) const
{
	bool retVal = false;
	boost::mutex::scoped_lock lock(m_reportedAvatarListMutex);
	PlayerIdList::const_iterator pos = find(m_reportedAvatarList.begin(), m_reportedAvatarList.end(), playerId);
	if (pos != m_reportedAvatarList.end())
		retVal = true;
	return retVal;
}

SessionManager &
ServerGame::GetSessionManager()
{
	return m_sessionManager;
}

const SessionManager &
ServerGame::GetSessionManager() const
{
	return m_sessionManager;
}

ServerDBInterface &
ServerGame::GetDatabase()
{
	if (!m_database) {
		throw ServerException(__FILE__, __LINE__, ERR_SOCK_INTERNAL, 0);
	}
	return *m_database;
}

ServerLobbyThread &
ServerGame::GetLobbyThread()
{
	if (!m_lobbyThread) {
		throw ServerException(__FILE__, __LINE__, ERR_SOCK_INTERNAL, 0);
	}
	return *m_lobbyThread;
}

ServerCallback &
ServerGame::GetCallback()
{
	return m_gui;
}

ServerGameState &
ServerGame::GetState()
{
	boost::mutex::scoped_lock lock(m_curStateMutex);
	if (!m_curState) {
		throw ServerException(__FILE__, __LINE__, ERR_SOCK_INTERNAL, 0);
	}
	return *m_curState;
}

bool
ServerGame::IsCurrentState(const ServerGameState *state) const
{
	boost::mutex::scoped_lock lock(m_curStateMutex);
	return m_curState == state;
}

void
ServerGame::SetState(ServerGameState &newState)
{
	ServerGameState *oldState = nullptr;
	{
		boost::mutex::scoped_lock lock(m_curStateMutex);
		oldState = m_curState;
		m_curState = &newState;
	}
	// Call virtual functions outside the lock to prevent deadlock if
	// Enter/Exit directly or indirectly call GetState().
	if (oldState) {
		try {
			oldState->Exit(shared_from_this());
		} catch (...) {
			LOG_ERROR("SetState: Exit() threw exception, attempting to continue with new state");
		}
	}
	try {
		newState.Enter(shared_from_this());
	} catch (...) {
		// Roll back on Enter failure - restore previous state and re-enter it
		{
			boost::mutex::scoped_lock lock(m_curStateMutex);
			m_curState = oldState;
		}
		if (oldState) {
			try { oldState->Enter(shared_from_this()); } catch (...) {
				LOG_ERROR("SetState: rollback to previous state failed, game may be in inconsistent state");
			}
		}
	}
}

boost::asio::steady_timer &
ServerGame::GetStateTimer1()
{
	return m_stateTimer1;
}

boost::asio::steady_timer &
ServerGame::GetStateTimer2()
{
	return m_stateTimer2;
}

boost::shared_ptr<Game>
ServerGame::GetGame() const
{
	boost::mutex::scoped_lock lock(m_gameMutex);
	if (!m_game)
		throw ServerException(__FILE__, __LINE__, ERR_SOCK_INVALID_STATE, 0);
	return m_game;
}

const GameData &
ServerGame::GetGameData() const
{
	return m_gameData;
}

const StartData &
ServerGame::GetStartData() const
{
	boost::mutex::scoped_lock lock(m_startDataMutex);
	return m_startData;
}

void
ServerGame::SetStartData(const StartData &startData)
{
	boost::mutex::scoped_lock lock(m_startDataMutex);
	m_startData = startData;
}

bool
ServerGame::IsPasswordProtected() const
{
	return !m_password.empty();
}

bool
ServerGame::CheckPassword(const string &password) const
{
	return Tools::ConstantTimeStringCompare(password, m_password);
}

bool
ServerGame::CheckSettings(const GameData &data, const string &password, ServerMode mode)
{
	bool retVal = true;

	// Validate enum ranges - reject unknown values that could bypass conditional checks.
	if (data.gameType < GAME_TYPE_NORMAL || data.gameType > GAME_TYPE_RANKING) {
		retVal = false;
	}
	if (data.raiseIntervalMode < RAISE_ON_HANDNUMBER || data.raiseIntervalMode > RAISE_ON_MINUTES) {
		retVal = false;
	}
	if (data.raiseMode < DOUBLE_BLINDS || data.raiseMode > MANUAL_BLINDS_ORDER) {
		retVal = false;
	}
	if (data.afterManualBlindsMode < AFTERMB_DOUBLE_BLINDS || data.afterManualBlindsMode > AFTERMB_STAY_AT_LAST_BLIND) {
		retVal = false;
	}

	if (mode != SERVER_MODE_LAN) {
		if (data.playerActionTimeoutSec < 5 || data.playerActionTimeoutSec > 60) {
			retVal = false;
		}
	}
	if (data.maxNumberOfPlayers < 2 || data.maxNumberOfPlayers > MAX_NUMBER_OF_PLAYERS) {
		retVal = false;
	}
	if (data.firstSmallBlind < 1) {
		retVal = false;
	}
	if (data.startMoney < 1) {
		retVal = false;
	}
	if (data.raiseIntervalMode == RAISE_ON_HANDNUMBER && data.raiseSmallBlindEveryHandsValue < 1) {
		retVal = false;
	}
	if (data.raiseIntervalMode == RAISE_ON_TIME && data.raiseSmallBlindEveryMinutesValue < 1) {
		retVal = false;
	}
	if (data.raiseMode == MANUAL_BLINDS_ORDER && data.manualBlindsList.empty()) {
		retVal = false;
	}
	if (data.afterManualBlindsMode == AFTERMB_RAISE_ABOUT && data.afterMBAlwaysRaiseValue < 1) {
		retVal = false;
	}
	if (data.gameType == GAME_TYPE_RANKING) {
		if ((data.startMoney != RANKING_GAME_START_CASH)
				|| (data.maxNumberOfPlayers != RANKING_GAME_NUMBER_OF_PLAYERS)
				|| (data.firstSmallBlind != RANKING_GAME_START_SBLIND)
				|| (data.raiseIntervalMode != RAISE_ON_HANDNUMBER)
				|| (data.raiseMode != DOUBLE_BLINDS)
				|| (data.raiseSmallBlindEveryHandsValue != RANKING_GAME_RAISE_EVERY_HAND)
				|| (!password.empty())) {
			retVal = false;
		}
	}
	return retVal;
}

GuiInterface &
ServerGame::GetGui()
{
	return m_gui;
}

unsigned
ServerGame::GetNextGameNum()
{
	constexpr unsigned MAX_SAFE_GAME_NUM = std::numeric_limits<unsigned>::max() - 1000;
	if (m_gameNum >= MAX_SAFE_GAME_NUM) {
		LOG_ERROR("Game number counter near overflow - server capacity exhausted");
		throw ServerException(__FILE__, __LINE__, ERR_NET_SERVER_FULL, 0);
	}
	return m_gameNum++;
}

void
ServerGame::AddPlayerToNumJoinsPerPlayer(const std::string &playerName)
{
	boost::mutex::scoped_lock lock(m_numJoinsPerPlayerMutex);
	NumJoinsPerPlayerMap::iterator pos = m_numJoinsPerPlayer.find(playerName);
	if (pos != m_numJoinsPerPlayer.end()) {
		pos->second++;
	} else {
		m_numJoinsPerPlayer[playerName] = 1;
	}
}

int
ServerGame::GetNumJoinsPerPlayer(const std::string &playerName) const
{
	boost::mutex::scoped_lock lock(m_numJoinsPerPlayerMutex);
	int num = 0;
	NumJoinsPerPlayerMap::const_iterator pos = m_numJoinsPerPlayer.find(playerName);
	if (pos != m_numJoinsPerPlayer.end()) {
		num = pos->second;
	}
	return num;
}
