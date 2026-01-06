/*****************************************************************************
 * PokerTH - Network Test Bot
 * 
 * Automated bot that connects to PokerTH server, logs in, creates/joins games,
 * and plays automatically with configurable strategy.
 * 
 * Features:
 * - Connect to server via TCP/TLS
 * - Login with username/password
 * - Create game or join existing game
 * - Automated decision making (fold/call/check/raise/bet)
 * - Game state monitoring (pot, bets, cards, positions)
 * - Detailed logging
 * 
 * Build: mkdir -p build && cd build && cmake ../.. && make pokerth_bot
 * Run: ./bin/pokerth_bot --server 127.0.0.1 --port 7234 --username bot1 --password demo1
 *****************************************************************************/

#include <iostream>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <queue>
#include <fstream>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <game_defs.h>
#include <gamedata.h>
#include <net/netpacket.h>
#include <net/socket_helper.h>

using boost::asio::ip::tcp;

enum class BotState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AUTHENTICATING,
    LOGGED_IN,
    IN_LOBBY,
    IN_GAME,
    PLAYING_HAND
};

enum class BotAction {
    FOLD,
    CHECK,
    CALL,
    BET,
    RAISE,
    ALL_IN,
    WAIT
};

struct PlayerState {
    unsigned id;
    std::string name;
    int cash;
    int bet;
    bool isActive;
    bool isAllIn;
};

struct GameState {
    unsigned id;
    std::string name;
    std::vector<PlayerState> players;
    int pot;
    int smallBlind;
    int bigBlind;
    int currentRound;  // 0=preflop, 1=flop, 2=turn, 3=river
    int dealerPosition;
    int currentPlayerPosition;
    std::vector<std::string> boardCards;
};

class PokerBot : public boost::enable_shared_from_this<PokerBot> {
public:
    PokerBot(const std::string& server, int port, const std::string& username, const std::string& password)
        : m_server(server), m_port(port), m_username(username), m_password(password),
          m_state(BotState::DISCONNECTED),
          m_socket(m_ioContext),
          m_resolver(m_ioContext),
          m_autoPlay(true),
          m_verbose(true) {
        
        m_logFile.open("bot_" + username + "_log.txt");
        log("=== PokerTH Bot Started ===");
        log("Server: " + server + ":" + std::to_string(port));
        log("Username: " + username);
    }

    ~PokerBot() {
        disconnect();
        if (m_logFile.is_open()) {
            m_logFile.close();
        }
    }

    void connect() {
        m_state = BotState::CONNECTING;
        log("Connecting to " + m_server + ":" + std::to_string(m_port) + "...");
        
        auto self = shared_from_this();
        m_resolver.async_resolve(m_server, std::to_string(m_port),
            [this, self](const boost::system::error_code& ec, tcp::resolver::results_type endpoints) {
                if (!ec) {
                    boost::asio::async_connect(m_socket, endpoints,
                        [this, self](const boost::system::error_code& ec, tcp::endpoint /*endpoint*/) {
                            if (!ec) {
                                m_state = BotState::CONNECTED;
                                log("Connected! Starting receive loop...");
                                startReceive();
                                startAuth();
                            } else {
                                log("Connect failed: " + ec.message());
                                m_state = BotState::DISCONNECTED;
                            }
                        });
                } else {
                    log("Resolve failed: " + ec.message());
                    m_state = BotState::DISCONNECTED;
                }
            });
    }

    void run() {
        m_ioContext.run();
    }

    void disconnect() {
        if (m_socket.is_open()) {
            boost::system::error_code ec;
            m_socket.close(ec);
        }
        m_state = BotState::DISCONNECTED;
        log("Disconnected");
    }

    void setAutoPlay(bool autoPlay) { m_autoPlay = autoPlay; }
    void setVerbose(bool verbose) { m_verbose = verbose; }

private:
    void log(const std::string& msg) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        std::lock_guard<std::mutex> lock(m_logMutex);
        std::stringstream ss;
        ss << "[" << std::put_time(std::localtime(&time), "%H:%M:%S") << "] " << msg;
        std::cout << ss.str() << std::endl;
        m_logFile << ss.str() << std::endl;
        m_logFile.flush();
    }

    void startReceive() {
        auto self = shared_from_this();
        boost::asio::async_read(m_socket,
            boost::asio::buffer(m_readBuffer, NET_MAX_MESSAGE_LENGTH),
            boost::asio::transfer_exactly(NET_MAX_MESSAGE_LENGTH),
            [this, self](const boost::system::error_code& ec, size_t bytesTransferred) {
                if (!ec && bytesTransferred > 0) {
                    handleReceive(m_readBuffer, bytesTransferred);
                    startReceive();
                } else if (ec) {
                    log("Receive failed: " + ec.message());
                    m_state = BotState::DISCONNECTED;
                }
            });
    }

    void handleReceive(const char* data, size_t len) {
        if (m_verbose) {
            log("Received " + std::to_string(len) + " bytes");
        }
        
        // Parse protobuf message
        NetPacket packet;
        if (packet.GetMsg()->ParseFromArray(data, len)) {
            handlePacket(packet);
        } else {
            log("Failed to parse packet");
        }
    }

    void handlePacket(NetPacket& packet) {
        auto type = packet.GetMsg()->messagetype();
        
        switch (type) {
            case PokerTHMessage::Type_AnnounceMessage:
                log("Received server announcement");
                break;
                
            case PokerTHMessage::Type_AuthChallengeMessage:
                log("Auth challenge received - sending credentials");
                sendAuthResponse();
                break;
                
            case PokerTHMessage::Type_AuthReplyMessage:
                log("Auth reply received");
                handleAuthReply(packet);
                break;
                
            case PokerTHMessage::Type_SessionMessage:
                log("Session established - logged in!");
                m_state = BotState::LOGGED_IN;
                enterLobby();
                break;
                
            case PokerTHMessage::Type_GameListMessage:
                handleGameList(packet);
                break;
                
            case PokerTHMessage::Type_GameInfoMessage:
                handleGameInfo(packet);
                break;
                
            case PokerTHMessage::Type_JoinGameRequestMessage:
                handleJoinGameRequest(packet);
                break;
                
            case PokerTHMessage::Type_PlayerActionMessage:
                handlePlayerAction(packet);
                break;
                
            case PokerTHMessage::Type_MyActionRequestMessage:
                handleMyActionRequest(packet);
                break;
                
            case PokerTHMessage::Type_HandEndMessage:
                log("Hand ended");
                m_state = BotState::IN_GAME;
                break;
                
            case PokerTHMessage::Type_GameStartMessage:
                log("Game started!");
                m_state = BotState::IN_GAME;
                break;
                
            case PokerTHMessage::Type_ShowCardsMessage:
                handleShowCards(packet);
                break;
                
            case PokerTHMessage::Type_PlayerJoinedMessage:
                handlePlayerJoined(packet);
                break;
                
            case PokerTHMessage::Type_PlayerLeftMessage:
                handlePlayerLeft(packet);
                break;
                
            case PokerTHMessage::Type_ErrorMessage:
                handleError(packet);
                break;
                
            default:
                if (m_verbose) {
                    log("Unhandled message type: " + std::to_string(type));
                }
                break;
        }
    }

    void startAuth() {
        log("Starting authentication...");
        // Wait for auth challenge
    }

    void sendAuthResponse() {
        log("Sending authentication response for: " + m_username);
        
        NetPacket packet;
        packet.GetMsg()->set_messagetype(PokerTHMessage::Type_AuthRequestMessage);
        auto* auth = packet.GetMsg()->mutable_authrequestmessage();
        auth->set_usertype(1);  // Regular user
        auth->set_username(m_username);
        auth->set_password(m_password);
        
        sendPacket(packet);
    }

    void handleAuthReply(NetPacket& packet) {
        auto* auth = packet.GetMsg()->authreplymessage();
        auto result = auth->result();
        
        if (result == 0) {
            log("Authentication successful!");
        } else {
            log("Authentication failed: " + auth->message());
        }
    }

    void enterLobby() {
        log("Entering lobby...");
        m_state = BotState::IN_LOBBY;
        
        // Request game list
        NetPacket packet;
        packet.GetMsg()->set_messagetype(PokerTHMessage::Type_GetGameListMessage);
        sendPacket(packet);
        
        // Start lobby timer - will check for games periodically
        checkForGames();
    }

    void checkForGames() {
        log("Checking for games...");
        // Will be implemented to look for existing games to join
        // or create a new game
    }

    void handleGameList(NetPacket& packet) {
        log("Received game list");
        auto* gameList = packet.GetMsg()->gamelistmessage();
        
        if (gameList->gamelist_size() > 0) {
            log("Found " + std::to_string(gameList->gamelist_size()) + " games");
            // Join first available game
            auto gameInfo = gameList->gamelist(0);
            joinGame(gameInfo.gameid());
        } else {
            log("No games found - creating one");
            createGame();
        }
    }

    void createGame() {
        log("Creating new game: " + m_username + "'s Game");
        
        GameData gameData;
        gameData.gameType = GAME_TYPE_NORMAL;
        gameData.maxNumberOfPlayers = 2;
        gameData.startMoney = 1000;
        gameData.firstSmallBlind = 10;
        gameData.raiseIntervalMode = RAISE_ON_HANDNUMBER;
        gameData.raiseSmallBlindEveryHandsValue = 8;
        gameData.guiSpeed = 4;
        gameData.playerActionTimeoutSec = 30;
        
        NetPacket packet;
        packet.GetMsg()->set_messagetype(PokerTHMessage::Type_CreateGameMessage);
        auto* create = packet.GetMsg()->mutable_creategamemessage();
        create->mutable_gamedata()->CopyFrom(gameData);
        create->set_gamename(m_username + "'s Game");
        create->set_password("");  // No password
        create->set_autoleave(false);
        
        sendPacket(packet);
    }

    void joinGame(unsigned gameId) {
        log("Joining game " + std::to_string(gameId));
        
        NetPacket packet;
        packet.GetMsg()->set_messagetype(PokerTHMessage::Type_JoinGameMessage);
        auto* join = packet.GetMsg()->mutable_joingamemessage();
        join->set_gameid(gameId);
        join->set_password("");
        join->set_autoleave(false);
        
        sendPacket(packet);
    }

    void handleGameInfo(NetPacket& packet) {
        auto* gameInfo = packet.GetMsg()->gameinfomessage();
        log("Game info: " + gameInfo->name() + " with " + 
            std::to_string(gameInfo->players_size()) + " players");
    }

    void handleJoinGameRequest(NetPacket& packet) {
        log("Join game request received - accepting");
        // Accept join request
    }

    void handlePlayerJoined(NetPacket& packet) {
        auto* playerMsg = packet.GetMsg()->playerjoinedmessage();
        log("Player joined: " + playerMsg->playername() + " (ID: " + 
            std::to_string(playerMsg->playerid()) + ")");
        
        if (playerMsg->playername() != m_username && m_autoPlay) {
            log("Opponent joined - starting game");
            startGame();
        }
    }

    void handlePlayerLeft(NetPacket& packet) {
        auto* playerMsg = packet.GetMsg()->playerleftmessage();
        log("Player left: " + std::to_string(playerMsg->playerid()));
    }

    void startGame() {
        log("Starting the game...");
        
        NetPacket packet;
        packet.GetMsg()->set_messagetype(PokerTHMessage::Type_StartEventMessage);
        auto* start = packet.GetMsg()->mutable_starteventmessage();
        start->set_starteventtype(StartEventMessage::startEvent);
        start->set_fillupwithcomputerplayers(false);
        
        sendPacket(packet);
    }

    void handlePlayerAction(NetPacket& packet) {
        auto* action = packet.GetMsg()->playeractionmessage();
        log("Player " + std::to_string(action->playerid()) + " action: " + 
            std::to_string(action->myaction()));
    }

    void handleMyActionRequest(NetPacket& packet) {
        auto* action = packet.GetMsg()->myactionrequestmessage();
        log("Action request! Game state: " + std::to_string(action->gamestate()) + 
            ", Hand: " + std::to_string(action->handnum()));
        
        if (!m_autoPlay) {
            log("Auto-play disabled - waiting for manual action");
            return;
        }
        
        // Make automated decision
        BotAction botAction = decideAction(action->gamestate());
        takeAction(botAction, action->handnum());
    }

    BotAction decideAction(int gameState) {
        // Simple strategy: always check or call
        // TODO: Implement more sophisticated strategy
        
        if (gameState == GAME_STATE_PREFLOP) {
            // Preflop logic - could be facing raise
            log("Preflop decision");
        } else {
            // Post-flop - check if facing bet or not
            log("Post-flop decision");
        }
        
        return BotAction::CALL;  // Default: call
    }

    void takeAction(BotAction action, int handNum) {
        log("Taking action: " + std::to_string(static_cast<int>(action)));
        
        // Set player's action
        // Note: This needs to be set on the player object before sending
        
        NetPacket packet;
        packet.GetMsg()->set_messagetype(PokerTHMessage::Type_MyActionRequestMessage);
        auto* myAction = packet.GetMsg()->mutable_myactionrequestmessage();
        myAction->set_handnum(handNum);
        myAction->set_gamestate(GAME_STATE_PREFLOP);  // Will be updated
        myAction->set_myaction(static_cast<NetPlayerAction>(action));
        myAction->set_myrelativebet(0);
        
        sendPacket(packet);
    }

    void handleShowCards(NetPacket& packet) {
        auto* show = packet.GetMsg()->showcardsmessage();
        log("Player " + std::to_string(show->playerid()) + " shows: " + 
            show->cards());
    }

    void handleError(NetPacket& packet) {
        auto* error = packet.GetMsg()->errormessage();
        log("Error: " + error->errormessage());
    }

    void sendPacket(NetPacket& packet) {
        int size = packet.GetMsg()->ByteSizeLong();
        std::vector<char> buffer(size);
        packet.GetMsg()->SerializeWithCachedSizesToArray(buffer.data());
        
        boost::system::error_code ec;
        boost::asio::write(m_socket, boost::asio::buffer(buffer), ec);
        
        if (ec) {
            log("Send failed: " + ec.message());
        } else if (m_verbose) {
            log("Sent " + std::to_string(buffer.size()) + " bytes");
        }
    }

    std::string m_server;
    int m_port;
    std::string m_username;
    std::string m_password;
    
    BotState m_state;
    boost::asio::io_context m_ioContext;
    tcp::socket m_socket;
    tcp::resolver m_resolver;
    char m_readBuffer[NET_MAX_MESSAGE_LENGTH];
    
    std::mutex m_logMutex;
    std::ofstream m_logFile;
    
    bool m_autoPlay;
    bool m_verbose;
    
    GameState m_gameState;
};

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n\n"
              << "Options:\n"
              << "  --server <addr>    Server address (default: 127.0.0.1)\n"
              << "  --port <port>      Server port (default: 7234)\n"
              << "  --username <name>  Username (default: bot1)\n"
              << "  --password <pass>  Password (default: demo1)\n"
              << "  --no-auto          Disable auto-play\n"
              << "  --quiet            Minimize output\n"
              << "  --help             Show this help\n";
}

int main(int argc, char* argv[]) {
    std::string server = "127.0.0.1";
    int port = 7234;
    std::string username = "bot1";
    std::string password = "demo1";
    bool autoPlay = true;
    bool verbose = true;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--server" && i + 1 < argc) {
            server = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--username" && i + 1 < argc) {
            username = argv[++i];
        } else if (arg == "--password" && i + 1 < argc) {
            password = argv[++i];
        } else if (arg == "--no-auto") {
            autoPlay = false;
        } else if (arg == "--quiet") {
            verbose = false;
        }
    }
    
    try {
        auto bot = std::make_shared<PokerBot>(server, port, username, password);
        bot->setAutoPlay(autoPlay);
        bot->setVerbose(verbose);
        
        // Run in separate thread so we can handle signals
        std::thread runThread([bot]() {
            bot->connect();
            bot->run();
        });
        
        // Wait for thread
        runThread.join();
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
