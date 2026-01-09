# PokerTH Build and Run Guide (WSL/Ubuntu)

## 1. Install Dependencies

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    qt6-base-dev \
    qt6-base-dev-tools \
    libqt6xml6 \
    libqt6sql6 \
    libboost-all-dev \
    libcurl4-openssl-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libssl-dev \
    zlib1g-dev \
    mariadb-server \
    mariadb-client
```

## 2. Setup Database

```bash
# Start MySQL/MariaDB
sudo service mariadb start

# Secure installation (optional but recommended)
sudo mysql_secure_installation

# Login as root
sudo mysql -u root
```

**In MySQL shell:**
```sql
-- Create database and user
CREATE DATABASE pokerth;
CREATE USER 'pokerth'@'localhost' IDENTIFIED BY '';
GRANT ALL PRIVILEGES ON pokerth.* TO 'pokerth'@'localhost';
FLUSH PRIVILEGES;

-- Use the database
USE pokerth;

-- Create tables
CREATE TABLE player (
    player_id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    password BLOB NOT NULL,
    blocked TINYINT DEFAULT 0,
    country_iso VARCHAR(2),
    last_login DATETIME,
    active TINYINT DEFAULT 1,
    avatar_hash VARCHAR(32),
    avatar_mime VARCHAR(50),
    last_games INT DEFAULT 0,
    last_ip VARCHAR(45)
);

CREATE TABLE game (
    idgame INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    start_time DATETIME,
    end_time DATETIME
);

CREATE TABLE game_has_player (
    game_idgame INT NOT NULL,
    player_idplayer INT NOT NULL,
    place INT,
    PRIMARY KEY (game_idgame, player_idplayer),
    FOREIGN KEY (game_idgame) REFERENCES game(idgame),
    FOREIGN KEY (player_idplayer) REFERENCES player(player_id)
);

CREATE TABLE reported_avatar (
    idplayer INT NOT NULL,
    avatar_hash VARCHAR(32) NOT NULL,
    avatar_type VARCHAR(50),
    by_idplayer INT NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (idplayer) REFERENCES player(player_id),
    FOREIGN KEY (by_idplayer) REFERENCES player(player_id)
);

CREATE TABLE reported_gamename (
    game_creator_idplayer INT NOT NULL,
    game_idgame INT NOT NULL,
    game_name VARCHAR(100),
    by_idplayer INT NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (game_creator_idplayer) REFERENCES player(player_id),
    FOREIGN KEY (game_idgame) REFERENCES game(idgame),
    FOREIGN KEY (by_idplayer) REFERENCES player(player_id)
);

CREATE TABLE avatar_blacklist (
    id INT AUTO_INCREMENT PRIMARY KEY,
    avatar_hash VARCHAR(32) NOT NULL
);

CREATE TABLE admin_player (
    admin_idplayer INT NOT NULL,
    FOREIGN KEY (admin_idplayer) REFERENCES player(player_id)
);

-- Create demo accounts (passwords are stored as AES_ENCRYPT with empty key)
INSERT INTO player (username, password, blocked, active) VALUES 
    ('demo1', AES_ENCRYPT('demo1', ''), 0, 1),
    ('demo2', AES_ENCRYPT('demo2', ''), 0, 1);

-- Verify
SELECT player_id, username, blocked, active FROM player WHERE username IN ('demo1', 'demo2');

EXIT;
```

**Verify database:**
```bash
mysql -u pokerth -p pokerth -e "SELECT player_id, username, blocked, active FROM player WHERE username IN ('demo1', 'demo2');"
# Password: (press enter, empty password)
```

## 3. Clone Repository

```bash
cd ~
git clone https://github.com/ph4n70mr1ddl3r/pokerth.git
cd pokerth
```

## 4. Build with CMake

```bash
cmake -DCMAKE_BUILD_TYPE:STRING=Release -S. -B./build -G Ninja
cmake --build ./build --config Release --target all --
```

## 5. Setup Data Files

```bash
mkdir -p ./build/share/pokerth
cp -r tls/ ./build/
cp -r data ./build/share/pokerth/

# Create data symlink for server (required!)
mkdir -p ./build/bin
ln -s ./build/share/pokerth/data ./build/bin/data
```

## 6. Run the Official Server

**Terminal 1 - Start PokerTH server:**

```bash
cd ~/pokerth

# Start the PokerTH server
./restart_server.sh

# Monitor logs in another terminal (see below)
```

**Terminal 2 - Monitor logs:**
```bash
tail -f ~/.pokerth/log-files/server_messages.log
```

You should see:
```
MSG: Starting PokerTH dedicated server.
MSG: Successfully connected to database.
```

## 7. Run Clients

**Terminal 3 - First Client (demo1/demo):**
```bash
cd ~/pokerth
./run_client1.sh
```

Then in the PokerTH GUI:
1. Click "Join Network Game" (or "Internet Game")
2. Server: `127.0.0.1` (should be auto-filled)
3. Login: `demo1` / `demo1`
4. Click "Connect"

**Terminal 4 - Second Client (demo2/demo2):**
```bash
cd ~/pokerth
./run_client2.sh
```

Then in the PokerTH GUI:
1. Click "Join Network Game" (or "Internet Game")
2. Server: `127.0.0.1` (should be auto-filled)
3. Login: `demo2` / `demo2`
4. Click "Connect"

## 8. Server Management

```bash
# Stop server
./restart_server.sh stop

# Or force kill
pkill -f pokerth_official_server

# Restart server
./restart_server.sh
```

## 9. Monitoring Commands

**Watch server messages:**
```bash
tail -f ~/.pokerth/log-files/server_messages.log
```

**Watch server statistics:**
```bash
tail -f ~/.pokerth/log-files/server_statistics.log
```

**View recent logs (last 50 lines):**
```bash
tail -50 ~/.pokerth/log-files/server_messages.log
```

## 10. Troubleshooting

### "Avatar directory does not exist"
Verify the data symlink was created:
```bash
ls -la ~/pokerth/build/bin/data/gfx/avatars/default/
```
If empty, recreate:
```bash
mkdir -p ~/pokerth/build/bin
ln -sf ~/pokerth/build/share/pokerth/data ~/pokerth/build/bin/data
```

### "Cannot bind/listen on port"
This usually means a previous server run left a stale PID file. Force kill and restart:
```bash
pkill -f pokerth_official_server
sleep 1
./restart_server.sh
```

If the port still appears in use:
```bash
# Check what's using port 7234
netstat -tlnp | grep 7234
# Kill any remaining process
sudo kill -9 <PID>
./restart_server.sh
```

### "Could not download the PokerTH internet server list"
Make sure the HTTP server is running to serve the serverlist.xml:
```bash
# Check if serverlist.xml exists
ls -la ~/pokerth/build/share/pokerth/data/serverlist.xml

# Start the HTTP server (in a separate terminal)
./serve_serverlist.sh

# Or manually serve it
cd ~/pokerth/build/share/pokerth/data
python3 -m http.server 8000
```

Then restart the client. The client downloads from `http://127.0.0.1:8000/serverlist.xml`.

### Check server logs:
```bash
tail -30 ~/.pokerth/log-files/server_messages.log
```

### Client GUI won't start (headless WSL)
Install VcXsrv or similar X server on Windows, then:
```bash
export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0
```

Or run with X410/WinWSL:
```bash
export DISPLAY=:0
./run_client1.sh
```

### Clean rebuild
```bash
rm -rf ~/.pokerth
rm -rf ~/pokerth/.pokerth_client1
rm -rf ~/pokerth/.pokerth_client2
rm -rf ~/pokerth/build
cmake -DCMAKE_BUILD_TYPE:STRING=Release -S. -B./build -G Ninja
cmake --build ./build --config Release --target all --

# Re-setup data files
mkdir -p ~/pokerth/build/share/pokerth
cp -r ~/pokerth/tls/ ~/pokerth/build/
cp -r ~/pokerth/data ~/pokerth/build/share/pokerth/
mkdir -p ~/pokerth/build/bin
ln -sf ~/pokerth/build/share/pokerth/data ~/pokerth/build/bin/data
```

## Quick Reference Summary

| Command | Terminal |
|---------|----------|
| `./restart_server.sh` | 1 |
| `tail -f ~/.pokerth/log-files/server_messages.log` | 2 |
| `./run_client1.sh` | 3 |
| `./run_client2.sh` | 4 |
