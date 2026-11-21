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

// Regression test: irc_auto_rename_nick() no-ops on a max-length nick.
//
// Background
// ----------
// SanitizeIrcNick() (src/net/ircthread.cpp:85) deliberately produces nicks up
// to IRC_MAX_NICK_LEN == 16 characters. On a nickname collision,
// irc_auto_rename_nick() (src/net/ircthread.cpp:108) first appends
// IRC_RENAME_ATTACH == "|Lobby" (6 chars). For any original nick longer than
// 10 characters that yields exactly a 16-character nick, e.g.
// "VeryLongPl" + "|Lobby" == "VeryLongPl|Lobby".
//
// The bug
// -------
// On the *next* collision the else branch at ircthread.cpp:121 tries to add a
// numeric suffix. Both append sites are guarded by:
//
//     if (context->nick.length() < IRC_MAX_NICK_LEN)
//         context->nick = context->nick + "1";   // or + "0" for a trailing '9'
//
// At length 16 the guard is false, so the nick is NOT changed at all. The very
// same nick is passed to irc_cmd_nick(), collides again, and the cycle repeats
// until renameTries reaches IRC_MAX_RENAME_TRIES (5) and the bot disconnects
// with irc_cmd_quit(). A perfectly valid alternative ("VeryLongPl|Lobb1") was
// available; the function just failed to make room for the suffix.
//
// Commit 03a18c9 introduced the `length() < IRC_MAX_NICK_LEN` guard to stop a
// numeric append from overflowing the buffer, but it forgot that the nick can
// already be at the maximum when the else branch is reached.
//
// The fix
// -------
// When at max length, truncate by one character before appending the suffix:
//
//     if (context->nick.length() >= IRC_MAX_NICK_LEN)
//         context->nick = context->nick.substr(0, IRC_MAX_NICK_LEN - 1);
//     context->nick = context->nick + "1";   // or + "0"
//
// This keeps every generated nick within 16 characters while guaranteeing the
// nick actually changes on every rename attempt.
//
// This standalone test models the exact nick-mangling state machine (no Qt,
// Boost or libircclient needed) and compares the buggy and fixed behaviour
// across repeated collisions, the trailing-'9' case, and short nicks.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o test_irc_rename_maxlen_noop_bug test_irc_rename_maxlen_noop_bug.cpp

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr int kMaxNickLen = 16;
const char kAttach[] = "|Lobby";

// Models one call to irc_auto_rename_nick()'s nick-mangling block.
// fixed == false reproduces the current source (append only if length < MAX);
// fixed == true models the truncate-then-append fix.
std::string renameOnce(std::string nick, bool fixed)
{
    if (nick.find(kAttach) == std::string::npos) {
        if (static_cast<int>(nick.length()) + (sizeof(kAttach) - 1) > kMaxNickLen)
            nick = nick.substr(0, kMaxNickLen - (sizeof(kAttach) - 1));
        nick += kAttach;
    } else {
        if (!nick.empty() && std::isdigit(static_cast<unsigned char>(nick.back()))) {
            if (nick.back() != '9') {
                nick.back() = static_cast<char>(nick.back() + 1);
            } else {
                if (fixed) {
                    if (static_cast<int>(nick.length()) >= kMaxNickLen)
                        nick = nick.substr(0, kMaxNickLen - 1);
                    nick += "0";
                } else {
                    if (static_cast<int>(nick.length()) < kMaxNickLen)
                        nick += "0";
                }
            }
        } else {
            if (fixed) {
                if (static_cast<int>(nick.length()) >= kMaxNickLen)
                    nick = nick.substr(0, kMaxNickLen - 1);
                nick += "1";
            } else {
                if (static_cast<int>(nick.length()) < kMaxNickLen)
                    nick += "1";
            }
        }
    }
    return nick;
}

bool endsWith(const std::string& s, const char* suffix)
{
    const std::string strSuffix(suffix);
    return s.size() >= strSuffix.size() &&
           s.compare(s.size() - strSuffix.size(), strSuffix.size(), strSuffix) == 0;
}

int failures = 0;

void check(const char* label, bool ok)
{
    std::printf("  %-72s %s\n", label, ok ? "PASS" : "FAIL");
    if (!ok)
        ++failures;
}

} // namespace

int main()
{
    std::printf("Regression test: IRC auto-rename no-op at IRC_MAX_NICK_LEN=%d\n\n",
                kMaxNickLen);

    const std::string longNick = "VeryLongPlayerNa"; // 16 chars, no "|Lobby"

    // 1) Reproduce: first collision fills the nick to exactly 16 characters.
    const std::string buggyFirst = renameOnce(longNick, false);
    check("first collision yields a 16-char nick ending in |Lobby",
          buggyFirst.size() == kMaxNickLen && endsWith(buggyFirst, kAttach));

    const std::string buggySecond = renameOnce(buggyFirst, false);
    check("buggy: second collision leaves the nick unchanged", buggySecond == buggyFirst);

    // 2) Fixed behaviour: first collision unchanged, second collision changes it.
    const std::string fixedFirst = renameOnce(longNick, true);
    check("fixed: first collision identical to buggy", fixedFirst == buggyFirst);
    const std::string fixedSecond = renameOnce(fixedFirst, true);
    check("fixed: second collision changes the nick", fixedSecond != fixedFirst);
    check("fixed: nick stays within 16 chars", fixedSecond.size() <= kMaxNickLen);

    // 3) Drive all five rename attempts under persistent collisions.
    std::vector<std::string> buggySeq{longNick};
    std::vector<std::string> fixedSeq{longNick};
    for (int i = 0; i < 5; ++i) {
        buggySeq.push_back(renameOnce(buggySeq.back(), false));
        fixedSeq.push_back(renameOnce(fixedSeq.back(), true));
    }

    bool buggyStalls = true;
    for (std::size_t i = 2; i < buggySeq.size(); ++i)
        if (buggySeq[i] != buggySeq[i - 1])
            buggyStalls = false;
    check("buggy: attempts 2..5 all resend the identical nick", buggyStalls);

    bool fixedAlwaysChanges = true;
    for (std::size_t i = 1; i < fixedSeq.size(); ++i)
        if (fixedSeq[i] == fixedSeq[i - 1])
            fixedAlwaysChanges = false;
    check("fixed: every attempt produces a new nick", fixedAlwaysChanges);

    bool allWithin = true;
    for (const auto& n : buggySeq)
        if (n.size() > static_cast<std::size_t>(kMaxNickLen))
            allWithin = false;
    for (const auto& n : fixedSeq)
        if (n.size() > static_cast<std::size_t>(kMaxNickLen))
            allWithin = false;
    check("both models stay within IRC_MAX_NICK_LEN (no overflow)", allWithin);

    // 4) Trailing '9' at max length hits the same dead end.
    const std::string nineNick = "abcdefghi|Lobby9"; // 9 + 6 + 1 == 16
    check("setup: trailing-'9' nick is exactly 16 chars", nineNick.size() == kMaxNickLen);
    check("buggy: trailing-'9' max-length nick unchanged", renameOnce(nineNick, false) == nineNick);
    const std::string nineFixed = renameOnce(nineNick, true);
    check("fixed: trailing-'9' max-length nick changes",
          nineFixed != nineNick && nineFixed.size() <= kMaxNickLen);

    // 5) Short nicks must behave identically in both models.
    const std::string shortNick = "Alice";
    check("short nick: attach path identical",
          renameOnce(shortNick, false) == "Alice|Lobby" &&
          renameOnce(shortNick, true) == "Alice|Lobby");
    const std::string shortBuggy = renameOnce(renameOnce(shortNick, false), false);
    const std::string shortFixed = renameOnce(renameOnce(shortNick, true), true);
    check("short nick: numeric suffix path identical",
          shortBuggy == "Alice|Lobby1" && shortFixed == shortBuggy);

    if (failures) {
        std::printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    std::printf("\nTEST PASSED: max-length rename no-op reproduced and fix verified\n");
    return 0;
}
