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

// Regression test: ServerAcceptWebHelper::Listen() has no port bounds check.
//
// Bug
// ---
// src/net/serveracceptwebhelper.cpp, ServerAcceptWebHelper::Listen():
//
//     void Listen(unsigned serverPort, ...)
//     {
//         ...
//         m_webSocketTlsServer->listen(serverPort);
//         ...
//         m_webSocketServer->listen(serverPort);
//     }
//
// The websocketpp endpoint takes a uint16_t:
//
//     endpoint.hpp:458: void listen(uint16_t port)
//
// Passing an unsigned narrows it silently. The port comes straight from the
// user-editable config file:
//
//     session.cpp:340: myConfig->readConfigInt("ServerWebSocketPort")
//       -> ServerManager::Init(..., unsigned websocketPort, ...)
//       -> webAcceptHelper->Listen(websocketPort, ...)
//
// So a configured ServerWebSocketPort of 70000 binds port 4464 (70000 &
// 0xFFFF) with no error, and a configured -1 becomes UINT_MAX and binds 65535.
// The TCP accept helper (serveraccepthelper.h:130) and the client
// (clientstate.cpp) already reject ports outside 1024..65535; the websocket
// listener was simply never given the matching guard.
//
// Fix
// ---
//     if (serverPort < 1024 || serverPort > 65535)
//         throw ServerException(__FILE__, __LINE__, ERR_SOCK_INVALID_PORT, 0);
//
// This test models both listen() paths, shows which ports slip through the
// buggy version, and checks the real source file so the regression cannot
// silently return.
//
// Compile:
//   g++ -std=c++23 -Wall -Wextra -o /tmp/test_websocket_port_upper_bound_bug test_websocket_port_upper_bound_bug.cpp
//   /tmp/test_websocket_port_upper_bound_bug

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

static_assert(sizeof(unsigned short) == 2, "test assumes a 16-bit unsigned short");

static bool listenAttempt(unsigned configured, bool fixed, unsigned short &boundPort)
{
    if (fixed && (configured < 1024 || configured > 65535))
        return false;
    boundPort = static_cast<unsigned short>(configured);
    return true;
}

static std::string findSourceFile()
{
    const char *candidates[] = {
        "src/net/serveracceptwebhelper.cpp",
        "../src/net/serveracceptwebhelper.cpp",
        "../../src/net/serveracceptwebhelper.cpp",
    };
    for (const char *path : candidates) {
        std::ifstream f(path);
        if (f.good())
            return path;
    }
    return "";
}

struct Case {
    unsigned configured;
    unsigned short expected;
    bool shouldListen;
    const char *note;
};

int main()
{
    const Case cases[] = {
        { 1024,  1024,  true,  "lowest legal port" },
        { 7233,  7233,  true,  "shipped default" },
        { 65535, 65535, true,  "highest legal port" },
        { 0,     0,     false, "port zero" },
        { 80,    80,    false, "privileged port" },
        { 65536, 0,     false, "first out-of-range port (wraps to 0)" },
        { 70000, 4464,  false, "70000 & 0xFFFF == 4464" },
        { 4294967295u, 65535, false, "-1 read as int, narrowed to UINT_MAX" },
    };

    int failures = 0;
    bool reproduced = false;
    std::printf("Websocket port bounds regression test\n\n");

    for (const Case &c : cases) {
        unsigned short buggyBound = 0;
        unsigned short fixedBound = 0;
        const bool buggyListen = listenAttempt(c.configured, false, buggyBound);
        const bool fixedListen = listenAttempt(c.configured, true, fixedBound);

        std::printf("configured=%10u expected=%5u shouldListen=%-5s | buggy: listen=%-5s port=%5u | fixed: listen=%-5s port=%5u  (%s)\n",
                    c.configured, c.expected, c.shouldListen ? "yes" : "no",
                    buggyListen ? "yes" : "no", buggyBound,
                    fixedListen ? "yes" : "no", fixedBound, c.note);

        if (fixedListen != c.shouldListen) {
            std::printf("  FAIL: fixed guard accepted/rejected the wrong port\n");
            ++failures;
        }
        if (fixedListen && fixedBound != c.expected) {
            std::printf("  FAIL: fixed path bound to the wrong port\n");
            ++failures;
        }
        if (!c.shouldListen && buggyListen && buggyBound != c.configured)
            reproduced = true;
    }

    if (reproduced) {
        std::printf("\n  PASS: buggy path silently bound an illegal port (bug reproduced)\n");
    } else {
        std::printf("\n  FAIL: test does not reproduce the buggy narrowing\n");
        ++failures;
    }

    std::printf("\nSource checks\n");
    const std::string path = findSourceFile();
    if (path.empty()) {
        std::printf("FAIL: serveracceptwebhelper.cpp not found\n");
        ++failures;
    } else {
        std::ifstream f(path);
        const std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        const std::string good = "if (serverPort < 1024 || serverPort > 65535)";
        if (src.find(good) != std::string::npos) {
            std::printf("  PASS: upper/lower bound guard found in %s\n", path.c_str());
        } else {
            std::printf("  FAIL: '%s' NOT found in %s\n", good.c_str(), path.c_str());
            ++failures;
        }
        const std::string badTls = "m_webSocketTlsServer->listen(serverPort);";
        const std::string badPlain = "m_webSocketServer->listen(serverPort);";
        if (src.find(badTls) == std::string::npos && src.find(badPlain) == std::string::npos) {
            std::printf("  PASS: unguarded listen(serverPort) calls are gone\n");
        } else {
            std::printf("  FAIL: unguarded listen(serverPort) call still present in %s\n", path.c_str());
            ++failures;
        }
        if (src.find("ERR_SOCK_INVALID_PORT") != std::string::npos) {
            std::printf("  PASS: invalid-port exception used\n");
        } else {
            std::printf("  FAIL: ERR_SOCK_INVALID_PORT not used in %s\n", path.c_str());
            ++failures;
        }
    }

    std::printf("\n");
    if (failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("TESTS FAILED: %d failures\n", failures);
    return 1;
}
