#!/usr/bin/env bash
set -euo pipefail

# Unified local Android setup + build script for PokerTH on Ubuntu 25.10
# This script combines the Dockerfile's environment setup steps (installing
# packages, Android SDK/NDK, Gradle, vcpkg, optional Qt) and then calls the
# repo's `build_android.sh` to run the CMake + androiddeployqt steps.
#
# WARNING: Many operations are long (Qt download, vcpkg builds). The script
# asks before doing heavy installs and supports flags to skip parts.

ROOT=${ROOT:-/opt/pokerth-android}
ANDROID_ARCH=${ANDROID_ARCH:-arm64-v8a}
VCPKG_ARCH=${VCPKG_ARCH:-arm64}
QT_ARCH=${QT_ARCH:-arm64_v8a}
TARGET=${TARGET:-pokerth_client}
ANDROID_API_LEVEL=${ANDROID_API_LEVEL:-35}
QT_VERSION=${QT_VERSION:-6.9.3}
GRADLE_VERSION=${GRADLE_VERSION:-8.3}
ANDROID_NDK_VERSION=${ANDROID_NDK_VERSION:-28.0.13004108}
VCPKG_ROOT=${VCPKG_ROOT:-${ROOT}/vcpkg}
QT_ANDROID_DIR=${QT_ANDROID_DIR:-${ROOT}/Qt/${QT_VERSION}/android_${QT_ARCH}}
QT_HOST_PATH=${QT_HOST_PATH:-${ROOT}/Qt/${QT_VERSION}/linux_gcc_64}

FORCE_NO_QT=0
FORCE_NO_VCPKG=0
YES=0

usage(){
  cat <<EOF
Usage: $0 [--arch ARCH] [--target TARGET] [--api-level N] [--no-qt] [--no-vcpkg] [--yes]

Options:
  --arch ARCH          Android ABI (armeabi-v7a|arm64-v8a|x86|x86_64) default: ${ANDROID_ARCH}
  --target TARGET      CMake target to build (pokerth_client or pokerth_qml-client)
  --api-level N        Android API level (default: ${ANDROID_API_LEVEL})
  --no-qt              Skip installing Qt (very long)
  --no-vcpkg           Skip installing vcpkg and packages
  --yes                Assume yes for prompts
  -h, --help           Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch) ANDROID_ARCH="$2"; shift 2;;
    --target) TARGET="$2"; shift 2;;
    --api-level) ANDROID_API_LEVEL="$2"; shift 2;;
    --no-qt) FORCE_NO_QT=1; shift 1;;
    --no-vcpkg) FORCE_NO_VCPKG=1; shift 1;;
    --yes) YES=1; shift 1;;
    -h|--help) usage; exit 0;;
    *) echo "Unknown arg: $1"; usage; exit 1;;
  esac
done

echo "=== PokerTH local Android setup + build ==="
echo "ROOT=$ROOT ARCH=${ANDROID_ARCH} TARGET=${TARGET} API=${ANDROID_API_LEVEL} QT_VER=${QT_VERSION}"

confirm(){
  if [[ $YES -eq 1 ]]; then
    return 0
  fi
  read -r -p "$1 [y/N]: " ans
  case "$ans" in
    [yY]|[yY][eE][sS]) return 0;;
    *) return 1;;
  esac
}

require_sudo(){
  if [[ $EUID -ne 0 ]]; then
    if ! command -v sudo >/dev/null 2>&1; then
      echo "This script needs root privileges for some operations. Please install sudo or run as root." >&2
      exit 1
    fi
  fi
}

install_apt_packages(){
  echo "Installing required apt packages (requires sudo)..."
  sudo apt-get update
  sudo apt-get install -y --no-install-recommends \
    ca-certificates curl wget unzip git build-essential pkg-config \
    python3 python3-pip python3-venv jq \
    libwebsocketpp-dev libboost1.88-all-dev locales sudo nano mc \
    openjdk-17-jdk-headless cmake ninja-build zip unzip
  sudo apt-get autoremove -y && sudo rm -rf /var/lib/apt/lists/* /var/cache/apt/archives/* || true
}

install_android_cmdline_tools(){
  echo "Installing Android command line tools into ${ANDROID_SDK_ROOT}..."
  mkdir -p "${ANDROID_SDK_ROOT}" || sudo mkdir -p "${ANDROID_SDK_ROOT}" && sudo chown -R "$USER":"$USER" "${ROOT}" || true
  TMPDIR=$(mktemp -d)
  pushd "$TMPDIR" >/dev/null
  curl -fSL https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip -o cmdline-tools.zip
  unzip -q cmdline-tools.zip
  mkdir -p "${ANDROID_SDK_ROOT}/cmdline-tools/latest"
  # Move contents into proper path
  if [[ -d cmdline-tools ]]; then
    mv cmdline-tools/* "${ANDROID_SDK_ROOT}/cmdline-tools/latest/" || true
  else
    mv commandlinetools*/* "${ANDROID_SDK_ROOT}/cmdline-tools/latest/" || true
  fi
  popd >/dev/null
  rm -rf "$TMPDIR"
}

install_sdk_components(){
  echo "Installing Android SDK/NDK components (may take time)..."
  export ANDROID_SDK_ROOT
  yes | "${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin/sdkmanager" --sdk_root="${ANDROID_SDK_ROOT}" --licenses || true
  "${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin/sdkmanager" --sdk_root="${ANDROID_SDK_ROOT}" \
    "platform-tools" "platforms;android-${ANDROID_API_LEVEL}" "build-tools;33.0.2" "ndk;${ANDROID_NDK_VERSION}" "cmake;3.22.1"
}

install_gradle(){
  echo "Installing Gradle ${GRADLE_VERSION} into ${ROOT}/gradle..."
  mkdir -p "${ROOT}/gradle"
  curl -fSL https://services.gradle.org/distributions/gradle-${GRADLE_VERSION}-bin.zip -o /tmp/gradle.zip
  unzip -q /tmp/gradle.zip -d "${ROOT}/gradle"
  rm -f /tmp/gradle.zip
}

install_python_tools(){
  echo "Creating Python venv and installing aqtinstall..."
  python3 -m venv "${ROOT}/venv"
  export PATH="${ROOT}/venv/bin:$PATH"
  pip install --upgrade pip
  pip install aqtinstall
}

install_qt_via_aqt(){
  if [[ $FORCE_NO_QT -eq 1 ]]; then
    echo "Skipping Qt install as requested (--no-qt). Ensure you have a suitable Qt for Android installed and set QT_ANDROID_DIR and QT_HOST_PATH.";
    return
  fi
  echo "Installing Qt ${QT_VERSION} for Android (this can take a long time)..."
  mkdir -p "${ROOT}/Qt"
  aqt install-qt all_os android ${QT_VERSION} android_${QT_ARCH} --autodesktop --modules qt3d qt5compat qtcharts qtconnectivity qtdatavis3d qtgraphs qtgrpc qthttpserver qtimageformats qtlocation qtlottie qtmultimedia qtnetworkauth qtpositioning qtquick3d qtquick3dphysics qtquicktimeline qtremoteobjects qtscxml qtsensors qtserialbus qtserialport qtshadertools qtspeech qtvirtualkeyboard qtwebchannel qtwebsockets qtwebview || true
  # install host desktop Qt as QT_HOST_PATH
  aqt install-qt linux desktop ${QT_VERSION} linux_gcc_64 --modules qt3d qt5compat qtcharts qtconnectivity qtdatavis3d qtgraphs qtgrpc qthttpserver qtimageformats qtlocation qtlottie qtmultimedia qtnetworkauth qtpositioning qtquick3d qtquick3dphysics qtquicktimeline qtremoteobjects qtscxml qtsensors qtserialbus qtserialport qtshadertools qtspeech qtvirtualkeyboard qtwebchannel qtwebsockets qtwebview || true
  # Set final env vars
  QT_ANDROID_DIR="${ROOT}/Qt/${QT_VERSION}/android_${QT_ARCH}"
  QT_HOST_PATH="${ROOT}/Qt/${QT_VERSION}/linux_gcc_64"
}

install_vcpkg_and_packages(){
  if [[ $FORCE_NO_VCPKG -eq 1 ]]; then
    echo "Skipping vcpkg installation as requested (--no-vcpkg).";
    return
  fi
  echo "Installing vcpkg and required packages (this may take long)..."
  git clone https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}"
  "${VCPKG_ROOT}/bootstrap-vcpkg.sh"
  "${VCPKG_ROOT}/vcpkg" install \
    boost-system:${VCPKG_ARCH}-android \
    boost-filesystem:${VCPKG_ARCH}-android \
    boost-thread:${VCPKG_ARCH}-android boost-regex:${VCPKG_ARCH}-android \
    boost-chrono:${VCPKG_ARCH}-android \
    boost-date-time:${VCPKG_ARCH}-android boost-serialization:${VCPKG_ARCH}-android \
    boost-asio:${VCPKG_ARCH}-android \
    boost-interprocess:${VCPKG_ARCH}-android \
    boost-iostreams:${VCPKG_ARCH}-android \
    boost-program-options:${VCPKG_ARCH}-android \
    boost-lambda:${VCPKG_ARCH}-android \
    boost-foreach:${VCPKG_ARCH}-android \
    boost-uuid:${VCPKG_ARCH}-android \
    openssl:${VCPKG_ARCH}-android \
    curl:${VCPKG_ARCH}-android \
    protobuf:x64-linux || true
}

overlay_protobuf_and_reinstall(){
  echo "Creating vcpkg protobuf overlay and reinstalling protobuf for Android..."
  mkdir -p "${ROOT}/vcpkg-overlays/protobuf"
  cp -r "${VCPKG_ROOT}/ports/protobuf"/* "${ROOT}/vcpkg-overlays/protobuf/" || true
  # Add a small linker flags tweak like in Dockerfile
  sed -i '1i\# Workaround für TLS-Emulation\nset(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-warn-execstack")\nset(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-warn-execstack")\n' "${ROOT}/vcpkg-overlays/protobuf/portfile.cmake" || true
  "${VCPKG_ROOT}/vcpkg" remove protobuf:${VCPKG_ARCH}-android --recurse || true
  rm -rf "${VCPKG_ROOT}/buildtrees/protobuf" || true
  rm -rf "${VCPKG_ROOT}/packages/protobuf_${VCPKG_ARCH}-android" || true
  rm -rf /tmp/vcpkg-buildtrees/protobuf || true
  "${VCPKG_ROOT}/vcpkg" install protobuf:${VCPKG_ARCH}-android --overlay-ports="${ROOT}/vcpkg-overlays/protobuf" --x-buildtrees-root=/tmp/vcpkg-buildtrees --no-binarycaching || true
}

clone_pokerth_repo(){
  echo "Cloning PokerTH into ${ROOT}/pokerth (if not present)"
  mkdir -p "${ROOT}"
  if [[ ! -d "${ROOT}/pokerth" ]]; then
    git clone https://github.com/pokerth/pokerth.git "${ROOT}/pokerth"
    pushd "${ROOT}/pokerth" >/dev/null
    git checkout stable || true
    popd >/dev/null
  else
    echo "Repository already exists at ${ROOT}/pokerth"
  fi
}

run_repo_build_script(){
  echo "Running repository build script (build_android.sh) with environment variables set..."
  export ANDROID_SDK_ROOT=${ANDROID_SDK_ROOT:-${ROOT}/android-sdk}
  export ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT:-${ANDROID_SDK_ROOT}/ndk/${ANDROID_NDK_VERSION}}
  export JAVA_HOME=${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk-amd64}
  export QT_ANDROID_DIR=${QT_ANDROID_DIR}
  export QT_HOST_PATH=${QT_HOST_PATH}
  export VCPKG_ROOT=${VCPKG_ROOT}
  export ANDROID_ARCH=${ANDROID_ARCH}
  export ANDROID_API_LEVEL=${ANDROID_API_LEVEL}
  export TARGET=${TARGET}

  # Ensure the build script in the repo is executable and call it
  REPO_BUILD_SCRIPT="$(pwd)/docker/android/build_android.sh"
  if [[ ! -f "$REPO_BUILD_SCRIPT" ]]; then
    # try repository path under ROOT
    REPO_BUILD_SCRIPT="${ROOT}/pokerth/docker/android/build_android.sh"
  fi
  if [[ ! -f "$REPO_BUILD_SCRIPT" ]]; then
    echo "ERROR: cannot find build_android.sh in repo. Expected at ${REPO_BUILD_SCRIPT}" >&2
    exit 1
  fi
  chmod +x "$REPO_BUILD_SCRIPT"
  "$REPO_BUILD_SCRIPT"
}

# Main flow
require_sudo

ANDROID_SDK_ROOT=${ANDROID_SDK_ROOT:-${ROOT}/android-sdk}

if [[ $YES -ne 1 ]]; then
  echo "This script will perform system package installs and large downloads into ${ROOT}."
fi

if confirm "Proceed with installing apt packages and Android commandline tools?"; then
  install_apt_packages
  install_android_cmdline_tools
  install_sdk_components
  install_gradle
  install_python_tools
else
  echo "Skipping base installs. Ensure required tools are present: curl, unzip, git, cmake, ninja, openjdk, python3, sdkmanager.";
fi

if [[ $FORCE_NO_QT -eq 0 ]]; then
  if confirm "Install Qt ${QT_VERSION} for Android now? (very long)"; then
    install_qt_via_aqt
  else
    echo "Skipping Qt install. Make sure to set QT_ANDROID_DIR and QT_HOST_PATH before building.";
  fi
fi

if [[ $FORCE_NO_VCPKG -eq 0 ]]; then
  if confirm "Install vcpkg and requested packages now? (may take long)"; then
    install_vcpkg_and_packages
    overlay_protobuf_and_reinstall
  else
    echo "Skipping vcpkg install.";
  fi
fi

clone_pokerth_repo

echo "Ready to run the build. The script will now invoke the repo's build script which performs CMake and androiddeployqt steps."
if confirm "Run repo build script now?"; then
  run_repo_build_script
else
  echo "Done. To build later, run:"
  echo "  export ANDROID_SDK_ROOT=${ANDROID_SDK_ROOT} ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT} JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 QT_ANDROID_DIR=${QT_ANDROID_DIR} QT_HOST_PATH=${QT_HOST_PATH} VCPKG_ROOT=${VCPKG_ROOT} TARGET=${TARGET}"
  echo "  bash docker/android/build_android.sh --arch ${ANDROID_ARCH} --api-level ${ANDROID_API_LEVEL} --build-type Release"
fi

exit 0
