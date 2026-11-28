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

// Regression test: MAX_CHAT_TEXT_SIZE off-by-one rejected a legal 128-char chat.
//
// Bug
// ---
// MAX_CHAT_TEXT_SIZE is 128 (src/net/netpacket.h:50) and the wire validator
// accepts chattext lengths of 1..128 inclusive:
//
//   netpacketvalidator.cpp: VALIDATE_STRING_SIZE(msg.chattext(), 1, 128)
//   #define VALIDATE_STRING_SIZE(s, lo, hi) ((s).size() >= (lo) && (s).size() <= (hi))
//
// But the lobby immediately rejected a 128-character message:
//
//   serverlobbythread.cpp: if (chatMsg.size() >= MAX_CHAT_TEXT_SIZE) { reject; }
//
// So a client message that passed validation got a ChatRejectMessage echo
// instead of being broadcast. The IRC admin "chat"/"msg" commands had the
// mirror-image bug, rejecting an exactly-128-character command:
//
//   serveradminbot.cpp: if (!chat.empty() && chat.size() < MAX_CHAT_TEXT_SIZE)
//
// Fix
// ---
//   serverlobbythread.cpp: if (chatMsg.size() > MAX_CHAT_TEXT_SIZE) { reject; }
//   serveradminbot.cpp:    ... chat.size() <= MAX_CHAT_TEXT_SIZE ...
//   serveradminbot.cpp:    ... message.size() <= MAX_CHAT_TEXT_SIZE ...
//
// This test models the validator and all three guards, shows which boundary
// values slip through the buggy versions, and checks the real source files so
// the regression cannot silently return.
//
// Compile:
//   g++ -std=c++23 -Wall -Wextra -o /tmp/test_chat_length_boundary_bug test_chat_length_boundary_bug.cpp
//   /tmp/test_chat_length_boundary_bug

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

static const int MAX_CHAT_TEXT_SIZE = 128;

static bool validatorAccepts(int length)
{
    return length >= 1 && length <= MAX_CHAT_TEXT_SIZE;
}

static bool lobbyAccepts(int length, bool fixed)
{
    if (length == 0)
        return false;
    // Buggy guard rejects >= 128; fixed guard rejects > 128.
    const bool tooLong = fixed ? (length > MAX_CHAT_TEXT_SIZE)
                               : (length >= MAX_CHAT_TEXT_SIZE);
    return !tooLong;
}

static bool adminAccepts(int length, bool fixed)
{
    if (length == 0)
        return false;
    // Buggy guard required < 128; fixed guard allows <= 128.
    return fixed ? (length <= MAX_CHAT_TEXT_SIZE)
                 : (length < MAX_CHAT_TEXT_SIZE);
}

static bool fileContains(const char *path, const std::string &needle)
{
    std::ifstream f(path);
    if (!f.good())
        return false;
    const std::string src((std::istreambuf_iterator<char>(f)),
                          std::istreambuf_iterator<char>());
    return src.find(needle) != std::string::npos;
}

struct Case {
    int length;
    const char *note;
};

int main()
{
    const Case cases[] = {
        { 0,   "empty message" },
        { 1,   "one character" },
        { 127, "last unambiguously accepted length" },
        { 128, "legal maximum - the boundary under test" },
        { 129, "first illegal length" },
        { 256, "clearly illegal" },
    };

    int failures = 0;
    bool reproducedLobby = false;
    bool reproducedAdmin = false;
    std::printf("Chat length boundary regression test (MAX_CHAT_TEXT_SIZE=%d)\n\n",
                MAX_CHAT_TEXT_SIZE);

    for (const Case &c : cases) {
        const bool legal = validatorAccepts(c.length);
        const bool buggyLobby = lobbyAccepts(c.length, false);
        const bool fixedLobby = lobbyAccepts(c.length, true);
        const bool buggyAdmin = adminAccepts(c.length, false);
        const bool fixedAdmin = adminAccepts(c.length, true);

        std::printf("len=%-4d validator=%-5s | lobby buggy=%-5s fixed=%-5s | admin buggy=%-5s fixed=%-5s  (%s)\n",
                    c.length,
                    legal ? "yes" : "no",
                    buggyLobby ? "yes" : "no", fixedLobby ? "yes" : "no",
                    buggyAdmin ? "yes" : "no", fixedAdmin ? "yes" : "no",
                    c.note);

        if (fixedLobby != legal) {
            std::printf("  FAIL: fixed lobby guard disagrees with the wire validator\n");
            ++failures;
        }
        if (fixedAdmin != legal) {
            std::printf("  FAIL: fixed admin guard disagrees with the wire validator\n");
            ++failures;
        }
        if (!legal && !buggyLobby) {
            std::printf("  NOTE: buggy lobby already rejects this illegal length\n");
        }
        if (legal && !buggyLobby)
            reproducedLobby = true;
        if (legal && !buggyAdmin)
            reproducedAdmin = true;
    }

    if (reproducedLobby) {
        std::printf("\n  PASS: buggy lobby guard rejected a legal message (bug reproduced)\n");
    } else {
        std::printf("\n  FAIL: test does not reproduce the buggy lobby rejection\n");
        ++failures;
    }
    if (reproducedAdmin) {
        std::printf("  PASS: buggy admin guard rejected a legal command (bug reproduced)\n");
    } else {
        std::printf("  FAIL: test does not reproduce the buggy admin rejection\n");
        ++failures;
    }

    std::printf("\nSource checks\n");
    struct SrcCase {
        const char *path;
        const char *good;
        const char *bad;
    };
    const SrcCase srcCases[] = {
        { "src/net/serverlobbythread.cpp",
          "if (chatMsg.size() > MAX_CHAT_TEXT_SIZE)",
          "if (chatMsg.size() >= MAX_CHAT_TEXT_SIZE)" },
        { "src/net/serveradminbot.cpp",
          "chat.size() <= MAX_CHAT_TEXT_SIZE",
          "chat.size() < MAX_CHAT_TEXT_SIZE" },
        { "src/net/serveradminbot.cpp",
          "message.size() <= MAX_CHAT_TEXT_SIZE",
          "message.size() < MAX_CHAT_TEXT_SIZE" },
    };

    for (const SrcCase &s : srcCases) {
        if (fileContains(s.path, s.good)) {
            std::printf("  PASS: '%s' found in %s\n", s.good, s.path);
        } else {
            std::printf("  FAIL: '%s' NOT found in %s\n", s.good, s.path);
            ++failures;
        }
        if (!fileContains(s.path, s.bad)) {
            std::printf("  PASS: old buggy guard '%s' is gone from %s\n", s.bad, s.path);
        } else {
            std::printf("  FAIL: old buggy guard '%s' is still present in %s\n", s.bad, s.path);
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
