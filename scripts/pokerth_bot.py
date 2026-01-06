#!/usr/bin/env python3
"""
PokerTH Network Test Bot

Automated bot that connects to PokerTH server, logs in, creates/joins games,
and plays automatically with configurable strategy.

Usage:
    python3 pokerth_bot.py --server 127.0.0.1 --port 7234 --username bot1 --password demo1

Features:
- Connect to server via TCP
- Login with username/password
- Create game or join existing game
- Automated decision making (fold/call/check/raise/bet)
- Game state monitoring (pot, bets, cards, positions)
- Detailed logging
"""

import socket
import struct
import threading
import time
import argparse
import logging
import sys
from datetime import datetime
from collections import namedtuple
from typing import Optional, List, Dict

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('bot_log.txt'),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)

# Message types (from pokerth protobuf)
MESSAGE_TYPES = {
    0: 'Unknown',
    1: 'Announce',
    2: 'AuthChallenge',
    3: 'AuthRequest',
    4: 'AuthReply',
    5: 'Session',
    6: 'GameList',
    7: 'GetGameList',
    8: 'GameInfo',
    9: 'CreateGame',
    10: 'JoinGame',
    11: 'LeaveGame',
    12: 'KickPlayer',
    13: 'PlayerAction',
    14: 'MyActionRequest',
    15: 'StartEvent',
    16: 'GameStart',
    17: 'GameEnd',
    18: 'HandEnd',
    19: 'ShowCards',
    20: 'BoardCards',
    21: 'PlayerCards',
    22: 'PlayerMoney',
    23: 'GamePot',
    24: 'GameBet',
    25: 'GamePlayerBet',
    26: 'PlayerJoined',
    27: 'PlayerLeft',
    28: 'ChatRequest',
    29: 'ChatMessage',
    30: 'LobbyChatRequest',
    31: 'LobbyChatMessage',
    32: 'VoteKick',
    33: 'Error',
    34: 'GetGameInfo',
    35: 'RejoinGame',
    36: 'InvitePlayer',
    37: 'RejectInvitation',
    38: 'ReportAvatar',
    39: 'ReportGameName',
    40: 'AdminRemoveGame',
    41: 'AdminBanPlayer',
    42: 'AdminKickPlayer',
    43: 'AdminShutdown',
    44: 'AdminGameMessage',
    45: 'AdminPlayerMessage',
    46: 'ServerAuth',
    47: 'ServerMessage',
}

# Player actions
PLAYER_ACTIONS = {
    0: 'None',
    1: 'Fold',
    2: 'Check',
    3: 'Call',
    4: 'Bet',
    5: 'Raise',
    6: 'AllIn',
}

# Game states
GAME_STATES = {
    0: 'Preflop',
    1: 'Flop',
    2: 'Turn',
    3: 'River',
    4: 'PostRiver',
}


class GameState:
    """Track the current game state"""
    def __init__(self):
        self.game_id = 0
        self.game_name = ""
        self.players: Dict[int, dict] = {}  # player_id -> player info
        self.pot = 0
        self.small_blind = 10
        self.big_blind = 20
        self.current_round = 0  # 0=preflop, 1=flop, 2=turn, 3=river
        self.dealer_position = 0
        self.current_player_position = 0
        self.board_cards = []
        self.my_position = 0
        self.my_cash = 0
        self.my_bet = 0
        
    def __str__(self):
        return (f"Game({self.game_id}: {self.game_name}) "
                f"Pot: ${self.pot} "
                f"Round: {GAME_STATES.get(self.current_round, 'Unknown')} "
                f"Cash: ${self.my_cash} "
                f"Bet: ${self.my_bet} "
                f"Players: {len(self.players)}")


class PokerBot:
    """PokerTH Network Bot"""
    
    def __init__(self, server, port, username, password, auto_play=True, verbose=True):
        self.server = server
        self.port = port
        self.username = username
        self.password = password
        self.auto_play = auto_play
        self.verbose = verbose
        
        self.socket = None
        self.connected = False
        self.running = False
        self.receive_thread = None
        
        self.game_state = GameState()
        self.player_id = 0
        self.game_id = 0
        
        # Response tracking
        self.pending_requests = {}
        self.request_id = 0
        
        # Strategy settings
        self.strategy = {
            'always_call': True,  # Simple strategy: always call
            'min_raise': 0,
            'max_raise': 0,
        }
    
    def log(self, message, level=logging.INFO):
        """Log a message"""
        logger.log(level, f"[{self.username}] {message}")
    
    def connect(self):
        """Connect to the PokerTH server"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10.0)
            self.socket.connect((self.server, self.port))
            self.connected = True
            self.running = True
            self.log(f"Connected to {self.server}:{self.port}")
            
            # Start receive thread
            self.receive_thread = threading.Thread(target=self._receive_loop)
            self.receive_thread.daemon = True
            self.receive_thread.start()
            
            return True
            
        except Exception as e:
            self.log(f"Connection failed: {e}", logging.ERROR)
            return False
    
    def disconnect(self):
        """Disconnect from the server"""
        self.running = False
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        self.connected = False
        self.log("Disconnected")
    
    def _receive_loop(self):
        """Receive messages from server"""
        buffer = b""
        
        while self.running:
            try:
                data = self.socket.recv(4096)
                if not data:
                    self.log("Server closed connection")
                    break
                
                buffer += data
                
                # Process complete messages (格式: 4字节长度 + 数据)
                while len(buffer) >= 4:
                    msg_len = struct.unpack('!I', buffer[:4])[0]
                    if len(buffer) < 4 + msg_len:
                        break
                    
                    msg_data = buffer[4:4+msg_len]
                    buffer = buffer[4+msg_len:]
                    
                    self._handle_message(msg_data)
                    
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    self.log(f"Receive error: {e}", logging.ERROR)
                break
        
        self.connected = False
    
    def _handle_message(self, data):
        """Handle a received message"""
        try:
            # 简单的消息解析 - 实际应该用 protobuf
            # 这里我们先打印消息类型
            
            if len(data) >= 4:
                msg_type = struct.unpack('!I', data[:4])[0]
                type_name = MESSAGE_TYPES.get(msg_type, f"Unknown({msg_type})")
                
                if self.verbose:
                    self.log(f"Received: {type_name} ({len(data)} bytes)")
                
                # 根据消息类型处理
                self._process_message(msg_type, data)
                
        except Exception as e:
            self.log(f"Error handling message: {e}", logging.ERROR)
    
    def _process_message(self, msg_type, data):
        """Process a message based on type"""
        if msg_type == 2:  # AuthChallenge
            self._send_auth()
            
        elif msg_type == 4:  # AuthReply
            self._handle_auth_reply(data)
            
        elif msg_type == 5:  # Session - logged in
            self.log("Session established - logged in!")
            self._request_game_list()
            
        elif msg_type == 6:  # GameList
            self._handle_game_list(data)
            
        elif msg_type == 7:  # GetGameList
            pass
            
        elif msg_type == 8:  # GameInfo
            self._handle_game_info(data)
            
        elif msg_type == 13:  # PlayerAction
            self._handle_player_action(data)
            
        elif msg_type == 14:  # MyActionRequest
            self._handle_action_request(data)
            
        elif msg_type == 16:  # GameStart
            self.log("Game started!")
            self.game_state.current_round = 0
            
        elif msg_type == 18:  # HandEnd
            self.log("Hand ended")
            
        elif msg_type == 19:  # ShowCards
            self._handle_show_cards(data)
            
        elif msg_type == 26:  # PlayerJoined
            self._handle_player_joined(data)
            
        elif msg_type == 27:  # PlayerLeft
            self._handle_player_left(data)
            
        elif msg_type == 33:  # Error
            self._handle_error(data)
    
    def _send_message(self, msg_type, data):
        """Send a message to the server"""
        if not self.connected:
            self.log("Not connected!", logging.ERROR)
            return False
        
        try:
            # 格式: 4字节长度 + 4字节类型 + 数据
            header = struct.pack('!II', len(data) + 4, msg_type)
            full_data = header + data
            
            self.socket.sendall(full_data)
            return True
            
        except Exception as e:
            self.log(f"Send failed: {e}", logging.ERROR)
            return False
    
    def _send_auth(self):
        """Send authentication response"""
        self.log(f"Sending auth for {self.username}")
        
        # 构建认证消息 (简化版 - 实际应该用 protobuf)
        # 格式: username (字符串) + password (字符串)
        username_bytes = self.username.encode('utf-8')
        password_bytes = self.password.encode('utf-8')
        
        data = struct.pack('!I', 1) + struct.pack('!I', len(username_bytes)) + username_bytes + \
               struct.pack('!I', len(password_bytes)) + password_bytes
        
        self._send_message(3, data)  # AuthRequest
    
    def _handle_auth_reply(self, data):
        """Handle authentication reply"""
        try:
            result = struct.unpack('!I', data[4:8])[0]
            message_len = struct.unpack('!I', data[8:12])[0]
            message = data[12:12+message_len].decode('utf-8') if message_len > 0 else ""
            
            if result == 0:
                self.log("Authentication successful!")
            else:
                self.log(f"Authentication failed: {message}", logging.ERROR)
                
        except Exception as e:
            self.log(f"Error parsing auth reply: {e}", logging.ERROR)
    
    def _request_game_list(self):
        """Request list of available games"""
        self.log("Requesting game list...")
        self._send_message(7, b'')  # GetGameList
    
    def _handle_game_list(self, data):
        """Handle game list response"""
        self.log("Received game list")
        
        try:
            # 解析游戏列表
            offset = 4  # 跳过消息类型
            num_games = struct.unpack('!I', data[offset:offset+4])[0]
            offset += 4
            
            games = []
            for i in range(num_games):
                game_id = struct.unpack('!I', data[offset:offset+4])[0]
                offset += 4
                
                name_len = struct.unpack('!I', data[offset:offset+4])[0]
                offset += 4
                name = data[offset:offset+name_len].decode('utf-8') if name_len > 0 else ""
                offset += name_len
                
                num_players = struct.unpack('!I', data[offset:offset+4])[0]
                offset += 4
                
                max_players = struct.unpack('!I', data[offset:offset+4])[0]
                offset += 4
                
                games.append({'id': game_id, 'name': name, 'players': num_players, 'max': max_players})
                offset += 4  # 跳过状态
                offset += 4  # 跳过类型
                offset += 8  # 跳过金钱信息
            
            if games:
                self.log(f"Found {len(games)} games: {[g['name'] for g in games]}")
                # 加入第一个游戏
                self._join_game(games[0]['id'])
            else:
                self.log("No games found - creating one")
                self._create_game()
                
        except Exception as e:
            self.log(f"Error parsing game list: {e}", logging.ERROR)
            self._create_game()
    
    def _create_game(self):
        """Create a new game"""
        self.log(f"Creating game: {self.username}'s Game")
        
        game_name = self.username + "'s Game"
        game_name_bytes = game_name.encode('utf-8')
        
        # GameData: maxPlayers(4) + startMoney(4) + smallBlind(4) + gameType(4)
        game_data = struct.pack('!I', 2) + struct.pack('!I', 1000) + \
                    struct.pack('!I', 10) + struct.pack('!I', 1)  # GAME_TYPE_NORMAL
        
        data = struct.pack('!I', len(game_name_bytes)) + game_name_bytes + \
               struct.pack('!I', 0) + struct.pack('!I', 0) +  # password, autoleave
               game_data
        
        self._send_message(9, data)  # CreateGame
    
    def _join_game(self, game_id):
        """Join an existing game"""
        self.log(f"Joining game {game_id}")
        self.game_id = game_id
        
        data = struct.pack('!I', game_id) + struct.pack('!I', 0) + struct.pack('!I', 0)
        self._send_message(10, data)  # JoinGame
    
    def _handle_game_info(self, data):
        """Handle game info"""
        self.log("Received game info")
    
    def _handle_player_joined(self, data):
        """Handle player joined message"""
        try:
            offset = 4
            player_id = struct.unpack('!I', data[offset:offset+4])[0]
            offset += 4
            
            name_len = struct.unpack('!I', data[offset:offset+4])[0]
            offset += 4
            name = data[offset:offset+name_len].decode('utf-8') if name_len > 0 else ""
            offset += name_len
            
            self.log(f"Player joined: {name} (ID: {player_id})")
            
            # 如果是另一个玩家加入，且游戏人数够了，就开始游戏
            if name != self.username and player_id != self.player_id:
                self.log("Opponent joined - starting game")
                self._start_game()
                
        except Exception as e:
            self.log(f"Error handling player joined: {e}", logging.ERROR)
    
    def _handle_player_left(self, data):
        """Handle player left message"""
        try:
            offset = 4
            player_id = struct.unpack('!I', data[offset:offset+4])[0]
            self.log(f"Player {player_id} left")
            
        except Exception as e:
            self.log(f"Error handling player left: {e}", logging.ERROR)
    
    def _start_game(self):
        """Start the game"""
        self.log("Starting game...")
        
        # 检查是否我是游戏创建者
        # 如果是，发送 StartEvent
        self._send_message(15, b'')  # StartEvent
    
    def _handle_player_action(self, data):
        """Handle player action message"""
        try:
            offset = 4
            player_id = struct.unpack('!I', data[offset:offset+4])[0]
            offset += 4
            
            action = struct.unpack('!I', data[offset:offset+4])[0]
            offset += 4
            
            relative_bet = struct.unpack('!I', data[offset:offset+4])[0]
            
            action_name = PLAYER_ACTIONS.get(action, f"Unknown({action})")
            self.log(f"Player {player_id}: {action_name} (${relative_bet})")
            
            # 更新游戏状态
            if player_id in self.game_state.players:
                self.game_state.players[player_id]['last_action'] = action
                
        except Exception as e:
            self.log(f"Error handling player action: {e}", logging.ERROR)
    
    def _handle_action_request(self, data):
        """Handle action request - it's our turn to act"""
        try:
            offset = 4
            
            self.game_id = struct.unpack('!I', data[offset:offset+4])[0]
            offset += 4
            
            hand_num = struct.unpack('!I', data[offset:offset+4])[0]
            offset += 4
            
            game_state = struct.unpack('!I', data[offset:offset+4])[0]
            offset += 4
            
            self.game_state.current_round = game_state
            
            state_name = GAME_STATES.get(game_state, f"Unknown({game_state})")
            self.log(f"Action request! Hand {hand_num}, {state_name}")
            
            if self.auto_play:
                self._make_decision(game_state)
                
        except Exception as e:
            self.log(f"Error handling action request: {e}", logging.ERROR)
    
    def _make_decision(self, game_state):
        """Make a playing decision"""
        action = 3  # Call
        
        self.log(f"Taking action: {PLAYER_ACTIONS[action]}")
        self._take_action(action, 0)
    
    def _take_action(self, action, amount):
        """Take an action"""
        # 构建 MyActionRequest 消息
        data = struct.pack('!I', self.game_id) + \
               struct.pack('!I', 1) +  # hand num (simplified)
               struct.pack('!I', self.game_state.current_round) + \
               struct.pack('!I', action) + \
               struct.pack('!I', amount)
        
        self._send_message(14, data)  # MyActionRequest
    
    def _handle_show_cards(self, data):
        """Handle show cards message"""
        try:
            offset = 4
            player_id = struct.unpack('!I', data[offset:offset+4])[0]
            offset += 4
            
            cards_len = struct.unpack('!I', data[offset:offset+4])[0]
            offset += 4
            cards = data[offset:offset+cards_len].decode('utf-8') if cards_len > 0 else ""
            
            self.log(f"Player {player_id} shows: {cards}")
            
        except Exception as e:
            self.log(f"Error handling show cards: {e}", logging.ERROR)
    
    def _handle_error(self, data):
        """Handle error message"""
        try:
            offset = 4
            error_code = struct.unpack('!I', data[offset:offset+4])[0]
            offset += 4
            
            msg_len = struct.unpack('!I', data[offset:offset+4])[0]
            offset += 4
            message = data[offset:offset+msg_len].decode('utf-8') if msg_len > 0 else ""
            
            self.log(f"Server error ({error_code}): {message}", logging.ERROR)
            
        except Exception as e:
            self.log(f"Error handling error: {e}", logging.ERROR)
    
    def run(self):
        """Main run loop"""
        if not self.connect():
            return False
        
        # Wait for connection
        time.sleep(2)
        
        if not self.connected:
            self.log("Failed to connect", logging.ERROR)
            return False
        
        self.log("Bot running. Press Ctrl+C to stop.")
        
        try:
            while self.connected:
                time.sleep(1)
        except KeyboardInterrupt:
            self.log("Interrupted by user")
        
        self.disconnect()
        return True


def main():
    parser = argparse.ArgumentParser(description='PokerTH Network Bot')
    parser.add_argument('--server', default='127.0.0.1', help='Server address')
    parser.add_argument('--port', type=int, default=7234, help='Server port')
    parser.add_argument('--username', default='bot1', help='Username')
    parser.add_argument('--password', default='demo1', help='Password')
    parser.add_argument('--no-auto', action='store_true', help='Disable auto-play')
    parser.add_argument('--quiet', action='store_true', help='Minimize output')
    parser.add_argument('--debug', action='store_true', help='Enable debug logging')
    
    args = parser.parse_args()
    
    # Setup logging
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)
    elif args.quiet:
        logging.getLogger().setLevel(logging.WARNING)
    
    # Create and run bot
    bot = PokerBot(
        server=args.server,
        port=args.port,
        username=args.username,
        password=args.password,
        auto_play=not args.no_auto,
        verbose=not args.quiet
    )
    
    success = bot.run()
    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())
