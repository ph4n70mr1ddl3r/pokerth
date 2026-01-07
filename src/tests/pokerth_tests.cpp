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

#include <boost/shared_ptr.hpp>
#include <net/netpacket.h>
#include <game_defs.h>
#include <gamedata.h>
#include <playerdata.h>
#include "pokerth_test_framework.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>
#include <map>
#include <random>
#include <cmath>

TEST_SUITE(NetPacket)

TEST(NetPacket_Create, TestCreate)
{
    NetPacket packet;
    EXPECT_TRUE(packet.GetMsg() != nullptr);
    return true;
}

TEST(NetPacket_GetMsg, TestGetMsg)
{
    NetPacket packet;
    const PokerTHMessage* msg = packet.GetMsg();
    EXPECT_TRUE(msg != nullptr);
    return true;
}

TEST(NetPacket_MessageType, TestMessageType)
{
    NetPacket packet;
    packet.GetMsg()->set_messagetype(PokerTHMessage_PokerTHMessageType_Type_AnnounceMessage);
    EXPECT_EQ(packet.GetMsg()->messagetype(), PokerTHMessage_PokerTHMessageType_Type_AnnounceMessage);
    return true;
}

TEST(NetPacket_Serialization, TestPacketSerialization)
{
    NetPacket packet;
    packet.GetMsg()->set_messagetype(PokerTHMessage_PokerTHMessageType_Type_AnnounceMessage);
    
    int size = packet.GetMsg()->ByteSizeLong();
    EXPECT_TRUE(size > 0);
    
    std::vector<unsigned char> buffer(size);
    packet.GetMsg()->SerializeWithCachedSizesToArray(buffer.data());
    EXPECT_TRUE(buffer.size() > 0);
    return true;
}

TEST(NetPacket_Deserialization, TestPacketDeserialization)
{
    NetPacket original;
    original.GetMsg()->set_messagetype(PokerTHMessage_PokerTHMessageType_Type_AnnounceMessage);
    
    int size = original.GetMsg()->ByteSizeLong();
    std::vector<unsigned char> buffer(size);
    original.GetMsg()->SerializeWithCachedSizesToArray(buffer.data());
    
    NetPacket restored;
    bool success = restored.GetMsg()->ParseFromArray(buffer.data(), size);
    EXPECT_TRUE(success);
    EXPECT_EQ(restored.GetMsg()->messagetype(), PokerTHMessage_PokerTHMessageType_Type_AnnounceMessage);
    return true;
}

END_TEST_SUITE

TEST_SUITE(GameDefinitions)

TEST(GameStateValues, TestGameStateConstants)
{
    EXPECT_EQ(GAME_STATE_PREFLOP, 0);
    EXPECT_EQ(GAME_STATE_FLOP, 1);
    EXPECT_EQ(GAME_STATE_TURN, 2);
    EXPECT_EQ(GAME_STATE_RIVER, 3);
    EXPECT_EQ(GAME_STATE_POST_RIVER, 4);
    return true;
}

TEST(PlayerActionValues, TestPlayerActionConstants)
{
    EXPECT_EQ(PLAYER_ACTION_NONE, 0);
    EXPECT_EQ(PLAYER_ACTION_FOLD, 1);
    EXPECT_EQ(PLAYER_ACTION_CHECK, 2);
    EXPECT_EQ(PLAYER_ACTION_CALL, 3);
    EXPECT_EQ(PLAYER_ACTION_BET, 4);
    EXPECT_EQ(PLAYER_ACTION_RAISE, 5);
    EXPECT_EQ(PLAYER_ACTION_ALLIN, 6);
    return true;
}

TEST(GameTypeValues, TestGameTypeConstants)
{
    EXPECT_EQ(GAME_TYPE_NORMAL, 1);
    EXPECT_EQ(GAME_TYPE_REGISTERED_ONLY, 2);
    EXPECT_EQ(GAME_TYPE_INVITE_ONLY, 3);
    EXPECT_EQ(GAME_TYPE_RANKING, 4);
    return true;
}

TEST(ServerModeValues, TestServerModeConstants)
{
    EXPECT_EQ(SERVER_MODE_LAN, 0);
    EXPECT_EQ(SERVER_MODE_INTERNET_NOAUTH, 1);
    EXPECT_EQ(SERVER_MODE_INTERNET_AUTH, 2);
    return true;
}

TEST(PlayerRightsValues, TestPlayerRightsConstants)
{
    EXPECT_EQ(PLAYER_RIGHTS_GUEST, 1);
    EXPECT_EQ(PLAYER_RIGHTS_NORMAL, 2);
    EXPECT_EQ(PLAYER_RIGHTS_ADMIN, 3);
    return true;
}

TEST(PlayerTypeValues, TestPlayerTypeConstants)
{
    EXPECT_EQ(PLAYER_TYPE_COMPUTER, 0);
    EXPECT_EQ(PLAYER_TYPE_HUMAN, 1);
    return true;
}

TEST(PlayerRightsBounds, TestPlayerRightsOrdering)
{
    EXPECT_TRUE(PLAYER_RIGHTS_GUEST < PLAYER_RIGHTS_NORMAL);
    EXPECT_TRUE(PLAYER_RIGHTS_NORMAL < PLAYER_RIGHTS_ADMIN);
    return true;
}

TEST(TransportProtocolValues, TestTransportProtocolConstants)
{
    EXPECT_EQ(TRANSPORT_PROTOCOL_TCP, 1);
    EXPECT_EQ(TRANSPORT_PROTOCOL_SCTP, 2);
    EXPECT_EQ(TRANSPORT_PROTOCOL_TCP_SCTP, 3);
    EXPECT_EQ(TRANSPORT_PROTOCOL_WEBSOCKET, 4);
    EXPECT_EQ(TRANSPORT_PROTOCOL_TCP_WEBSOCKET, 5);
    EXPECT_EQ(TRANSPORT_PROTOCOL_TCP_SCTP_WEBSOCKET, 7);
    return true;
}

TEST(PlayerActionLogValues, TestActionLogConstants)
{
    EXPECT_EQ(LOG_ACTION_NONE, 0);
    EXPECT_EQ(LOG_ACTION_DEALER, 1);
    EXPECT_EQ(LOG_ACTION_SMALL_BLIND, 2);
    EXPECT_EQ(LOG_ACTION_BIG_BLIND, 3);
    EXPECT_EQ(LOG_ACTION_FOLD, 4);
    EXPECT_EQ(LOG_ACTION_CHECK, 5);
    EXPECT_EQ(LOG_ACTION_CALL, 6);
    EXPECT_EQ(LOG_ACTION_BET, 7);
    EXPECT_EQ(LOG_ACTION_ALL_IN, 8);
    EXPECT_EQ(LOG_ACTION_SHOW, 9);
    EXPECT_EQ(LOG_ACTION_HAS, 10);
    EXPECT_EQ(LOG_ACTION_WIN, 11);
    EXPECT_EQ(LOG_ACTION_WIN_SIDE_POT, 12);
    EXPECT_EQ(LOG_ACTION_SIT_OUT, 13);
    EXPECT_EQ(LOG_ACTION_WIN_GAME, 14);
    EXPECT_EQ(LOG_ACTION_LEFT, 15);
    EXPECT_EQ(LOG_ACTION_KICKED, 16);
    EXPECT_EQ(LOG_ACTION_ADMIN, 17);
    EXPECT_EQ(LOG_ACTION_JOIN, 18);
    return true;
}

TEST(ActionCodeValues, TestActionCodeConstants)
{
    EXPECT_EQ(ACTION_CODE_VALID, 0);
    EXPECT_EQ(ACTION_CODE_INVALID_STATE, 1);
    EXPECT_EQ(ACTION_CODE_NOT_YOUR_TURN, 2);
    EXPECT_EQ(ACTION_CODE_NOT_ALLOWED, 3);
    return true;
}

TEST(ButtonValues, TestButtonConstants)
{
    EXPECT_EQ(BUTTON_NONE, 0);
    EXPECT_EQ(BUTTON_DEALER, 1);
    EXPECT_EQ(BUTTON_SMALL_BLIND, 2);
    EXPECT_EQ(BUTTON_BIG_BLIND, 3);
    return true;
}

TEST(RaiseModeValues, TestRaiseModeConstants)
{
    EXPECT_EQ(DOUBLE_BLINDS, 1);
    EXPECT_EQ(MANUAL_BLINDS_ORDER, 2);
    return true;
}

TEST(RaiseIntervalValues, TestRaiseIntervalConstants)
{
    EXPECT_EQ(RAISE_ON_HANDNUMBER, 1);
    EXPECT_EQ(RAISE_ON_MINUTES, 2);
    return true;
}

TEST(GameModeValues, TestGameModeConstants)
{
    EXPECT_EQ(GAME_MODE_CREATED, 1);
    EXPECT_EQ(GAME_MODE_STARTED, 2);
    EXPECT_EQ(GAME_MODE_CLOSED, 3);
    return true;
}

TEST(GameLimits, TestPlayerCountLimits)
{
    EXPECT_EQ(MIN_NUMBER_OF_PLAYERS, 2);
    EXPECT_EQ(MAX_NUMBER_OF_PLAYERS, 2);
    EXPECT_TRUE(MIN_NUMBER_OF_PLAYERS <= MAX_NUMBER_OF_PLAYERS);
    return true;
}

TEST(GUISpeedLimits, TestGUISpeedLimits)
{
    EXPECT_EQ(MIN_GUI_SPEED, 1);
    EXPECT_EQ(MAX_GUI_SPEED, 11);
    EXPECT_TRUE(MIN_GUI_SPEED < MAX_GUI_SPEED);
    return true;
}

TEST(RankingGameConstants, TestRankingGameValues)
{
    EXPECT_EQ(RANKING_GAME_START_CASH, 10000);
    EXPECT_EQ(RANKING_GAME_NUMBER_OF_PLAYERS, 10);
    EXPECT_EQ(RANKING_GAME_START_SBLIND, 50);
    EXPECT_EQ(RANKING_GAME_RAISE_EVERY_HAND, 11);
    return true;
}

TEST(VersionConstants, TestVersionNumbers)
{
    EXPECT_EQ(POKERTH_VERSION_MAJOR, 1);
    EXPECT_EQ(POKERTH_VERSION_MINOR, 11);
    return true;
}

TEST(VersionCalculation, TestVersionValue)
{
    EXPECT_EQ(POKERTH_VERSION, ((POKERTH_VERSION_MAJOR << 8) | POKERTH_VERSION_MINOR));
    EXPECT_EQ(POKERTH_VERSION, 267);
    return true;
}

TEST(VersionString, TestBetaReleaseString)
{
    std::string versionStr = POKERTH_BETA_RELEASE_STRING;
    EXPECT_EQ(versionStr, "1.1.2");
    return true;
}

END_TEST_SUITE

TEST_SUITE(GameDataTests)

TEST(GameDataDefault, TestDefaultGameData)
{
    GameData data;
    EXPECT_EQ(data.gameType, GAME_TYPE_NORMAL);
    EXPECT_TRUE(data.allowSpectators);
    EXPECT_EQ(data.maxNumberOfPlayers, 0);
    EXPECT_EQ(data.startMoney, 0);
    EXPECT_EQ(data.firstSmallBlind, 0);
    EXPECT_EQ(data.raiseIntervalMode, RAISE_ON_HANDNUMBER);
    EXPECT_EQ(data.raiseSmallBlindEveryHandsValue, 8);
    EXPECT_EQ(data.raiseSmallBlindEveryMinutesValue, 1);
    EXPECT_EQ(data.raiseMode, DOUBLE_BLINDS);
    EXPECT_EQ(data.afterManualBlindsMode, AFTERMB_DOUBLE_BLINDS);
    EXPECT_EQ(data.afterMBAlwaysRaiseValue, 0);
    EXPECT_EQ(data.guiSpeed, 4);
    EXPECT_EQ(data.delayBetweenHandsSec, 6);
    EXPECT_EQ(data.playerActionTimeoutSec, 20);
    return true;
}

TEST(GameDataCustom, TestCustomGameData)
{
    GameData data;
    data.gameType = GAME_TYPE_RANKING;
    data.maxNumberOfPlayers = 2;
    data.startMoney = 10000;
    data.firstSmallBlind = 50;
    data.guiSpeed = 8;
    data.playerActionTimeoutSec = 30;
    
    EXPECT_EQ(data.gameType, GAME_TYPE_RANKING);
    EXPECT_EQ(data.maxNumberOfPlayers, 2);
    EXPECT_EQ(data.startMoney, 10000);
    EXPECT_EQ(data.firstSmallBlind, 50);
    return true;
}

TEST(StartDataDefault, TestDefaultStartData)
{
    StartData data;
    EXPECT_EQ(data.startDealerPlayerId, 0);
    EXPECT_EQ(data.numberOfPlayers, 0);
    return true;
}

TEST(GameInfoDefault, TestDefaultGameInfo)
{
    GameInfo info;
    EXPECT_EQ(info.mode, GAME_MODE_CREATED);
    EXPECT_EQ(info.adminPlayerId, 0);
    EXPECT_FALSE(info.isPasswordProtected);
    EXPECT_TRUE(info.players.empty());
    EXPECT_TRUE(info.spectators.empty());
    EXPECT_TRUE(info.spectatorsDuringGame.empty());
    return true;
}

TEST(GameInfoWithPlayers, TestGameInfoWithPlayers)
{
    GameInfo info;
    info.name = "Test Game";
    info.adminPlayerId = 1;
    info.isPasswordProtected = true;
    info.players.push_back(1);
    info.players.push_back(2);
    info.players.push_back(3);
    
    EXPECT_EQ(info.name, "Test Game");
    EXPECT_EQ(info.adminPlayerId, 1);
    EXPECT_TRUE(info.isPasswordProtected);
    EXPECT_EQ(info.players.size(), 3);
    return true;
}

TEST(VoteKickDataDefault, TestDefaultVoteKickData)
{
    VoteKickData data;
    EXPECT_EQ(data.petitionId, 0);
    EXPECT_EQ(data.kickPlayerId, 0);
    EXPECT_EQ(data.numVotesToKick, 0);
    EXPECT_EQ(data.numVotesInFavourOfKicking, 0);
    EXPECT_EQ(data.numVotesAgainstKicking, 0);
    EXPECT_EQ(data.timeLimitSec, 0);
    EXPECT_TRUE(data.votedPlayerIds.empty());
    return true;
}

END_TEST_SUITE

TEST_SUITE(PlayerDataTests)

TEST(PlayerDataConstructor, TestPlayerDataInit)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, true);
    EXPECT_EQ(player.GetUniqueId(), 1);
    EXPECT_EQ(player.GetNumber(), 0);
    EXPECT_EQ(player.GetType(), PLAYER_TYPE_HUMAN);
    EXPECT_EQ(player.GetRights(), PLAYER_RIGHTS_NORMAL);
    EXPECT_TRUE(player.IsGameAdmin());
    return true;
}

TEST(PlayerDataName, TestPlayerName)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    player.SetName("TestPlayer");
    EXPECT_EQ(player.GetName(), "TestPlayer");
    return true;
}

TEST(PlayerDataCountry, TestPlayerCountry)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    player.SetCountry("US");
    EXPECT_EQ(player.GetCountry(), "US");
    return true;
}

TEST(PlayerDataType, TestPlayerType)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    player.SetType(PLAYER_TYPE_COMPUTER);
    EXPECT_EQ(player.GetType(), PLAYER_TYPE_COMPUTER);
    return true;
}

TEST(PlayerDataRights, TestPlayerRights)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_GUEST, false);
    player.SetRights(PLAYER_RIGHTS_ADMIN);
    EXPECT_EQ(player.GetRights(), PLAYER_RIGHTS_ADMIN);
    return true;
}

TEST(PlayerDataGameAdmin, TestGameAdmin)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    EXPECT_FALSE(player.IsGameAdmin());
    player.SetGameAdmin(true);
    EXPECT_TRUE(player.IsGameAdmin());
    return true;
}

TEST(PlayerDataNumber, TestPlayerSeatNumber)
{
    PlayerData player(1, 5, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    EXPECT_EQ(player.GetNumber(), 5);
    player.SetNumber(7);
    EXPECT_EQ(player.GetNumber(), 7);
    return true;
}

TEST(PlayerDataStartCash, TestPlayerStartCash)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    player.SetStartCash(5000);
    EXPECT_EQ(player.GetStartCash(), 5000);
    return true;
}

TEST(PlayerDataGuid, TestPlayerGuid)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    player.SetGuid("test-guid-123");
    EXPECT_EQ(player.GetGuid(), "test-guid-123");
    player.SetOldGuid("old-guid-456");
    EXPECT_EQ(player.GetOldGuid(), "old-guid-456");
    return true;
}

TEST(PlayerDataComparison, TestPlayerComparison)
{
    PlayerData player1(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    PlayerData player2(2, 1, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    EXPECT_TRUE(player1 < player2);
    EXPECT_FALSE(player2 < player1);
    return true;
}

TEST(PlayerDataLastGames, TestPlayerGamesHistory)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    
    player.AddPlayerLastGame(100);
    player.AddPlayerLastGame(200);
    player.AddPlayerLastGame(300);
    
    std::vector<long> games = player.GetPlayerLastGames();
    EXPECT_EQ(games.size(), 3);
    EXPECT_EQ(games[0], 100);
    EXPECT_EQ(games[1], 200);
    EXPECT_EQ(games[2], 300);
    return true;
}

TEST(PlayerDataSetLastGames, TestReplaceGamesHistory)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    
    player.AddPlayerLastGame(100);
    player.AddPlayerLastGame(200);
    
    std::vector<long> newGames = {400, 500, 600};
    player.SetPlayerLastGames(newGames);
    
    std::vector<long> games = player.GetPlayerLastGames();
    EXPECT_EQ(games.size(), 3);
    EXPECT_EQ(games[0], 400);
    EXPECT_EQ(games[1], 500);
    EXPECT_EQ(games[2], 600);
    return true;
}

TEST(PlayerDataCopy, TestPlayerDataCopyConstructor)
{
    PlayerData original(1, 5, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_ADMIN, true);
    original.SetName("Original");
    original.SetCountry("US");
    
    PlayerData copy(original);
    
    EXPECT_EQ(copy.GetUniqueId(), original.GetUniqueId());
    EXPECT_EQ(copy.GetNumber(), original.GetNumber());
    EXPECT_EQ(copy.GetName(), original.GetName());
    EXPECT_EQ(copy.GetType(), original.GetType());
    EXPECT_EQ(copy.GetRights(), original.GetRights());
    EXPECT_EQ(copy.IsGameAdmin(), original.IsGameAdmin());
    return true;
}

END_TEST_SUITE

TEST_SUITE(BoundaryTests)

TEST(BoundaryZeroValues, TestZeroPlayerId)
{
    PlayerData player(0, 0, PLAYER_TYPE_COMPUTER, PLAYER_RIGHTS_GUEST, false);
    EXPECT_EQ(player.GetUniqueId(), 0);
    EXPECT_EQ(player.GetNumber(), 0);
    return true;
}

TEST(BoundaryNegativeCash, TestNegativeCash)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    player.SetStartCash(-1000);
    EXPECT_EQ(player.GetStartCash(), -1000);
    return true;
}

TEST(BoundaryEmptyStrings, TestEmptyStrings)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    player.SetName("");
    EXPECT_EQ(player.GetName(), "");
    player.SetCountry("");
    EXPECT_EQ(player.GetCountry(), "");
    player.SetGuid("");
    EXPECT_EQ(player.GetGuid(), "");
    return true;
}

TEST(BoundaryLongStrings, TestLongString)
{
    PlayerData player(1, 0, PLAYER_TYPE_HUMAN, PLAYER_RIGHTS_NORMAL, false);
    std::string longName(1000, 'X');
    player.SetName(longName);
    EXPECT_EQ(player.GetName(), longName);
    return true;
}

TEST(BoundaryZeroGameValues, TestZeroGameValues)
{
    GameData data;
    data.maxNumberOfPlayers = 0;
    data.startMoney = 0;
    data.firstSmallBlind = 0;
    EXPECT_EQ(data.maxNumberOfPlayers, 0);
    EXPECT_EQ(data.startMoney, 0);
    EXPECT_EQ(data.firstSmallBlind, 0);
    return true;
}

END_TEST_SUITE

TEST_SUITE(RandomizedTests)

TEST(RandomCardDistribution, TestCardShuffling)
{
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 51);
    std::map<int, int> cardCounts;
    
    for (int i = 0; i < 5200; i++) {
        int card = dist(rng);
        cardCounts[card]++;
    }
    
    EXPECT_EQ(cardCounts.size(), 52);
    for (int card = 0; card < 52; card++) {
        EXPECT_TRUE(cardCounts[card] >= 80 && cardCounts[card] <= 120);
    }
    return true;
}

END_TEST_SUITE

TEST_SUITE(LogUploadTests)

TEST(LogUploadErrorCodes, TestLogUploadErrorEnum)
{
    EXPECT_EQ(LOG_UPLOAD_ERROR_NO_FILE, 1);
    EXPECT_EQ(LOG_UPLOAD_ERROR_OPEN_DB, 2);
    EXPECT_EQ(LOG_UPLOAD_ERROR_MAX_NUM_TOTAL, 3);
    EXPECT_EQ(LOG_UPLOAD_ERROR_MAX_NUM_IP, 4);
    EXPECT_EQ(LOG_UPLOAD_ERROR_FILE_SIZE, 5);
    EXPECT_EQ(LOG_UPLOAD_ERROR_FILE_EXT, 6);
    EXPECT_EQ(LOG_UPLOAD_ERROR_FILE_HEAD, 7);
    EXPECT_EQ(LOG_UPLOAD_ERROR_ID, 8);
    EXPECT_EQ(LOG_UPLOAD_ERROR_FILE_MOVE, 9);
    EXPECT_EQ(LOG_UPLOAD_ERROR_INSERT_DB, 10);
    return true;
}

TEST(LogUploadIdSize, TestLogUploadIdSize)
{
    EXPECT_EQ(LOG_UPLOAD_ID_SIZE, 40);
    return true;
}

END_TEST_SUITE

TEST_SUITE(VoteKickTests)

TEST(KickVoteValues, TestKickVoteEnum)
{
    EXPECT_EQ(KICK_VOTE_AGAINST, 0);
    EXPECT_EQ(KICK_VOTE_IN_FAVOUR, 1);
    return true;
}

TEST(DenyKickReasons, TestDenyKickReasons)
{
    EXPECT_EQ(KICK_DENIED_INVALID_STATE, 0);
    EXPECT_EQ(KICK_DENIED_TOO_FEW_PLAYERS, 1);
    EXPECT_EQ(KICK_DENIED_TEMPORARY, 2);
    EXPECT_EQ(KICK_DENIED_OTHER_IN_PROGRESS, 3);
    EXPECT_EQ(KICK_DENIED_INVALID_PLAYER_ID, 4);
    return true;
}

TEST(DenyVoteReasons, TestDenyVoteReasons)
{
    EXPECT_EQ(VOTE_DENIED_INVALID_PETITION, 0);
    EXPECT_EQ(VOTE_DENIED_ALREADY_VOTED, 1);
    return true;
}

TEST(PetitionEndReasons, TestPetitionEndReasons)
{
    EXPECT_EQ(PETITION_END_ENOUGH_VOTES, 0);
    EXPECT_EQ(PETITION_END_NOT_ENOUGH_PLAYERS, 1);
    EXPECT_EQ(PETITION_END_PLAYER_LEFT, 2);
    EXPECT_EQ(PETITION_END_TIMEOUT, 3);
    return true;
}

END_TEST_SUITE

TEST_SUITE(GameInvitationTests)

TEST(DenyGameInvitationReasons, TestDenyInvitationReasons)
{
    EXPECT_EQ(DENY_GAME_INVITATION_NO, 0);
    EXPECT_EQ(DENY_GAME_INVITATION_BUSY, 1);
    return true;
}

END_TEST_SUITE

TEST_SUITE(NetworkTimeoutTests)

TEST(NetTimeoutReasons, TestTimeoutReasonConstants)
{
    EXPECT_EQ(NETWORK_TIMEOUT_GENERIC, 0);
    EXPECT_EQ(NETWORK_TIMEOUT_GAME_ADMIN_IDLE, 1);
    EXPECT_EQ(NETWORK_TIMEOUT_KICK_AFTER_AUTOFOLD, 2);
    return true;
}

END_TEST_SUITE

TEST_SUITE(AvatarFileTests)

TEST(AvatarFileTypeValues, TestAvatarFileTypeEnum)
{
    EXPECT_EQ(AVATAR_FILE_TYPE_UNKNOWN, 0);
    EXPECT_EQ(AVATAR_FILE_TYPE_PNG, 1);
    EXPECT_EQ(AVATAR_FILE_TYPE_JPG, 2);
    EXPECT_EQ(AVATAR_FILE_TYPE_GIF, 3);
    return true;
}

TEST(PlayerInfoDefault, TestDefaultPlayerInfo)
{
    PlayerInfo info;
    EXPECT_EQ(info.ptype, PLAYER_TYPE_HUMAN);
    EXPECT_FALSE(info.isGuest);
    EXPECT_FALSE(info.isAdmin);
    EXPECT_FALSE(info.hasAvatar);
    EXPECT_EQ(info.avatarType, AVATAR_FILE_TYPE_UNKNOWN);
    return true;
}

END_TEST_SUITE
