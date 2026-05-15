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
#include "cleanerserver.h"

#include <QtNetwork>
#include <QtCore>
#include <QtEndian>
#include <QCryptographicHash>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>
#include <third_party/protobuf/chatcleaner.pb.h>

#include "messagefilter.h"
#include "cleanerconfig.h"

using namespace std;

CleanerServer::CleanerServer(): config(nullptr), blockConnection(false), m_recvBufUsed(0)
{
	config = std::make_unique<CleanerConfig>();

	clientSecret = QString::fromUtf8(config->readConfigString("ClientAuthString").c_str());
	serverSecret = QString::fromUtf8(config->readConfigString("ServerAuthString").c_str());

	myMessageFilter = std::make_unique<MessageFilter>(config);
	tcpServer = std::make_unique<QTcpServer>();
	tcpServer->setMaxPendingConnections(1);

	int listenPort = config->readConfigInt("DefaultListenPort");
	if (listenPort < 0 || listenPort > 65535) {
		qDebug() << "Invalid listen port:" << listenPort;
		return;
	}
	if (!tcpServer->listen(QHostAddress(QString::fromUtf8(config->readConfigString("HostAddress").c_str())), static_cast<quint16>(listenPort)) ) {
		qDebug() << QString("Unable to start the server: %1.").arg(tcpServer->errorString());
		return;
	}
	qDebug() << QString("The server is running on port %1.").arg(tcpServer->serverPort());

	configRefreshTimer = std::make_unique<QTimer>();

	connect(configRefreshTimer.get(), &QTimer::timeout, this, &CleanerServer::refreshConfig);
	connect(tcpServer.get(), &QTcpServer::newConnection, this, &CleanerServer::newCon);

	refreshConfig();
	configRefreshTimer->start(10000);
}

CleanerServer::~CleanerServer() noexcept = default;


void CleanerServer::newCon()
{
	if(!blockConnection) {
		if (tcpSocket) {
			tcpSocket->disconnect(this);
			tcpSocket->close();
			tcpSocket->deleteLater();
			tcpSocket = nullptr;
		}
		m_authenticated = false;
		tcpSocket = tcpServer->nextPendingConnection();
		if (!tcpSocket) {
			blockConnection = false;
			tcpServer->resumeAccepting();
			return;
		}
		connect(tcpSocket, &QTcpSocket::readyRead, this, &CleanerServer::onRead);
		connect(tcpSocket, &QTcpSocket::stateChanged, this, &CleanerServer::socketStateChanged);
		blockConnection = true;
		tcpServer->pauseAccepting();
	}
}

void CleanerServer::onRead()
{
	if (m_recvBufUsed >= sizeof(m_recvBuf)) {
		qDebug() << "Buffer overflow detected, closing connection";
		m_recvBufUsed = 0;
		if (tcpSocket) {
			tcpSocket->close();
		}
		return;
	}
	
	if (!tcpSocket) {
		qDebug() << "Cannot read from null socket";
		return;
	}
	qint64 bytesRead = tcpSocket->read(reinterpret_cast<char *>(m_recvBuf) + m_recvBufUsed, sizeof(m_recvBuf) - m_recvBufUsed);
	bool error = bytesRead < 1;
	if (!error) {
		m_recvBufUsed += bytesRead;
		bool valid;
		do {
			valid = false;
			if (m_recvBufUsed >= CLEANER_NET_HEADER_SIZE) {
				// Read the size of the packet (first 4 bytes in network byte order).
				uint32_t nativeVal;
				memcpy(&nativeVal, &m_recvBuf[0], sizeof(uint32_t));
				size_t packetSize = qFromBigEndian(nativeVal);
				if (packetSize == 0 || packetSize > MAX_CLEANER_PACKET_SIZE) {
					m_recvBufUsed = 0;
					qDebug() << "Invalid packet size: " << packetSize;
					error = true;
				} else if (m_recvBufUsed >= packetSize + CLEANER_NET_HEADER_SIZE) {
					try {
						boost::shared_ptr<ChatCleanerMessage> recvMsg(ChatCleanerMessage::default_instance().New());
						if (recvMsg->ParseFromArray(&m_recvBuf[CLEANER_NET_HEADER_SIZE], static_cast<int>(packetSize))) {
							m_recvBufUsed -= (packetSize + CLEANER_NET_HEADER_SIZE);
							if (m_recvBufUsed) {
								memmove(m_recvBuf, m_recvBuf + packetSize + CLEANER_NET_HEADER_SIZE, m_recvBufUsed);
							}
							error = handleMessage(*recvMsg);
							valid = true;
						} else {
							m_recvBufUsed = 0;
							qDebug() << "Failed to parse protobuf message";
							error = true;
						}
					} catch (const exception &e) {
						if (m_recvBufUsed >= packetSize + CLEANER_NET_HEADER_SIZE) {
							m_recvBufUsed -= (packetSize + CLEANER_NET_HEADER_SIZE);
							if (m_recvBufUsed) {
								memmove(m_recvBuf, m_recvBuf + packetSize + CLEANER_NET_HEADER_SIZE, m_recvBufUsed);
							}
						} else {
							m_recvBufUsed = 0;
						}
						qDebug() << "Exception while decoding packet: " << e.what();
						error = true;
					}
				}
			}
		} while (valid && !error);
	}

	if (error) {
		qDebug() << "Error handling packets from client.";
		blockConnection = false;
		if (tcpSocket) {
			tcpSocket->disconnect(this);
			tcpSocket->close();
			tcpSocket->deleteLater();
			tcpSocket = nullptr;
		}
	}


}

bool CleanerServer::handleMessage(ChatCleanerMessage &msg)
{
	bool error = false;
	if (msg.messagetype() == ChatCleanerMessage::Type_CleanerInitMessage) {
		const CleanerInitMessage &netInit = msg.cleanerinitmessage();
		if (netInit.requestedversion() == CLEANER_PROTOCOL_VERSION) {
			QString receivedSecret = QString::fromStdString(netInit.clientsecret());
			QByteArray expectedBytes = clientSecret.toUtf8();
			QByteArray receivedBytes = receivedSecret.toUtf8();
			volatile unsigned char result = 0;
			const auto maxLen = std::max(expectedBytes.size(), receivedBytes.size());
			for (decltype(maxLen) i = 0; i < maxLen; ++i) {
				volatile unsigned char eb = (i < expectedBytes.size()) ? static_cast<unsigned char>(expectedBytes[i]) : 0;
				volatile unsigned char rb = (i < receivedBytes.size()) ? static_cast<unsigned char>(receivedBytes[i]) : 0;
				result |= eb ^ rb;
			}
			// Use bitwise & instead of logical && to prevent short-circuit evaluation,
			// ensuring truly constant-time comparison resistant to timing side-channel attacks.
			volatile bool sizeMatch = (expectedBytes.size() == receivedBytes.size());
			volatile bool contentMatch = (result == 0);
			bool secretMatch = contentMatch & sizeMatch;
			if (secretMatch) {
				error = false;
				m_authenticated = true;

				boost::shared_ptr<ChatCleanerMessage> tmpAck(ChatCleanerMessage::default_instance().New());
				tmpAck->set_messagetype(ChatCleanerMessage::Type_CleanerInitAckMessage);
				CleanerInitAckMessage *netAck = tmpAck->mutable_cleanerinitackmessage();
				netAck->set_serverversion(CLEANER_PROTOCOL_VERSION);
				sendMessageToClient(*tmpAck);
			} else {
				qDebug() << "Invalid client secret.";
				error = true;
			}
		} else {
			qDebug() << "Invalid client version: " << netInit.requestedversion();
			error = true;
		}
	} else if (msg.messagetype() == ChatCleanerMessage::Type_CleanerChatRequestMessage) {
		if (!m_authenticated) {
			qDebug() << "Chat request received before authentication";
			return true;
		}
		error = false;
		const CleanerChatRequestMessage &netRequest = msg.cleanerchatrequestmessage();
		unsigned playerId = netRequest.playerid();
		QString nick(QString::fromUtf8(netRequest.playername().c_str()));
		QString message(QString::fromUtf8(netRequest.chatmessage().c_str()));
		unsigned gameId = netRequest.gameid();

		QStringList checkreturn = myMessageFilter->check(gameId, playerId, nick, message);
		QString checkAction;
		QString checkMessage;
		if (checkreturn.size() >= 2) {
			checkAction = checkreturn.at(0);
			checkMessage = checkreturn.at(1);
		}

		if (!checkAction.isEmpty()) {
			boost::shared_ptr<ChatCleanerMessage> tmpReply(ChatCleanerMessage::default_instance().New());
			tmpReply->set_messagetype(ChatCleanerMessage::Type_CleanerChatReplyMessage);
			CleanerChatReplyMessage *netReply = tmpReply->mutable_cleanerchatreplymessage();
			netReply->set_requestid(netRequest.requestid());
			netReply->set_gameid(netRequest.gameid());
			netReply->set_cleanerchattype(netRequest.cleanerchattype());
			netReply->set_playerid(netRequest.playerid());

			if(checkAction == "warn") {
				netReply->set_cleaneractiontype(CleanerChatReplyMessage_CleanerActionType_cleanerActionWarning);
			} else if (checkAction == "kick") {
				netReply->set_cleaneractiontype(CleanerChatReplyMessage_CleanerActionType_cleanerActionKick);
			} else if (checkAction == "kickban") {
				netReply->set_cleaneractiontype(CleanerChatReplyMessage_CleanerActionType_cleanerActionBan);
			} else if (checkAction == "mute") {
				netReply->set_cleaneractiontype(CleanerChatReplyMessage_CleanerActionType_cleanerActionMute);
			}

			netReply->set_cleanertext(checkMessage.toUtf8());
			sendMessageToClient(*tmpReply);
		}
	} else {
		qDebug() << "Unknown message type received: " << msg.messagetype();
	}
	return error;
}

void CleanerServer::socketStateChanged(QAbstractSocket::SocketState state)
{
	qDebug() << "Socket state changed to: " << state;
	if (state == QAbstractSocket::UnconnectedState) {
		if (tcpSocket) {
			tcpSocket->disconnect(this);
			tcpSocket->deleteLater();
			tcpSocket = nullptr;
		}
		m_authenticated = false;
		blockConnection = false;
		tcpServer->resumeAccepting();
	}
}

void CleanerServer::refreshConfig()
{

	QFileInfo configFileInfo(QString::fromUtf8(config->getConfigFileName().c_str()));

	if(configFileInfo.lastModified().secsTo(QDateTime::currentDateTime()) < 20) {
		config->fillBuffer();
	}

	myMessageFilter->refreshConfig();
}

void CleanerServer::sendMessageToClient(ChatCleanerMessage &msg)
{
	if (!tcpSocket) {
		qDebug() << "Cannot send message: socket is null";
		return;
	}
	if (tcpSocket->state() != QAbstractSocket::ConnectedState) {
		qDebug() << "Cannot send message: socket not connected";
		return;
	}
	size_t rawSize = msg.ByteSizeLong();
	if (rawSize > MAX_CLEANER_PACKET_SIZE || rawSize > std::numeric_limits<uint32_t>::max()) {
		qDebug() << "Chat cleaner send packet too large:" << rawSize;
		return;
	}
	uint32_t packetSize = static_cast<uint32_t>(rawSize);
	std::vector<google::protobuf::uint8> buf(packetSize + CLEANER_NET_HEADER_SIZE);
	uint32_t beSize = qToBigEndian(packetSize);
	memcpy(buf.data(), &beSize, sizeof(uint32_t));
	msg.SerializeWithCachedSizesToArray(buf.data() + CLEANER_NET_HEADER_SIZE);
	qint64 written = tcpSocket->write(reinterpret_cast<const char*>(buf.data()), packetSize + CLEANER_NET_HEADER_SIZE);
	if (written < 0) {
		qDebug() << "Failed to write to socket:" << tcpSocket->errorString();
	}
}

