# AGENTS.md - PokerTH Development Guide

This document provides guidelines for AI agents working on the PokerTH codebase.

## Build Commands

### CMake Build (Primary)
```bash
# Configure (Release build)
cmake -DCMAKE_BUILD_TYPE:STRING=Release -S. -B./build -G Ninja

# Build all targets
cmake --build ./build --config Release --target all --

# Build specific target
cmake --build ./build --target pokerth_lib

# Clean rebuild
rm -rf ./build && cmake -DCMAKE_BUILD_TYPE:STRING=Release -S. -B./build -G Ninja
cmake --build ./build --config Release --target all --
```

### Data Setup (Required after build)
```bash
mkdir -p ./build/share/pokerth
cp -r tls/ ./build/
cp -r data ./build/share/pokerth/
mkdir -p ./build/bin
ln -sf ./build/share/pokerth/data ./build/bin/data
```

### Run Server
```bash
./restart_server.sh          # Start server
./restart_server.sh stop     # Stop server
```

### Run Client
```bash
./run_client1.sh             # First client instance
./run_client2.sh             # Second client instance
```

### Tests (Java JUnit)
```bash
# Compile and run all tests
cd tests && ant test

# Run specific test class
cd tests && ant -Dtest.class=de.pokerth.test.GameListTest test
```

## Code Style Guidelines

### Copyright Header
All source files must include the AGPL header:
```cpp
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
 *****************************************************************************/
```

### C++ Standard
- Use C++23 (set in CMakeLists.txt: `set(CMAKE_CXX_STANDARD 23)`)
- Enable exceptions and RTTI: `-fexceptions -frtti`

### Naming Conventions
- **Classes**: PascalCase (e.g., `Game`, `PlayerInterface`, `ClientCallback`)
- **Functions**: camelCase (e.g., `getPlayerData()`, `signalNetClientConnect()`)
- **Variables**: camelCase (e.g., `myGameId`, `currentHandId`, `startQuantityPlayers`)
- **Member variables**: Prefix with `my` (e.g., `myFactory`, `myGui`, `myLog`)
- **Constants**: UPPER_SNAKE_CASE (e.g., `MAX_NUMBER_OF_PLAYERS`)
- **Interfaces**: Suffix with `Interface` (e.g., `PlayerInterface`, `BoardInterface`)
- **Callbacks**: Prefix with `Signal` (e.g., `SignalNetClientConnect()`)

### File Organization
- Headers in `src/` with subdirectories by module (engine/, net/, gui/, config/, core/)
- Implementation matches header location
- Include order: project headers first, then Boost, then system/STL
- Use forward declarations where possible to reduce includes

### Imports and Dependencies
```cpp
#include "game.h"                  // Project header (same module)
#include <enginefactory.h>         // Project header (different module)
#include <boost/shared_ptr.hpp>    // Boost
#include <string>                  // STL
#include <iostream>                // STL
```
- Avoid `using namespace std;` in headers
- Allowed in `.cpp` files after includes

### Error Handling
- Use exceptions for exceptional conditions (see `LocalException`, `NetException`)
- Throw with file/line info: `throw LocalException(__FILE__, __LINE__, ERR_DEALER_NOT_FOUND);`
- Use custom exception classes from `src/core/pokerthexception.h` and `src/net/netexception.h`
- Check for null pointers and invalid states

### Qt Conventions
- Qt6 required (minimum 6.7.0)
- Use Qt's signal/slot mechanism for GUI events
- Prefix member variables with `m_` for Qt widgets
- Use `Q_OBJECT` macro in classes with signals/slots
- Follow Qt naming for UI elements

### Memory Management
- Use `boost::shared_ptr<T>` for shared ownership
- Use raw pointers for non-owning references
- Avoid raw `new`/`delete`; use smart pointers or Qt's parent system for widgets

### Protobuf
- Protocol files: `pokerth.proto`, `chatcleaner.proto`
- Generated code in `src/third_party/protobuf/`
- Regenerate with: `protobuf_generate_cpp()` in CMakeLists.txt

### Logging
- Use the `Log` class from `src/engine/log.h`
- Check `DEBUG_MODE` flag for debug-only code paths

### Testing
- Java JUnit tests in `tests/src/de/pokerth/test/`
- Test classes extend `TestBase`
- AllTests.java aggregates all tests
- Use the same AGPL header in test files

### Compilation Flags
- `-Wno-stringop-overflow` (used in project)
- `-fexceptions -frtti` (required)
- `-DENABLE_IPV6 -DHAVE_OPENSSL -DBOOST_FILESYSTEM_DEPRECATED` (defines)

### Common Issues
- **Avatar directory missing**: Ensure data symlink is created after build
- **Port in use**: Stale PID file; run `./restart_server.sh stop` then restart
- **Database**: MariaDB/MySQL required; see BUILD_GUIDE.md for schema
