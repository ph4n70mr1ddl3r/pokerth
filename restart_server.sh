#!/bin/bash
# PokerTH Official Server Restart Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
SERVER_BIN="${BUILD_DIR}/bin/pokerth_official_server"
PID_FILE="${HOME}/.pokerth/log-files/pokerth.pid"
SERVER_NAME="pokerth_official_server"

echo "=== PokerTH Official Server Restart ==="

# Function to kill process by name
kill_by_name() {
    local pids=$(pgrep -f "$SERVER_NAME" 2>/dev/null || true)
    if [ -n "$pids" ]; then
        for pid in $pids; do
            if kill -0 "$pid" 2>/dev/null; then
                echo "Stopping server (PID: $pid, name: $SERVER_NAME)..."
                kill "$pid"
                local TIMEOUT=10
                while kill -0 "$pid" 2>/dev/null && [ $TIMEOUT -gt 0 ]; do
                    sleep 1
                    ((TIMEOUT--))
                done
                if kill -0 "$pid" 2>/dev/null; then
                    echo "Force killing server..."
                    kill -9 "$pid"
                fi
            fi
        done
        echo "Server(s) stopped."
    fi
}

# Stop the server (by name first, then by PID file)
kill_by_name

if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if kill -0 "$PID" 2>/dev/null; then
        echo "Stopping server (PID: $PID)..."
        kill "$PID"
        TIMEOUT=10
        while kill -0 "$PID" 2>/dev/null && [ $TIMEOUT -gt 0 ]; do
            sleep 1
            ((TIMEOUT--))
        done
        if kill -0 "$PID" 2>/dev/null; then
            echo "Force killing server..."
            kill -9 "$PID"
        fi
        echo "Server stopped."
    else
        echo "Server not running (stale PID file)."
    fi
    rm -f "$PID_FILE"
else
    echo "No PID file found. Server may not be running."
fi

# Start the server
if [ -x "$SERVER_BIN" ]; then
    echo "Starting server..."
    cd "$BUILD_DIR"
    "$SERVER_BIN" "$@"
    echo "Server started."
else
    echo "Error: Server binary not found at $SERVER_BIN"
    echo "Build with: cmake --build ./build --target pokerth_official_server"
    exit 1
fi

echo "=== Done ==="
