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
/* Generic server database implementation. Basically does nothing. */

#ifndef _SERVERDBNOACTION_H_
#define _SERVERDBNOACTION_H_

#include <db/serverdbinterface.h>


class ServerDBNoAction : public ServerDBInterface
{
public:
	ServerDBNoAction();
	virtual ~ServerDBNoAction() noexcept;

	virtual void Init(const std::string &/*host*/, const std::string &/*user*/, const std::string &/*pwd*/,
					  const std::string &/*database*/, const std::string &/*encryptionKey*/) override {}

	virtual void Start() override {}
	virtual void Stop() override {}

	virtual void AsyncPlayerLogin(unsigned /*requestId*/, const std::string &/*playerName*/) override {}
	virtual void AsyncCheckAvatarBlacklist(unsigned /*requestId*/, const std::string &/*avatarHash*/) override {}
	virtual void PlayerPostLogin(DB_id /*playerId*/, const std::string &/*avatarHash*/, const std::string &/*avatarType*/) override {}
	virtual void PlayerLogout(DB_id /*playerId*/) override {}

	virtual void AsyncCreateGame(unsigned /*requestId*/, const std::string &/*gameName*/) override {}
	virtual void SetGamePlayerPlace(unsigned /*requestId*/, DB_id /*playerId*/, unsigned /*place*/) override {}
	virtual void SetPlayerLastGames(unsigned /*requestId*/, DB_id /*playerId*/, const std::vector<long> &/*last_games*/, const std::string &/*playerIp*/) override {}
	virtual void EndGame(unsigned /*requestId*/) override {}

	virtual void AsyncReportAvatar(unsigned /*requestId*/, unsigned /*replyId*/, DB_id /*reportedPlayerId*/, const std::string &/*avatarHash*/, const std::string &/*avatarType*/, DB_id * /*byPlayerId*/) override {}
	virtual void AsyncReportGame(unsigned /*requestId*/, unsigned /*replyId*/, DB_id * /*creatorPlayerId*/, unsigned /*gameId*/, const std::string &/*gameName*/, DB_id * /*byPlayerId*/) override {}

	virtual void AsyncQueryAdminPlayers(unsigned /*requestId*/) override {}
	virtual void AsyncBlockPlayer(unsigned /*requestId*/, unsigned /*replyId*/, DB_id /*playerId*/, int /*valid*/, int /*active*/) override {}
};

#endif // _SERVERDBNOACTION_H_
