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

#include <net/serverbanmanager.h>
#include <net/serverexception.h>
#include <net/socket_msg.h>
#include <algorithm>

using namespace std;

#ifdef BOOST_ASIO_HAS_STD_CHRONO
using namespace std::chrono;
#else
using namespace boost::chrono;
#endif

ServerBanManager::ServerBanManager(boost::shared_ptr<boost::asio::io_context> ioService)
	: m_ioService(ioService), m_curBanId(0)
{
}

ServerBanManager::~ServerBanManager() noexcept
{
}

void
ServerBanManager::SetAdminPlayerIds(const std::list<DB_id> &adminList)
{
	boost::mutex::scoped_lock lock(m_banMutex);
	m_adminPlayers.resize(adminList.size());
	copy(adminList.begin(), adminList.end(), m_adminPlayers.begin());
	sort(m_adminPlayers.begin(), m_adminPlayers.end());
}

void
ServerBanManager::BanPlayerName(const std::string &playerName, unsigned durationHours)
{
	boost::mutex::scoped_lock lock(m_banMutex);
	unsigned banId = GetNextBanId();

	TimedPlayerBan tmpBan;
	tmpBan.timer = InternalRegisterTimedBan(banId, durationHours);
	tmpBan.nameStr = playerName;
	m_banPlayerNameMap[banId] = tmpBan;
}

void
ServerBanManager::BanPlayerRegex(const string &playerRegex, unsigned durationHours)
{
	boost::mutex::scoped_lock lock(m_banMutex);

	TimedPlayerBan tmpBan;
	try {
		tmpBan.nameRegex = boost::regex(playerRegex, boost::regex::extended | boost::regex::icase);
	} catch (const boost::regex_error &e) {
		LOG_ERROR("BanPlayerRegex: Invalid regex '" << playerRegex << "': " << e.what());
		return;
	}
	unsigned banId = GetNextBanId();
	tmpBan.timer = InternalRegisterTimedBan(banId, durationHours);
	m_banPlayerNameMap[banId] = tmpBan;
}

void
ServerBanManager::BanIPAddress(const string &ipAddress, unsigned durationHours)
{
	boost::mutex::scoped_lock lock(m_banMutex);
	unsigned banId = GetNextBanId();

	TimedIPBan tmpBan;
	tmpBan.timer = InternalRegisterTimedBan(banId, durationHours);
	tmpBan.ipAddress = ipAddress;
	m_banIPAddressMap[banId] = tmpBan;
}

bool
ServerBanManager::UnBan(unsigned banId)
{
	bool retVal = false;
	boost::mutex::scoped_lock lock(m_banMutex);
	RegexMap::iterator posNick = m_banPlayerNameMap.find(banId);
	if (posNick != m_banPlayerNameMap.end()) {
		if (posNick->second.timer)
			posNick->second.timer->cancel();
		m_banPlayerNameMap.erase(posNick);
		retVal = true;
	} else {
		IPAddressMap::iterator posIP = m_banIPAddressMap.find(banId);
		if (posIP != m_banIPAddressMap.end()) {
			if (posIP->second.timer)
				posIP->second.timer->cancel();
			m_banIPAddressMap.erase(posIP);
			retVal = true;
		}
	}
	return retVal;
}

void
ServerBanManager::GetBanList(list<string> &list) const
{
	boost::mutex::scoped_lock lock(m_banMutex);
	RegexMap::const_iterator i_nick = m_banPlayerNameMap.begin();
	RegexMap::const_iterator end_nick = m_banPlayerNameMap.end();
	while (i_nick != end_nick) {
		ostringstream banText;
		if ((*i_nick).second.nameStr.empty())
			banText << (*i_nick).first << ": (nickRegex) - " << (*i_nick).second.nameRegex.str();
		else
			banText << (*i_nick).first << ": (nickStr) - " << (*i_nick).second.nameStr;

		if ((*i_nick).second.timer) {
			auto expiry = (*i_nick).second.timer->expiry();
			auto now = steady_clock::now();
			if (expiry > now)
				banText << " duration: " << duration_cast<hours>(expiry - now).count() << "h";
			else
				banText << " (expired)";
		}
		list.push_back(banText.str());
		++i_nick;
	}
	IPAddressMap::const_iterator i_ip = m_banIPAddressMap.begin();
	IPAddressMap::const_iterator end_ip = m_banIPAddressMap.end();
	while (i_ip != end_ip) {
		ostringstream banText;
		banText << (*i_ip).first << ": (IP) - " << (*i_ip).second.ipAddress;
		if ((*i_ip).second.timer) {
			auto expiry = (*i_ip).second.timer->expiry();
			auto now = steady_clock::now();
			if (expiry > now)
				banText << " duration: " << duration_cast<hours>(expiry - now).count() << "h";
			else
				banText << " (expired)";
		}
		list.push_back(banText.str());
		++i_ip;
	}
}

void
ServerBanManager::ClearBanList()
{
	boost::mutex::scoped_lock lock(m_banMutex);
	for (auto &entry : m_banPlayerNameMap) {
		if (entry.second.timer) entry.second.timer->cancel();
	}
	for (auto &entry : m_banIPAddressMap) {
		if (entry.second.timer) entry.second.timer->cancel();
	}
	m_banPlayerNameMap.clear();
	m_banIPAddressMap.clear();
}

bool
ServerBanManager::IsAdminPlayer(DB_id playerId) const
{
	bool retVal = false;
	if (playerId != DB_ID_INVALID) {
		boost::mutex::scoped_lock lock(m_banMutex);

		if (binary_search(m_adminPlayers.begin(), m_adminPlayers.end(), playerId)) {
			retVal = true;
		}
	}
	return retVal;
}

bool
ServerBanManager::IsPlayerBanned(const std::string &name) const
{
	bool retVal = false;
	boost::mutex::scoped_lock lock(m_banMutex);
	RegexMap::const_iterator i = m_banPlayerNameMap.begin();
	RegexMap::const_iterator end = m_banPlayerNameMap.end();
	while (i != end) {
		// Use regex only if name not set.
		if ((*i).second.nameStr.empty()) {
			if (regex_match(name, (*i).second.nameRegex)) {
				retVal = true;
				break;
			}
		} else {
			if (name == (*i).second.nameStr) {
				retVal = true;
				break;
			}
		}
		++i;
	}

	return retVal;
}

bool
ServerBanManager::IsIPAddressBanned(const std::string &ipAddress) const
{
	bool retVal = false;
	boost::mutex::scoped_lock lock(m_banMutex);
	IPAddressMap::const_iterator i = m_banIPAddressMap.begin();
	IPAddressMap::const_iterator end = m_banIPAddressMap.end();
	while (i != end) {
		if (ipAddress == (*i).second.ipAddress) {
			retVal = true;
			break;
		}
		++i;
	}

	return retVal;
}

void
ServerBanManager::InitGameNameBadWordList(const std::list<string> &badWordList)
{
	boost::mutex::scoped_lock lock(m_banMutex);
	m_gameNameBadWordFilter.clear();
	for (const auto &word : badWordList) {
		try {
			m_gameNameBadWordFilter.push_back(boost::regex(word, boost::regex::extended | boost::regex::icase));
		} catch (const boost::regex_error &e) {
			LOG_ERROR("InitGameNameBadWordList: Invalid regex '" << word << "': " << e.what());
		}
	}
}

bool
ServerBanManager::IsBadGameName(const std::string &name) const
{
	bool retVal = false;
	boost::mutex::scoped_lock lock(m_banMutex);
	for (const auto &regex : m_gameNameBadWordFilter) {
		if (regex_match(name, regex)) {
			retVal = true;
			break;
		}
	}
	return retVal;
}

boost::shared_ptr<boost::asio::steady_timer>
ServerBanManager::InternalRegisterTimedBan(unsigned timerId, unsigned durationHours)
{
	boost::shared_ptr<boost::asio::steady_timer> tmpTimer;
	if (durationHours) {
		tmpTimer = boost::make_shared<boost::asio::steady_timer>(*m_ioService);
		tmpTimer->expires_after(hours(durationHours));
		tmpTimer->async_wait(
			[self = shared_from_this(), timerId, tmpTimer](const boost::system::error_code& ec) { self->TimerRemoveBan(ec, timerId, tmpTimer); });
	}
	return tmpTimer;
}

void
ServerBanManager::TimerRemoveBan(const boost::system::error_code &ec, unsigned banId, boost::shared_ptr<boost::asio::steady_timer> timer)
{
	if (!ec && timer)
		UnBan(banId);
}

	unsigned
ServerBanManager::GetNextBanId()
{
	unsigned startId = m_curBanId;
	// Use checked counter to handle startId==0 case: when m_curBanId wraps
	// from UINT_MAX to 0 (forced to 1), it can never equal startId==0,
	// causing an infinite loop. Track iterations to detect full cycle.
	unsigned checked = 0;
	do {
		m_curBanId++;
		if (m_curBanId == 0)
			m_curBanId = 1;
		if (++checked == 0) // wrapped around - checked all possible IDs
			break;
		if (m_banPlayerNameMap.find(m_curBanId) == m_banPlayerNameMap.end()
			&& m_banIPAddressMap.find(m_curBanId) == m_banIPAddressMap.end()) {
			return m_curBanId;
		}
	} while (true);
	throw ServerException(__FILE__, __LINE__, ERR_SOCK_INTERNAL, 0);
}

