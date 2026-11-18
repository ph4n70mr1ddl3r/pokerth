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

// Regression test: BadWordCheck whitelist exception exempts every occurrence.
//
// Background
// ----------
// BadWordCheck::run() flags a message when it contains a bad word from
// BadWordsList, unless that word is covered by an exception from
// BadWordsExceptionList. With the shipped defaults (cleanerconfig.cpp:165,
// 186-192) "idiot" is a bad word and "all-in-idiot", "all-in idiot",
// "allin idiot", "all in idiot" and "idiots end" are exceptions: they allow
// the harmless phrase but an actual insult must still be caught.
//
// The bug
// -------
// The exception test is done at the granularity of the bad word, not the
// specific occurrence (src/chatcleaner/badwordcheck.cpp:48-60):
//
//   if (msg.contains(bw)) {
//       for (bwe : badWordsException)
//           if (bwe.contains(bw) && msg.contains(bwe))
//               thisWordHasException = true;
//       if (!thisWordHasException) badMessage = true;
//   }
//
// Because msg.contains(bwe) is tested against the WHOLE message, a single
// whitelisted phrase anywhere in the message exempts every occurrence of the
// bad word. Examples with the shipped defaults:
//
//   "all-in-idiot was a great play, but you are an idiot"
//       -> contains exception "all-in-idiot" -> every "idiot" exempted -> allowed
//   "the idiots end was rough but he is an idiot" -> allowed
//   "ALL-IN-IDIOT was funny, but you are an IDIOT" -> allowed
//
// This is the same cross-contamination class that was fixed for UrlCheck
// (commits 50b82e8, 1813e91) and for unrelated bad words in 2e76b9e; the
// per-occurrence gap was left behind.
//
// The fix
// -------
// Remove whitelisted exception phrases from a lowercased working copy first
// (replacing each with a space so text on either side cannot join into a new
// bad word), then scan the remainder for bad words. An exception now allows
// only its own occurrence of the bad word; a separate, genuine insult in the
// same message is still flagged.
//
// This standalone test models both the current (buggy) and the fixed
// algorithms with std::string and the shipped default lists. It asserts that
// the bypass is reproducible with the current logic and that the fixed logic
// flags the bypass messages while still allowing legitimate exceptions.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_badword_whitelist_bypass test_badword_whitelist_bypass.cpp
//   /tmp/test_badword_whitelist_bypass

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

using std::string;
using std::vector;

static const vector<string> kBadWords = {
    "arsch", "asshole", "bastard", "bitch", "cunt", " dick ", "drecksau",
    " fag ", "fagget", "fotze", "fucker", "fuck", " fuk ", "gay", "horny",
    "hure", "idiot", "mistgeburt", "missgeburt", "motherfucker", "nazi",
    "nigga", "nigger", "nutte", "ommak", "penis", "pussy", "schlampe",
    "schwanz", "sex", "shit", "sieg heil", "slut", "suck", "whore"};

static const vector<string> kExceptions = {
    "idiots end", "all-in-idiot", "all-in idiot", "allin idiot", "all in idiot"};

static string toLower(string s)
{
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

static bool contains(const string &haystack, const string &needle)
{
    return haystack.find(needle) != string::npos;
}

// Current BadWordCheck::run(): for a bad word found anywhere in the message,
// any exception containing that word and present anywhere in the message
// exempts every occurrence of the word.
static bool runBuggy(string msg, const vector<string> &badWords,
                     const vector<string> &exceptions)
{
    msg = toLower(msg);
    for (const string &bw : badWords) {
        if (!contains(msg, bw)) {
            continue;
        }
        bool thisWordHasException = false;
        for (const string &bwe : exceptions) {
            if (contains(bwe, bw) && contains(msg, bwe)) {
                thisWordHasException = true;
                break;
            }
        }
        if (!thisWordHasException) {
            return true;
        }
    }
    return false;
}

// Fixed BadWordCheck::run(): strip every exception occurrence from a working
// copy before scanning for bad words.
static bool runFixed(string msg, const vector<string> &badWords,
                     const vector<string> &exceptions)
{
    msg = toLower(msg);
    for (const string &exception : exceptions) {
        if (exception.empty()) {
            continue;
        }
        size_t exPos = msg.find(exception);
        while (exPos != string::npos) {
            msg.replace(exPos, exception.size(), " ");
            exPos = msg.find(exception, exPos + 1);
        }
    }
    for (const string &bw : badWords) {
        if (!bw.empty() && contains(msg, bw)) {
            return true;
        }
    }
    return false;
}

struct TestCase {
    const char *msg;
    bool expected;
};

static const vector<TestCase> kCases = {
    // Per-occurrence whitelist bypass: the genuine insult must be flagged.
    {"all-in-idiot was a great play but you are an idiot", true},
    {"the idiots end was rough but he is an idiot", true},
    {"all in idiot joking, but seriously you idiot", true},
    {"ALL-IN-IDIOT was funny, but you are an IDIOT", true},
    {"all-in-idiot all-in-idiot, you idiot", true},
    {"allin idiot aside, that was an idiot move", true},
    // A whitelisted phrase must not shield a different bad word.
    {"all-in-idiot you shit", true},
    // Plain insults (no exception) are still flagged.
    {"you idiot", true},
    {"what an idiot", true},
    {"you shit", true},
    // Legitimate whitelisted uses must still pass.
    {"that was an all-in-idiot move", false},
    {"idiots end", false},
    {"the allin idiot line", false},
    {"all in idiot was just banter", false},
    {"the idiots end of the game", false},
    // No bad word at all.
    {"good game everyone", false},
    {"", false},
};

int main()
{
    int failures = 0;
    int reproduced = 0;

    printf("Testing BadWordCheck per-occurrence whitelist bypass:\n\n");
    printf("%-48s %8s %8s %8s\n", "message", "expected", "current", "fixed");

    for (const TestCase &tc : kCases) {
        const bool buggy = runBuggy(tc.msg, kBadWords, kExceptions);
        const bool fixed = runFixed(tc.msg, kBadWords, kExceptions);

        if (buggy != tc.expected) {
            ++reproduced;
        }
        if (fixed != tc.expected) {
            ++failures;
            printf("  ^ FIXED MISMATCH\n");
        }
        printf("%-48s %8s %8s %8s\n",
               tc.msg,
               tc.expected ? "flag" : "allow",
               buggy ? "flag" : "allow",
               fixed ? "flag" : "allow");
    }

    printf("\nBypass cases reproduced by current logic: %d/%zu\n",
           reproduced, kCases.size());
    if (reproduced == 0) {
        printf("FAIL: current logic no longer diverges (bug not reproduced)\n");
        ++failures;
    }
    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    printf("\nTEST PASSED: whitelist bypass reproduced and fix verified\n");
    return 0;
}
