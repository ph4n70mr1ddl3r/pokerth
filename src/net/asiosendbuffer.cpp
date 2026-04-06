/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2013 Felix Hammer, Florian Thauer, Lothar May          *
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

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio/ssl.hpp>

#include <net/asiosendbuffer.h>
#include <net/sessiondata.h>
#include <net/netpacket.h>
#include <net/netexception.h>
#include <net/socket_msg.h>
#include <core/loghelper.h>
#include <cstring>
#include <utility>
#include <limits>

using namespace std;


AsioSendBuffer::AsioSendBuffer()
	: sendBufUsed(0), curWriteBufUsed(0), closeAfterSend(false)
{
}

AsioSendBuffer::~AsioSendBuffer() noexcept
{
}

void
AsioSendBuffer::SetCloseAfterSend()
{
	closeAfterSend = true;
}

void
AsioSendBuffer::HandleWrite(boost::shared_ptr<boost::asio::ip::tcp::socket> socket, const boost::system::error_code &error)
{
	if (!error) {
		boost::mutex::scoped_lock lock(dataMutex);
		curWriteBufUsed = 0;
		AsyncSendNextPacket(socket);
	} else {
		LOG_ERROR("Async TCP write failed: " << error.message());
		boost::mutex::scoped_lock lock(dataMutex);
		curWriteBufUsed = 0;
		boost::system::error_code ec;
		socket->close(ec);
	}
}

// Implementierung mit exakt passender Signatur (any_io_executor)
void
AsioSendBuffer::HandleWriteSsl(boost::shared_ptr<boost::asio::ssl::stream<boost::asio::basic_stream_socket<boost::asio::ip::tcp, boost::asio::any_io_executor>>> sslStream, const boost::system::error_code &error)
{
    if (!error) {
        boost::mutex::scoped_lock lock(dataMutex);
        curWriteBufUsed = 0;
        AsyncSendNextPacketSsl(sslStream);
    } else {
        LOG_ERROR("Async SSL write failed: " << error.message());
        boost::mutex::scoped_lock lock(dataMutex);
        curWriteBufUsed = 0;
        boost::system::error_code ec;
        sslStream->lowest_layer().close(ec);
    }
}

void
AsioSendBuffer::AsyncSendNextPacket(boost::shared_ptr<SessionData> session)
{
    if (session->IsSsl()) {
        AsyncSendNextPacketSsl(session->GetSslStream());
    } else {
        AsyncSendNextPacket(session->GetAsioSocket());
    }
}

void
AsioSendBuffer::AsyncSendNextPacket(boost::shared_ptr<boost::asio::ip::tcp::socket> socket)
{
    if (!curWriteBufUsed) {
        sendBuf.swap(curWriteBuf);
        std::swap(curWriteBufUsed, sendBufUsed);
        if (curWriteBufUsed) {
            boost::asio::async_write(
				*socket,
				boost::asio::buffer(curWriteBuf.data(), curWriteBufUsed),
				boost::bind(&SendBuffer::HandleWrite,
							shared_from_this(),
							socket,
							boost::asio::placeholders::error));
        } else if (closeAfterSend) {
            boost::system::error_code ec;
            socket->close(ec);
        }
    }
}

void
AsioSendBuffer::AsyncSendNextPacketSsl(boost::shared_ptr<boost::asio::ssl::stream<boost::asio::basic_stream_socket<boost::asio::ip::tcp, boost::asio::any_io_executor>>> sslStream)
{
    if (!curWriteBufUsed) {
        sendBuf.swap(curWriteBuf);
        std::swap(curWriteBufUsed, sendBufUsed);
        if (curWriteBufUsed) {
            boost::asio::async_write(
                *sslStream,
                boost::asio::buffer(curWriteBuf.data(), curWriteBufUsed),
                boost::bind(&AsioSendBuffer::HandleWriteSsl,
                            boost::static_pointer_cast<AsioSendBuffer>(shared_from_this()),
                            sslStream,
                            boost::asio::placeholders::error));
        } else if (closeAfterSend) {
            boost::system::error_code ec;
            sslStream->lowest_layer().close(ec);
        }
    }
}

void
AsioSendBuffer::InternalStorePacket(boost::shared_ptr<SessionData> /*session*/, boost::shared_ptr<NetPacket> packet)
{
	size_t rawSize = packet->GetMsg()->ByteSizeLong();
	if (rawSize > MAX_SEND_BUF_SIZE || rawSize > std::numeric_limits<uint32_t>::max()) {
		return;
	}
	uint32_t packetSize = static_cast<uint32_t>(rawSize);
	std::vector<google::protobuf::uint8> buf(packetSize + NET_HEADER_SIZE);
	uint32_t netSize = htonl(packetSize);
	std::memcpy(buf.data(), &netSize, sizeof(netSize));
	packet->GetMsg()->SerializeWithCachedSizesToArray(&buf[NET_HEADER_SIZE]);
	if (EncodeToBuf(buf.data(), packetSize + NET_HEADER_SIZE) != 0) {
		LOG_ERROR("Failed to encode packet - send buffer full");
		return;
	}
}

int
AsioSendBuffer::EncodeToBuf(const void *data, size_t size)
{
    // Realloc buffer if necessary.
    while (GetSendBufLeft() < size) {
        if (!ReallocSendBuf()) {
            return -1;
        }
    }

    AppendToSendBufWithoutCheck(static_cast<const char*>(data), size);

    return 0;
}

// --- Buffer helper implementations ----------------------------------------
size_t
AsioSendBuffer::GetSendBufLeft() const
{
    return (sendBuf.size() > sendBufUsed) ? (sendBuf.size() - sendBufUsed) : 0;
}

bool
AsioSendBuffer::ReallocSendBuf()
{
    size_t newSize = sendBuf.empty() ? SEND_BUF_FIRST_ALLOC_CHUNKSIZE : sendBuf.size() * 2;
    if (newSize > MAX_SEND_BUF_SIZE)
        newSize = MAX_SEND_BUF_SIZE;
    if (newSize <= sendBuf.size())
        return false;

    sendBuf.resize(newSize);
    return true;
}

void
AsioSendBuffer::AppendToSendBufWithoutCheck(const char *data, size_t size)
{
    if (sendBuf.size() < sendBufUsed + size)
        throw NetException(__FILE__, __LINE__, ERR_NET_BUF_INVALID_SIZE, 0);
    std::memcpy(sendBuf.data() + sendBufUsed, data, size);
    sendBufUsed += size;
}

