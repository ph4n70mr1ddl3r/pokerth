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

#include <net/websendbuffer.h>
#include <net/asiosendbuffer.h>
#include <net/websocketdata.h>
#include <net/netpacket.h>
#include <net/sessiondata.h>
#include <core/loghelper.h>
#include <memory>
#include <limits>

using namespace std;


WebSendBuffer::WebSendBuffer()
	: closeAfterSend(false)
{
}

void
WebSendBuffer::SetCloseAfterSend()
{
	closeAfterSend = true;
}

void
WebSendBuffer::HandleWrite(boost::shared_ptr<boost::asio::ip::tcp::socket> /*socket*/, const boost::system::error_code &/*error*/)
{
}

void
WebSendBuffer::AsyncSendNextPacket(boost::shared_ptr<SessionData> session)
{
	if (closeAfterSend) {
		boost::shared_ptr<WebSocketData> webData = session->GetWebData();
		if (!webData)
			return;
#if defined(__GXX_EXPERIMENTAL_CXX0X__) || (__cplusplus >= 201103L)
		std::error_code std_ec;
		if (webData->isTls) {
			if (webData->webSocketTlsServer)
				webData->webSocketTlsServer->close(webData->webHandle, websocketpp::close::status::normal, "PokerTH server closed the connection.", std_ec);
		} else {
			if (webData->webSocketServer)
				webData->webSocketServer->close(webData->webHandle, websocketpp::close::status::normal, "PokerTH server closed the connection.", std_ec);
		}
#else
		boost::system::error_code ec;
		if (webData->isTls) {
			if (webData->webSocketTlsServer)
				webData->webSocketTlsServer->close(webData->webHandle, websocketpp::close::status::normal, "PokerTH server closed the connection.", ec);
		} else {
			if (webData->webSocketServer)
				webData->webSocketServer->close(webData->webHandle, websocketpp::close::status::normal, "PokerTH server closed the connection.", ec);
		}
#endif
	}
}

void
WebSendBuffer::InternalStorePacket(boost::shared_ptr<SessionData> session, boost::shared_ptr<NetPacket> packet)
{
	size_t rawSize = packet->GetMsg()->ByteSizeLong();
	if (rawSize > MAX_SEND_BUF_SIZE || rawSize > std::numeric_limits<uint32_t>::max()) {
		LOG_ERROR("WebSendBuffer: Packet too large: " << rawSize);
		return;
	}
	uint32_t packetSize = static_cast<uint32_t>(rawSize);
	auto buf = std::make_unique<google::protobuf::uint8[]>(packetSize);
	packet->GetMsg()->SerializeWithCachedSizesToArray(buf.get());

	boost::shared_ptr<WebSocketData> webData = session->GetWebData();
	if (!webData)
		return;
#if defined(__GXX_EXPERIMENTAL_CXX0X__) || (__cplusplus >= 201103L)
	std::error_code std_ec;
	if (webData->isTls) {
		if (webData->webSocketTlsServer)
			webData->webSocketTlsServer->send(webData->webHandle, string((const char *)buf.get(), packetSize), websocketpp::frame::opcode::BINARY, std_ec);
		else
			std_ec = make_error_code(std::errc::not_connected);
		if (std_ec) {
			SetCloseAfterSend();
		}
	} else {
		if (webData->webSocketServer)
			webData->webSocketServer->send(webData->webHandle, string((const char *)buf.get(), packetSize), websocketpp::frame::opcode::BINARY, std_ec);
		else
			std_ec = make_error_code(std::errc::not_connected);
		if (std_ec) {
			SetCloseAfterSend();
		}
	}
#else
	boost::system::error_code ec;
	if (webData->isTls) {
		if (webData->webSocketTlsServer)
			webData->webSocketTlsServer->send(webData->webHandle, string((const char *)buf.get(), packetSize), websocketpp::frame::opcode::BINARY, ec);
		else
			ec = boost::system::errc::make_error_code(boost::system::errc::not_connected);
		if (ec) {
			SetCloseAfterSend();
		}
	} else {
		if (webData->webSocketServer)
			webData->webSocketServer->send(webData->webHandle, string((const char *)buf.get(), packetSize), websocketpp::frame::opcode::BINARY, ec);
		else
			ec = boost::system::errc::make_error_code(boost::system::errc::not_connected);
		if (ec) {
			SetCloseAfterSend();
		}
	}
#endif
}

