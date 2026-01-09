# PokerTH

## Project Overview

PokerTH is an open-source Texas Hold'em engine and game, written in C++ using the Qt framework (Qt 6). It supports single-player games against AI, network games, and running a dedicated server.

### Key Technologies
*   **Language:** C++ (C++23 standard)
*   **Framework:** Qt 6 (Core, Sql, Xml, WebSockets, Qml, Quick, Widgets, Network, Multimedia)
*   **Build System:** CMake + Ninja
*   **Networking:** Protocol Buffers (protobuf) for data serialization, Boost.Asio (inferred) for sockets, cURL.
*   **Database:** MariaDB/MySQL (for the dedicated server).
*   **Dependencies:** Boost (iostreams, random, thread, filesystem, program_options), OpenSSL, zlib.

## Directory Structure

*   `src/`: Main source code.
    *   `pokerth.cpp`: Client application entry point.
    *   `pokerth_server.cpp`: Dedicated server entry point.
    *   `engine/`: Core game logic (rules, hand evaluation, deck management).
    *   `gui/`: Qt-based graphical user interface (Widgets and QML).
    *   `net/`: Networking implementation (client/server communication).
    *   `db/` & `dbofficial/`: Database abstraction and implementation.
    *   `core/`: Utility classes (threads, crypto, avatars).
    *   `third_party/`: Includes and generated protobuf code.
*   `data/`: Game assets (graphics, sounds, fonts, translations).
*   `cmake/`: Custom CMake modules.
*   `tests/`: Unit tests.
*   `build/`: Build artifacts (not tracked).
*   `tls/`: SSL/TLS certificates for the server.

## Building and Running

### Prerequisites
Ensure the following are installed (based on Ubuntu/Debian):
*   `cmake`, `ninja-build`, `build-essential`
*   Qt 6 development packages (`qt6-base-dev`, etc.)
*   Boost development libraries (`libboost-all-dev`)
*   Protocol Buffers (`libprotobuf-dev`, `protobuf-compiler`)
*   OpenSSL, cURL, zlib
*   MariaDB server (for dedicated server functionality)

### Build Instructions

The project uses CMake. A typical build sequence:

```bash
# 1. Configure
cmake -DCMAKE_BUILD_TYPE:STRING=Release -S . -B ./build -G Ninja

# 2. Build
cmake --build ./build --config Release --target all --
```

### Running the Application

Helper scripts are provided in the root directory:

*   **Dedicated Server:**
    ```bash
    ./restart_server.sh
    # Logs are at ~/.pokerth/log-files/server_messages.log
    ```

*   **Client (GUI):**
    ```bash
    ./run_client1.sh  # Launches client 1 (demo1)
    ./run_client2.sh  # Launches client 2 (demo2)
    ```

*   **Data Setup (Critical):**
    The build requires `data/` to be linked or copied to `build/bin/data`. The `BUILD_GUIDE.md` details this step:
    ```bash
    mkdir -p ./build/bin
    ln -s ../share/pokerth/data ./build/bin/data
    ```

## Development Conventions

*   **Code Style:** Modern C++ (C++23).
*   **Logging:** Use project-specific macros like `LOG_MSG` and `LOG_ERROR`.
*   **Configuration:** Settings are stored in XML format, managed by the `ConfigFile` class.
*   **Protocol:** Network messages are defined in `.proto` files (`pokerth.proto`, `chatcleaner.proto`).
*   **Database:** The server uses a MySQL/MariaDB database. SQL schema setup is described in `BUILD_GUIDE.md`.

## Key Configuration Files

*   `CMakeLists.txt`: Main build configuration.
*   `BUILD_GUIDE.md`: Comprehensive guide for setting up the environment, database, and running the game.
*   `pokerth.proto`: Definition of the network protocol.
*   `pokerth_game.pro`: Legacy QMake project file (likely superseded by CMake but still present).
