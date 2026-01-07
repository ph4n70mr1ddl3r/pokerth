# AGENTS.md - PokerTH Development Guide

This file provides guidelines for agentic coding agents working on the PokerTH codebase.

## Build Commands

### Full Build
```bash
cmake -DCMAKE_BUILD_TYPE:STRING=Release -S. -B./build -G Ninja
cmake --build ./build --config Release --target all --
```

### Clean Rebuild
```bash
rm -rf ./build
cmake -DCMAKE_BUILD_TYPE:STRING=Release -S. -B./build -G Ninja
cmake --build ./build --config Release --target all --
```

### Build Specific Target
```bash
cmake --build ./build --target pokerth_tests  # Build tests only
cmake --build ./build --target pokerth_lib    # Build library only
```

### Debug Build
```bash
cmake -DCMAKE_BUILD_TYPE:STRING=Debug -S. -B./build -G Ninja
cmake --build ./build --config Debug --target all --
```

## Test Commands

### Run All Tests
```bash
./build/bin/pokerth_tests
# or via CTest:
ctest --test-dir ./build
```

### Run Tests with Verbose Output
```bash
ctest --test-dir ./build --verbose
```

### Run Specific Test Suite
The custom test framework doesn't support filtering natively. To run specific tests, modify the test file to comment out unwanted TEST macros, then rebuild and run.

```bash
# Edit src/tests/pokerth_tests.cpp, comment out unwanted tests
# Rebuild
cmake --build ./build --target pokerth_tests
# Run
./build/bin/pokerth_tests
```

### Run Integration Tests
See `src/tests/INTEGRATION_TESTS.md` for integration test procedures.

## Code Style Guidelines

### File Headers
All source files must include the AGPLv3 license header:
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

### Indentation and Spacing
- Use **tabs** for indentation (not spaces)
- Place opening brace on new line for function definitions
- Place opening brace on same line for control statements
- Align member variable assignments with tabs

```cpp
void
MyClass::MyFunction(int param1, int param2)
{
	if (condition) {
		doSomething();
	} else {
		doOtherThing();
	}
}
```

### Naming Conventions

**Classes**: PascalCase
```cpp
class PlayerData { };
class NetPacket { };
```

**Member Variables**: Prefix with `m_`, use camelCase
```cpp
private:
	const unsigned         m_uniqueId;
	DB_id                  m_dbId;
	std::string            m_name;
	boost::mutex           m_dataMutex;
```

**Functions**: PascalCase for class methods, camelCase for free functions
```cpp
std::string GetName() const;
void SetName(const std::string &name);
void doNetworkHandshake();
```

**Constants/Enums**: UPPER_SNAKE_CASE for macros, PascalCase for enum values
```cpp
#define MAX_PACKET_SIZE    384
#define NET_VERSION_MAJOR  5

enum PlayerType {
	PLAYER_TYPE_COMPUTER,
	PLAYER_TYPE_HUMAN
};
```

**Local Variables**: camelCase
```cpp
bool retVal = false;
std::string playerName;
auto tmpPacket = std::make_shared<NetPacket>();
```

### Type System

**C++ Standard**: C++20 (set in CMakeLists.txt)
```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

**Smart Pointers**: Prefer `std::shared_ptr` for shared ownership
```cpp
std::shared_ptr<NetPacket> packet = NetPacket::Create(data, size);
std::shared_ptr<AvatarFile> avatar = std::make_shared<AvatarFile>();
```

**Thread Safety**: Use `boost::mutex` with `scoped_lock`
```cpp
mutable boost::mutex m_dataMutex;

std::string GetName() const
{
	boost::mutex::scoped_lock lock(m_dataMutex);
	return m_name;
}
```

**Primitive Types**: Use C++ types (`unsigned`, `int`, `size_t`) over C types

### Import Style

**Order**: System includes first, then project includes
```cpp
#include <memory>
#include <string>
#include <list>

#include <third_party/protobuf/pokerth.pb.h>
#include <net/netpacket.h>
#include <playerdata.h>
```

**Angle Brackets**: Use angle brackets for system/third-party, quotes for project-local
```cpp
#include <boost/thread.hpp>           // System/Boost
#include <third_party/protobuf/...>   // Third-party
#include <net/netpacket.h>            // Project local
```

### Error Handling

**Exceptions**: Use custom exception classes for domain errors
```cpp
#include <core/pokerthexception.cpp>

class MyException : public PokerTHException { };
throw PokerTHException("Descriptive error message");
```

**Return Codes**: Use integer error codes for network protocol errors
```cpp
int NetPacket::NetErrorToGameError(ErrorMessage::ErrorReason netErrorReason)
{
	int retVal;
	switch(netErrorReason) {
	case ErrorMessage::initVersionNotSupported:
		retVal = ERR_NET_VERSION_NOT_SUPPORTED;
		break;
	// ... more cases ...
	default:
		retVal = ERR_SOCK_INTERNAL;
		break;
	}
	return retVal;
}
```

**Null Checks**: Check for null pointers explicitly
```cpp
if (m_msg && m_msg->ParseFromArray(data, size)) {
	// safe to use m_msg
}
```

### Protobuf Usage

**Generated Files**: Protobuf-generated files are in `src/third_party/protobuf/`
- Never edit `.pb.h` or `.pb.cc` files directly
- Regenerate with: `protobuf_generate_cpp(...)` in CMakeLists.txt
- Include generated headers with angle brackets: `#include <third_party/protobuf/pokerth.pb.h>`

### Test Framework

**Custom macros** in `pokerth_test_framework.h`:
```cpp
TEST_SUITE(MySuite)
TEST(TestName, Description)
{
	// test code
	return true;  // true = passed
}
END_TEST_SUITE

// Assertions
EXPECT_TRUE(condition);
EXPECT_EQ(expected, actual);
EXPECT_FALSE(condition);
EXPECT_NE(expected, actual);
EXPECT_GT(a, b);
EXPECT_LT(a, b);
```

### Qt Integration

Qt6 components used: Core, Sql, Xml, WebSockets, Qml, Quick, QuickControls2, Widgets, Svg, Network, LinguistTools, Multimedia

```cpp
#include <QtCore/QObject>
#include <QtSql/QSqlDatabase>

// Qt signals/slots syntax (old style)
public slots:
	void onButtonClicked();

signals:
	void valueChanged(int newValue);
```

### Performance Considerations

- Use `BOOST_FOREACH` for iteration over containers
- Reserve vector capacity when size is known
- Use `const` for read-only parameters and methods
- Consider move semantics for large objects

### Common Patterns

**Singleton Pattern**: Use static instance holder
```cpp
static TestRunner& instance() {
	static TestRunner runner;
	return runner;
}
```

**Static Factory Method**: For object creation
```cpp
static std::shared_ptr<NetPacket> Create(const char *data, size_t dataSize);
```

**pImpl Idiom**: Not currently used, but acceptable for ABI stability

### Database

- MySQL/MariaDB via Qt6::Sql
- Table prefix: `pokerth_` for official server tables
- See `src/dbofficial/` for async DB operations

### Known Issues and Legacy Code

- Some code uses `using namespace std;` in .cpp files (avoid in headers)
- `@TODO` comments indicate unfinished work
- `BOOST_FOREACH` is used but could be replaced with range-based for loops
- Some files still use `#include <boost/foreach.hpp>` explicitly
