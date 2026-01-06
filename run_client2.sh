#!/bin/bash
# PokerTH Client 2 - Run second client instance

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
CLIENT_BIN="${BUILD_DIR}/bin/pokerth_client"

# Use separate directories for this instance
export HOME="${SCRIPT_DIR}/.pokerth_client2"
export XDG_CONFIG_HOME="${HOME}/.config"
export XDG_CACHE_HOME="${HOME}/.cache"
export TMPDIR="${HOME}/.tmp"

# Ensure required directories exist
mkdir -p "${XDG_CONFIG_HOME}/.pokerth"
mkdir -p "${XDG_CACHE_HOME}/.pokerth"
mkdir -p "${TMPDIR}"

# Copy/symlink data files
mkdir -p "${XDG_CONFIG_HOME}/.pokerth/data"
if [ ! -e "${XDG_CONFIG_HOME}/.pokerth/data/gfx" ]; then
    ln -s "${BUILD_DIR}/share/pokerth/data/gfx" "${XDG_CONFIG_HOME}/.pokerth/data/gfx"
fi

echo "=== Starting PokerTH Client 2 ==="
echo "HOME: ${HOME}"
echo ""

cd "${BUILD_DIR}"
"$CLIENT_BIN" "$@"
