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
/* State of network client. */

#ifndef _CLIENTSTATE_H_
#define _CLIENTSTATE_H_

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <string>

#define CLIENT_INITIAL_STATE ClientStateInit
#define CLIENT_FINAL_STATE ClientStateFinal

class ClientThread;
class ClientContext;
class ClientCallback;
class Game;
class NetPacket;
class DownloadHelper;

class ClientState
{
public:
	virtual ~ClientState() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client) = 0;
	virtual void Exit(boost::shared_ptr<ClientThread> client) = 0;

	virtual void HandlePacket(boost::shared_ptr<ClientThread> client, boost::shared_ptr<NetPacket> tmpPacket) = 0;
};

// State: Initialization.
class ClientStateInit : public ClientState
{
public:
	static ClientStateInit &Instance();
	virtual ~ClientStateInit() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

	virtual void HandlePacket(boost::shared_ptr<ClientThread> /*client*/, boost::shared_ptr<NetPacket> /*tmpPacket*/) {}

protected:
	ClientStateInit();
};

class ClientStateStartResolve : public ClientState
{
public:
	static ClientStateStartResolve &Instance();
	virtual ~ClientStateStartResolve() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

	virtual void HandlePacket(boost::shared_ptr<ClientThread> /*client*/, boost::shared_ptr<NetPacket> /*tmpPacket*/) {}

protected:

	ClientStateStartResolve();

	void HandleResolve(
		const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type endpoint_iterator,
		boost::shared_ptr<ClientThread> client);
};

class ClientStateStartServerListDownload : public ClientState
{
public:
	static ClientStateStartServerListDownload &Instance();
	virtual ~ClientStateStartServerListDownload() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

	virtual void HandlePacket(boost::shared_ptr<ClientThread> /*client*/, boost::shared_ptr<NetPacket> /*tmpPacket*/) {}

protected:

	ClientStateStartServerListDownload();
};

class ClientStateDownloadingServerList : public ClientState
{
public:
	static ClientStateDownloadingServerList &Instance();
	virtual ~ClientStateDownloadingServerList() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

	virtual void HandlePacket(boost::shared_ptr<ClientThread> /*client*/, boost::shared_ptr<NetPacket> /*tmpPacket*/) {}

	void SetDownloadHelper(boost::shared_ptr<DownloadHelper> helper);

protected:

	ClientStateDownloadingServerList();

	void TimerLoop(const boost::system::error_code& ec, boost::shared_ptr<ClientThread> client);

private:

	boost::shared_ptr<DownloadHelper> myDownloadHelper;
};

class ClientStateReadingServerList : public ClientState
{
public:
	static ClientStateReadingServerList &Instance();
	virtual ~ClientStateReadingServerList() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

	virtual void HandlePacket(boost::shared_ptr<ClientThread> /*client*/, boost::shared_ptr<NetPacket> /*tmpPacket*/) {}

protected:

	ClientStateReadingServerList();
};

class ClientStateWaitChooseServer : public ClientState
{
public:
	static ClientStateWaitChooseServer &Instance();
	virtual ~ClientStateWaitChooseServer() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

	virtual void HandlePacket(boost::shared_ptr<ClientThread> /*client*/, boost::shared_ptr<NetPacket> /*tmpPacket*/) {}

protected:

	ClientStateWaitChooseServer();

	void TimerLoop(const boost::system::error_code& ec, boost::shared_ptr<ClientThread> client);
};

class ClientStateStartConnect : public ClientState
{
public:
	static ClientStateStartConnect &Instance();
	virtual ~ClientStateStartConnect() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

	virtual void HandlePacket(boost::shared_ptr<ClientThread> /*client*/, boost::shared_ptr<NetPacket> /*tmpPacket*/) {}

	void SetRemoteEndpoint(boost::asio::ip::tcp::resolver::results_type endpointIterator);

protected:

	ClientStateStartConnect();

	void HandleConnect(const boost::system::error_code& ec,
					   boost::asio::ip::basic_resolver_iterator<boost::asio::ip::tcp> endpoint_iterator,
					   boost::shared_ptr<ClientThread> client);

	void TimerTimeout(const boost::system::error_code& ec, boost::shared_ptr<ClientThread> client);

    void HandleSslHandshake(const boost::system::error_code& ec, boost::shared_ptr<ClientThread> client);

private:
	boost::asio::ip::tcp::resolver::results_type myRemoteEndpoint;
	boost::asio::ip::basic_resolver_iterator<boost::asio::ip::tcp> myRemoteEndpointIterator;
};

class AbstractClientStateReceiving : public ClientState
{
public:
	virtual ~AbstractClientStateReceiving() noexcept;

	virtual void HandlePacket(boost::shared_ptr<ClientThread> client, boost::shared_ptr<NetPacket> tmpPacket);

protected:
	AbstractClientStateReceiving();

	virtual void InternalHandlePacket(boost::shared_ptr<ClientThread> client, boost::shared_ptr<NetPacket> tmpPacket) = 0;
};

class ClientStateStartSession : public AbstractClientStateReceiving
{
public:
	static ClientStateStartSession &Instance();
	virtual ~ClientStateStartSession() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

protected:

	ClientStateStartSession();

	virtual void InternalHandlePacket(boost::shared_ptr<ClientThread> client, boost::shared_ptr<NetPacket> tmpPacket);
};

class ClientStateWaitEnterLogin : public ClientState
{
public:
	static ClientStateWaitEnterLogin &Instance();
	virtual ~ClientStateWaitEnterLogin() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

	virtual void HandlePacket(boost::shared_ptr<ClientThread> client, boost::shared_ptr<NetPacket> tmpPacket);

protected:

	ClientStateWaitEnterLogin();

	void TimerLoop(const boost::system::error_code& ec, boost::shared_ptr<ClientThread> client);
};

class ClientStateWaitAuthChallenge : public AbstractClientStateReceiving
{
public:
	static ClientStateWaitAuthChallenge &Instance();
	virtual ~ClientStateWaitAuthChallenge() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

protected:

	ClientStateWaitAuthChallenge();

	virtual void InternalHandlePacket(boost::shared_ptr<ClientThread> client, boost::shared_ptr<NetPacket> tmpPacket);
};

class ClientStateWaitAuthVerify : public AbstractClientStateReceiving
{
public:
	static ClientStateWaitAuthVerify &Instance();
	virtual ~ClientStateWaitAuthVerify() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

protected:

	ClientStateWaitAuthVerify();

	virtual void InternalHandlePacket(boost::shared_ptr<ClientThread> client, boost::shared_ptr<NetPacket> tmpPacket);
};

class ClientStateWaitSession : public AbstractClientStateReceiving
{
public:
	static ClientStateWaitSession &Instance();
	virtual ~ClientStateWaitSession() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

protected:

	ClientStateWaitSession();

	virtual void InternalHandlePacket(boost::shared_ptr<ClientThread> client, boost::shared_ptr<NetPacket> tmpPacket);
};

class ClientStateWaitJoin : public AbstractClientStateReceiving
{
public:
	static ClientStateWaitJoin &Instance();
	virtual ~ClientStateWaitJoin() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

protected:

	ClientStateWaitJoin();

	virtual void InternalHandlePacket(boost::shared_ptr<ClientThread> client, boost::shared_ptr<NetPacket> tmpPacket);
};

class ClientStateWaitGame : public AbstractClientStateReceiving
{
public:
	static ClientStateWaitGame &Instance();
	virtual ~ClientStateWaitGame() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

protected:

	ClientStateWaitGame();

	virtual void InternalHandlePacket(boost::shared_ptr<ClientThread> client, boost::shared_ptr<NetPacket> tmpPacket);
};

class ClientStateSynchronizeStart : public AbstractClientStateReceiving
{
public:
	static ClientStateSynchronizeStart &Instance();
	virtual ~ClientStateSynchronizeStart() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

protected:

	ClientStateSynchronizeStart();

	void TimerLoop(const boost::system::error_code& ec, boost::shared_ptr<ClientThread> client);
	virtual void InternalHandlePacket(boost::shared_ptr<ClientThread> client, boost::shared_ptr<NetPacket> tmpPacket);
};

class ClientStateWaitStart : public AbstractClientStateReceiving
{
public:
	static ClientStateWaitStart &Instance();
	virtual ~ClientStateWaitStart() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

protected:

	ClientStateWaitStart();

	virtual void InternalHandlePacket(boost::shared_ptr<ClientThread> client, boost::shared_ptr<NetPacket> tmpPacket);
};

class ClientStateWaitHand : public AbstractClientStateReceiving
{
public:
	static ClientStateWaitHand &Instance();
	virtual ~ClientStateWaitHand() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

protected:

	ClientStateWaitHand();

	virtual void InternalHandlePacket(boost::shared_ptr<ClientThread> client, boost::shared_ptr<NetPacket> tmpPacket);
};

class ClientStateRunHand : public AbstractClientStateReceiving
{
public:
	static ClientStateRunHand &Instance();
	virtual ~ClientStateRunHand() noexcept;

	virtual void Enter(boost::shared_ptr<ClientThread> client);
	virtual void Exit(boost::shared_ptr<ClientThread> client);

protected:

	ClientStateRunHand();

	virtual void InternalHandlePacket(boost::shared_ptr<ClientThread> client, boost::shared_ptr<NetPacket> tmpPacket);

	static void ResetPlayerActions(Game &curGame);
	static void ResetPlayerSets(Game &curGame);
};

class ClientStateFinal : public ClientState
{
public:
	static ClientStateFinal &Instance();
	virtual ~ClientStateFinal() noexcept {}

	virtual void Enter(boost::shared_ptr<ClientThread> /*client*/) {}
	virtual void Exit(boost::shared_ptr<ClientThread> /*client*/) {}

	virtual void HandlePacket(boost::shared_ptr<ClientThread> /*client*/, boost::shared_ptr<NetPacket> /*tmpPacket*/) {}

protected:
	ClientStateFinal() {}
};

#endif
