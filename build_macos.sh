#!/usr/bin/env bash

###
# you need to install latest xcode and xcode commandline tools before running this script

set -euo pipefail

########################################
# Configuration
########################################

BREW_PREFIX_DEFAULT="/opt/homebrew"   # Apple Silicon
VCPKG_DIR="$HOME/vcpkg"
PYTHON_USER_BASE="$HOME/.local"
AQT_BIN="$PYTHON_USER_BASE/bin/aqt"

########################################
# Helper functions
########################################

log() {
  echo "▶ $1"
}

command_exists() {
  command -v "$1" >/dev/null 2>&1
}

########################################
# 1. Homebrew
########################################

if ! command_exists brew; then
  log "Installing Homebrew…"
  NONINTERACTIVE=1 /bin/bash -c \
    "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

  # shellenv (Apple Silicon)
  if [ -d "$BREW_PREFIX_DEFAULT" ]; then
    eval "$($BREW_PREFIX_DEFAULT/bin/brew shellenv)"
  fi
else
  log "Homebrew already installed"
fi

########################################
# 2. Base packages
########################################

log "Installing base packages via Homebrew…"
brew update
brew install \
  cmake \
  ninja \
  git \
  python \
  pkg-config

########################################
# 3. pipx
########################################

if ! command_exists pipx; then
  log "Installing pipx…"
  brew install pipx
  pipx ensurepath
else
  log "pipx already installed"
fi

########################################
# 4. aqtinstall (via pipx)
########################################

if ! command_exists aqt; then
  log "Installing aqtinstall via pipx…"
  pipx install aqtinstall
else
  log "aqtinstall already installed"
fi

export PATH="$HOME/.local/bin:$PATH"

########################################
# 5. vcpkg
########################################

if [ ! -d "$VCPKG_DIR" ]; then
  log "Cloning vcpkg…"
  git clone https://github.com/microsoft/vcpkg.git "$VCPKG_DIR"
else
  log "vcpkg directory already exists"
fi

log "Bootstrapping vcpkg…"
"$VCPKG_DIR/bootstrap-vcpkg.sh"


########################################
# vcpkg dependencies
########################################

brew install \
  cmake \
  ninja \
  git \
  python \
  pkg-config \
  autoconf \
  autoconf-archive \
  automake \
  libtool

declare -a VCPKG_PORTS=(
  boost
  protobuf
  curl
)

# Architektur bestimmen
if [[ "$(uname -m)" == "arm64" ]]; then
  VCPKG_TRIPLET="arm64-osx"
else
  VCPKG_TRIPLET="x64-osx"
fi

log "Installing vcpkg dependencies (${VCPKG_TRIPLET})…"
"$HOME/vcpkg/vcpkg" install \
  --triplet="$VCPKG_TRIPLET" \
  "${VCPKG_PORTS[@]}"

########################################
# Qt installation (aqtinstall)
########################################

QT_VERSION="6.10.0"
QT_OUTPUT_DIR="$HOME/Qt"

QT_MODULES=(
  qt3d
  qt5compat
  qtcharts
  qtconnectivity
  qtdatavis3d
  qtgraphs
  qtgrpc
  qthttpserver
  qtimageformats
  qtlocation
  qtlottie
  qtmultimedia
  qtnetworkauth
  qtpositioning
  qtquick3d
  qtquick3dphysics
  qtquicktimeline
  qtremoteobjects
  qtscxml
  qtsensors
  qtserialbus
  qtserialport
  qtshadertools
  qtspeech
  qtvirtualkeyboard
  qtwebchannel
  qtwebsockets
  qtwebview
)

log "Installing Qt ${QT_VERSION} for macOS (clang_64) with modules…"
aqt install-qt mac desktop "$QT_VERSION" clang_64 \
  --outputdir "$QT_OUTPUT_DIR" \
  --modules "${QT_MODULES[@]}"

QT_DIR="$QT_OUTPUT_DIR/$QT_VERSION/macos"
log "Qt installed at: $QT_DIR"

########################################
# Build PokerTH
########################################

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build_macos"

log "Configuring CMake build…"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0

log "Building pokerth_client…"
ninja -C "$BUILD_DIR" pokerth_client

########################################
# Create macOS App Bundle
########################################

APP_NAME="PokerTH"
APP_BUNDLE="$BUILD_DIR/${APP_NAME}.app"
APP_CONTENTS="$APP_BUNDLE/Contents"
APP_MACOS="$APP_CONTENTS/MacOS"
APP_RESOURCES="$APP_CONTENTS/Resources"

log "Creating app bundle structure…"
mkdir -p "$APP_MACOS"
mkdir -p "$APP_RESOURCES"

log "Copying binary and resources…"
cp "$BUILD_DIR/bin/pokerth_client" "$APP_MACOS/$APP_NAME"
cp -r "$SCRIPT_DIR/data" "$APP_RESOURCES/"

log "Creating Info.plist…"
cat > "$APP_CONTENTS/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>$APP_NAME</string>
    <key>CFBundleIdentifier</key>
    <string>org.pokerth.PokerTH</string>
    <key>CFBundleName</key>
    <string>$APP_NAME</string>
    <key>CFBundleDisplayName</key>
    <string>$APP_NAME</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>CFBundleVersion</key>
    <string>1.0</string>
    <key>CFBundleIconFile</key>
    <string>pokerth.icns</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
EOF

log "Deploying Qt frameworks with macdeployqt…"
"$QT_DIR/bin/macdeployqt" "$APP_BUNDLE" -verbose=1

########################################
# Code Signing (Optional)
########################################

# Check if CODESIGN_IDENTITY environment variable is set
# Usage: export CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAM_ID)"
if [ -n "${CODESIGN_IDENTITY:-}" ]; then
    log "Code signing with identity: $CODESIGN_IDENTITY"
    
    # Sign all frameworks and dylibs first
    find "$APP_BUNDLE/Contents/Frameworks" -type f \( -name "*.dylib" -o -name "Qt*" \) -exec codesign --force --sign "$CODESIGN_IDENTITY" --timestamp --options runtime {} \;
    
    # Sign the main executable
    codesign --force --sign "$CODESIGN_IDENTITY" --timestamp --options runtime "$APP_BUNDLE/Contents/MacOS/PokerTH"
    
    # Sign the app bundle
    codesign --force --sign "$CODESIGN_IDENTITY" --timestamp --options runtime --entitlements /dev/null "$APP_BUNDLE"
    
    # Verify signature
    codesign --verify --deep --strict --verbose=2 "$APP_BUNDLE"
    
    log "Code signing complete!"
else
    log "Skipping code signing (CODESIGN_IDENTITY not set)"
    echo "  To enable code signing, set: export CODESIGN_IDENTITY=\"Developer ID Application: Your Name (TEAM_ID)\""
fi

########################################
# Create DMG
########################################

DMG_NAME="${APP_NAME}.dmg"
DMG_PATH="$BUILD_DIR/$DMG_NAME"

log "Creating DMG installer…"
rm -f "$DMG_PATH"
hdiutil create -volname "$APP_NAME" -srcfolder "$APP_BUNDLE" -ov -format UDZO "$DMG_PATH"

########################################
# Summary
########################################

log "Build complete!"
echo ""
echo "✓ App Bundle: $APP_BUNDLE"
echo "✓ DMG Installer: $DMG_PATH"
echo ""
echo "To run: open $APP_BUNDLE"
echo "To install: open $DMG_PATH"
########################################


