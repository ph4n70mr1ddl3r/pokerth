/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2013 Felix Hammer, Florian Thauer, Lothar May          *
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

// Regression test: irc_notify_player_list() drops the last nickname (and all
// nicknames when there is only one).
//
// Background
// ----------
// irc_notify_player_list() (src/net/ircthread.cpp:142) is the IRC event
// callback for RPL_NAMREPLY. It receives params[3], a whitespace-separated
// list of nicknames with NO trailing whitespace, and must signal one
// SignalIrcPlayerJoined() per nickname.
//
// The bug
// -------
// The current code uses a "read-ahead then loop while !fail && !eof" pattern:
//
//     istringstream input(players);
//     string name;
//     input >> name;                       // read the first token
//     while (!input.fail() && !input.eof()) {
//         SignalIrcPlayerJoined(name);     // process the PREVIOUS token
//         input >> name;                   // read the next token
//     }
//
// operator>>(istream&, string&) sets eofbit when it consumes the final token
// of the string. Consequently:
//
//   * "alice bob carol": reading "carol" sets eofbit, so the loop guard fails
//     on the next iteration and "carol" is never signalled (only alice/bob).
//   * "alice" (a single nick, the common NAMES case): the initial extraction
//     already sets eofbit, so the loop body never runs and NO nick is reported.
//
// This is the exact class of "operator>> + eof() in the loop condition" bug
// that should always be written as "while (stream >> value)".
//
// The fix
// -------
// Drive the loop directly off the extraction, which only evaluates true when
// a token was actually read:
//
//     istringstream input(players);
//     string name;
//     while (input >> name) {
//         SignalIrcPlayerJoined(name);
//     }
//
// This standalone test models both loops with std::istringstream, asserts the
// false drop is reproducible, and verifies the fix reports every nickname. It
// needs no Qt, Boost, libircclient or protobuf headers.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_irc_notify_player_list_bug test_irc_notify_player_list_bug.cpp
//   /tmp/test_irc_notify_player_list_bug

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> parseBuggy(const std::string& players)
{
    std::vector<std::string> joined;
    std::istringstream input(players);
    std::string name;
    input >> name;
    while (!input.fail() && !input.eof()) {
        joined.push_back(name);
        input >> name;
    }
    return joined;
}

std::vector<std::string> parseFixed(const std::string& players)
{
    std::vector<std::string> joined;
    std::istringstream input(players);
    std::string name;
    while (input >> name) {
        joined.push_back(name);
    }
    return joined;
}

int failures = 0;

void check(const char* label, bool ok)
{
    std::printf("  %-76s %s\n", label, ok ? "PASS" : "FAIL");
    if (!ok)
        ++failures;
}

} // namespace

int main()
{
    std::printf("Testing irc_notify_player_list() last-nickname drop:\n\n");

    std::printf("1) Multi-nick NAMES reply: the final nickname is dropped today.\n");
    {
        const auto buggy = parseBuggy("alice bob carol");
        const auto fixed = parseFixed("alice bob carol");
        const std::vector<std::string> expected = {"alice", "bob", "carol"};
        check("buggy drops 'carol' (only alice, bob)",
              buggy == std::vector<std::string>({"alice", "bob"}));
        check("fixed reports all three nicknames", fixed == expected);
    }

    std::printf("\n2) Single-nick NAMES reply: no nickname is reported today.\n");
    {
        const auto buggy = parseBuggy("alice");
        const auto fixed = parseFixed("alice");
        check("buggy reports nobody", buggy.empty());
        check("fixed reports the lone nickname", fixed == std::vector<std::string>({"alice"}));
    }

    std::printf("\n3) Two-nick NAMES reply: second nickname is dropped today.\n");
    {
        const auto buggy = parseBuggy("alice bob");
        const auto fixed = parseFixed("alice bob");
        check("buggy drops 'bob'", buggy == std::vector<std::string>({"alice"}));
        check("fixed reports both nicknames",
              fixed == std::vector<std::string>({"alice", "bob"}));
    }

    std::printf("\n4) Edge cases: empty and whitespace-only replies.\n");
    check("empty reply -> no nicknames in both",
          parseBuggy("").empty() && parseFixed("").empty());
    check("extra whitespace handled identically by the fix",
          parseFixed("  alice   bob\tcarol  ") == std::vector<std::string>({"alice", "bob", "carol"}));

    if (failures) {
        std::printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    std::printf("\nTEST PASSED: IRC last-nickname drop bug reproduced and fix verified\n");
    return 0;
}
