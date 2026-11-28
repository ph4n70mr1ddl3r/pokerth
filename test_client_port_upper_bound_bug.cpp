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

// Regression test: ClientStateInit::Enter() lacks an upper-bound port check.
//
// Bug
// ---
// src/net/clientstate.cpp, ClientStateInit::Enter():
//
//     if (context.GetServerPort() < 1024)
//         throw ClientException(..., ERR_SOCK_INVALID_PORT, 0);
//
// GetServerPort() returns unsigned (src/net/clientcontext.h:116), and the
// value comes straight from the user-editable config file:
//
//     session.cpp:196: myConfig->readConfigInt("InternetServerPort")
//       -> ClientThread::Init(..., unsigned serverPort, ...)
//       -> context.SetServerPort(serverPort)
//
// A hand-edited config with InternetServerPort=70000 passes this guard, so
// ClientStateStartResolve::Enter() tries to resolve service "70000" and the
// user gets a generic resolve failure instead of the intended "invalid port"
// error. A configured -1 becomes UINT_MAX and fails the same way. The server
// side (serveraccepthelper.h:129) already rejects ports above 65535; the
// client side was simply never given the matching upper bound.
//
// Fix
// ---
//     if (context.GetServerPort() < 1024 || context.GetServerPort() > 65535)
//         throw ClientException(..., ERR_SOCK_INVALID_PORT, 0);
//
// This test models both guards, shows which ports slip through the buggy one,
// and checks the real source file so the regression cannot silently return.
//
// Compile:
//   g++ -std=c++23 -Wall -Wextra -o /tmp/test_client_port_upper_bound_bug test_client_port_upper_bound_bug.cpp
//   /tmp/test_client_port_upper_bound_bug

#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>

static bool isPortAccepted(unsigned port, bool fixed)
{
    if (port < 1024)
        return false;
    if (fixed && port > 65535)
        return false;
    return true;
}

static std::string resolveService(unsigned port)
{
    std::ostringstream portStr;
    portStr << port;
    return portStr.str();
}

static std::string findSourceFile()
{
    const char *candidates[] = {
        "src/net/clientstate.cpp",
        "../src/net/clientstate.cpp",
        "../../src/net/clientstate.cpp",
    };
    for (const char *path : candidates) {
        std::ifstream f(path);
        if (f.good()) return path;
    }
    return "";
}

struct Case {
    unsigned configured;
    bool shouldAccept;
    const char *note;
};

int main()
{
    const Case cases[] = {
        { 1024,                              true,  "lowest legal port" },
        { 7234,                              true,  "shipped default" },
        { 65535,                             true,  "highest legal port" },
        { 65536,                             false, "first out-of-range port" },
        { 70000,                             false, "hand-edited config value" },
        { std::numeric_limits<unsigned>::max(), false, "configured -1 cast to unsigned" },
    };

    int failures = 0;
    std::printf("Client port upper-bound regression test\n\n");

    for (const Case &c : cases) {
        const bool buggy = isPortAccepted(c.configured, false);
        const bool fixed = isPortAccepted(c.configured, true);

        std::printf("configured=%10u shouldAccept=%-5s | buggy=%-5s service=%-10s | fixed=%-5s  (%s)\n",
                    c.configured, c.shouldAccept ? "yes" : "no",
                    buggy ? "yes" : "no", buggy ? resolveService(c.configured).c_str() : "-",
                    fixed ? "yes" : "no", c.note);

        if (fixed != c.shouldAccept) {
            std::printf("  FAIL: fixed guard accepted/rejected the wrong port\n");
            ++failures;
        }
        if (!c.shouldAccept && !buggy) {
            std::printf("  FAIL: test does not reproduce the buggy behavior\n");
            ++failures;
        }
    }

    const std::string path = findSourceFile();
    if (path.empty()) {
        std::printf("\nNOTE: clientstate.cpp not found; skipping source check\n");
    } else {
        std::ifstream f(path);
        const std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        const std::string good = "if (context.GetServerPort() < 1024 || context.GetServerPort() > 65535)";
        const std::string bad = "if (context.GetServerPort() < 1024)";

        std::printf("\nSource check (%s)\n", path.c_str());
        if (src.find(good) != std::string::npos) {
            std::printf("  PASS: upper-bound guard '%s' found\n", good.c_str());
        } else {
            std::printf("  FAIL: upper-bound guard '%s' NOT found\n", good.c_str());
            ++failures;
        }
        if (src.find(bad) == std::string::npos) {
            std::printf("  PASS: old lower-bound-only guard is gone\n");
        } else {
            std::printf("  FAIL: old lower-bound-only guard '%s' is still present\n", bad.c_str());
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
