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

#include <net/serveracceptwebhelper.h>
#include <net/sessiondata.h>
#include <net/webreceivebuffer.h>
#include <net/websocketdata.h>
#include <core/loghelper.h>

using namespace std;

ServerAcceptWebHelper::ServerAcceptWebHelper(ServerCallback &serverCallback, boost::shared_ptr<boost::asio::io_context> ioService,
		const string &webSocketResource, const string &webSocketOrigin, const bool &websocketTls)
	: m_ioService(ioService), m_serverCallback(serverCallback),
	  m_webSocketResource(webSocketResource), m_webSocketOrigin(webSocketOrigin)
{
	m_tls = websocketTls;
	if(m_tls) {
		m_webSocketTlsServer = boost::make_shared<tls_server>();
	} else {
		m_webSocketServer = boost::make_shared<server>();
	}
	
}

void
ServerAcceptWebHelper::Listen(unsigned serverPort, bool /*ipv6*/, const std::string &/*logDir*/, boost::shared_ptr<ServerLobbyThread> lobbyThread)
{
	m_lobbyThread = lobbyThread;


	if(m_tls) {
		// Set logging settings
#ifdef QT_NO_DEBUG
		m_webSocketTlsServer->clear_access_channels(websocketpp::log::alevel::all);
#else
		m_webSocketTlsServer->set_access_channels(websocketpp::log::alevel::all);
#endif

		m_webSocketTlsServer->init_asio(m_ioService.get());

		m_webSocketTlsServer->set_validate_handler([this](auto && conn_hdl) { return validate(conn_hdl); });
		m_webSocketTlsServer->set_open_handler([this](auto && conn_hdl) { on_open(conn_hdl); });
		m_webSocketTlsServer->set_close_handler([this](auto && conn_hdl) { on_close(conn_hdl); });
		m_webSocketTlsServer->set_message_handler([this](auto && conn_hdl, auto && msg_ptr) { on_message(conn_hdl, msg_ptr); });
		m_webSocketTlsServer->set_tls_init_handler([this](auto && conn_hdl) { return on_tls_init(conn_hdl); });

		m_webSocketTlsServer->listen(serverPort);
		m_webSocketTlsServer->start_accept();
	}else{
		// Set logging settings
#ifdef QT_NO_DEBUG
		m_webSocketServer->clear_access_channels(websocketpp::log::alevel::all);
#else
		m_webSocketServer->set_access_channels(websocketpp::log::alevel::all);
#endif

		m_webSocketServer->init_asio(m_ioService.get());

		m_webSocketServer->set_validate_handler([this](auto && conn_hdl) { return validate(conn_hdl); });
		m_webSocketServer->set_open_handler([this](auto && conn_hdl) { on_open(conn_hdl); });
		m_webSocketServer->set_close_handler([this](auto && conn_hdl) { on_close(conn_hdl); });
		m_webSocketServer->set_message_handler([this](auto && conn_hdl, auto && msg_ptr) { on_message(conn_hdl, msg_ptr); });

		m_webSocketServer->listen(serverPort);
		m_webSocketServer->start_accept();
	}

}

void
ServerAcceptWebHelper::Close()
{
	try {
		if (m_tls) {
			if (m_webSocketTlsServer)
				m_webSocketTlsServer->stop_listening();
		} else {
			if (m_webSocketServer)
				m_webSocketServer->stop_listening();
		}
	} catch (const std::exception &e) {
		LOG_ERROR("WebSocket stop_listening error: " << e.what());
	}
}

bool
ServerAcceptWebHelper::validate(websocketpp::connection_hdl hdl)
{
	bool retVal = false;
	if (m_tls) {
		tls_server::connection_ptr con = m_webSocketTlsServer->get_con_from_hdl(hdl);
		if ((m_webSocketResource.empty() || con->get_resource() == m_webSocketResource)
				&& (m_webSocketOrigin.empty() ||
					(con->get_origin() != "null" &&
					 (con->get_origin() == "http://" + m_webSocketOrigin
					  || con->get_origin() == "http://www." + m_webSocketOrigin
					  || con->get_origin() == "https://" + m_webSocketOrigin
					  || con->get_origin() == "https://www." + m_webSocketOrigin)))) {
			retVal = true;
		}
	} else {
		server::connection_ptr con = m_webSocketServer->get_con_from_hdl(hdl);
		if ((m_webSocketResource.empty() || con->get_resource() == m_webSocketResource)
				&& (m_webSocketOrigin.empty() ||
					(con->get_origin() != "null" &&
					 (con->get_origin() == "http://" + m_webSocketOrigin
					  || con->get_origin() == "http://www." + m_webSocketOrigin
					  || con->get_origin() == "https://" + m_webSocketOrigin
					  || con->get_origin() == "https://www." + m_webSocketOrigin)))) {
			retVal = true;
		}
	}
	return retVal;
}

void
ServerAcceptWebHelper::on_open(websocketpp::connection_hdl hdl)
{
	auto webData = boost::make_shared<WebSocketData>();
	webData->webSocketServer = m_webSocketServer;
	webData->webSocketTlsServer = m_webSocketTlsServer;
	webData->webHandle = hdl;
	webData->isTls = m_tls;
	auto sessionData = boost::make_shared<SessionData>(webData, m_lobbyThread->GetNextSessionId(), m_lobbyThread->GetSessionDataCallback(), *m_ioService, 0);
	{
		boost::mutex::scoped_lock lock(m_sessionMapMutex);
		m_sessionMap.insert(make_pair(hdl, sessionData));
	}
	m_lobbyThread->AddConnection(sessionData);
}

void
ServerAcceptWebHelper::on_close(websocketpp::connection_hdl hdl)
{
	boost::shared_ptr<SessionData> tmpSession;
	{
		boost::mutex::scoped_lock lock(m_sessionMapMutex);
		SessionMap::iterator pos = m_sessionMap.find(hdl);
		if (pos != m_sessionMap.end()) {
			tmpSession = pos->second.lock();
			m_sessionMap.erase(pos);
		}
	}
	if (tmpSession) {
		tmpSession->Close();
	}
}

void
ServerAcceptWebHelper::on_message(websocketpp::connection_hdl hdl, server::message_ptr msg)
{
	if (msg->get_opcode() == websocketpp::frame::opcode::BINARY) {
		boost::shared_ptr<SessionData> tmpSession;
		{
			boost::mutex::scoped_lock lock(m_sessionMapMutex);
			SessionMap::iterator pos = m_sessionMap.find(hdl);
			if (pos != m_sessionMap.end()) {
				tmpSession = pos->second.lock();
			}
		}
		if (tmpSession) {
			tmpSession->GetReceiveBuffer().HandleMessage(tmpSession, msg->get_payload());
		}
	}
}

context_ptr ServerAcceptWebHelper::on_tls_init(websocketpp::connection_hdl hdl) {
    context_ptr ctx(boost::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12));

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                         boost::asio::ssl::context::no_sslv2 |
                         boost::asio::ssl::context::no_sslv3 |
                         boost::asio::ssl::context::single_dh_use);

        // TLS certificate and key paths can be configured via environment variables
        // POKERTH_TLS_CERT and POKERTH_TLS_KEY, otherwise defaults to tls/server.crt and tls/server.key
        const char* certPath = std::getenv("POKERTH_TLS_CERT");
        const char* keyPath = std::getenv("POKERTH_TLS_KEY");
        
        std::string certFile = certPath ? certPath : "tls/server.crt";
        std::string keyFile = keyPath ? keyPath : "tls/server.key";
        
        ctx->use_certificate_chain_file(certFile);
        ctx->use_private_key_file(keyFile, boost::asio::ssl::context::pem);
        std::string ciphers;
        ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:AES:CAMELLIA:DES-CBC3-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!aECDH:!EDH-DSS-DES-CBC3-SHA:!EDH-RSA-DES-CBC3-SHA:!KRB5-DES-CBC3-SHA";
        if (SSL_CTX_set_cipher_list(ctx->native_handle() , ciphers.c_str()) != 1) {
            LOG_ERROR("Error setting cipher list");
        }
    } catch (std::exception& e) {
        LOG_ERROR("TLS initialization error: " << e.what());
        throw;
    }
    return ctx;
}
