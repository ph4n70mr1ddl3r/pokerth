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

// Regression test: UrlCheck whitelist exceptions disable the filter for the
// whole message instead of only for the URL they are meant to allow.
//
// Background
// ----------
// The chat cleaner flags a message when it contains a URL pattern from
// UrlStringsList (defaults: "http://", ".com", ".net", ".org", ".de"), unless
// the URL is one of the allowed sites in UrlExceptionStringsList (defaults:
// "http://www.esl.", "https://www.pokerth.net", "pokerth.net").
//
// The bug
// -------
// UrlCheck::run() (src/chatcleaner/urlcheck.cpp) iterates the URL patterns and,
// when one is found, scans the ENTIRE message for any exception string. Because
// that exception scan is independent of the pattern that matched, a single
// whitelisted token anywhere in the message clears the suspicion for every URL
// in the message. A spammer can append "pokerth.net" to any message full of
// spam links and pass the filter untouched.
//
// This is the same class of cross-contamination security bypass that was fixed
// for bad words in BadWordCheck::run() (commit 2e76b9e); UrlCheck was missed.
//
// The fix
// -------
// Remove whitelisted URL tokens from a working copy of the message BEFORE
// scanning for URL patterns. A token is the whitespace-delimited word that
// contains the exception string; removing the whole token lets a prefix
// exception such as "http://www.esl." also cover its trailing TLD.
//
// This standalone test models both the original (buggy) and the fixed
// algorithms with std::string, using the shipped default pattern/exception
// lists. It asserts the bypass is reproducible with the original logic and
// that the fixed logic gives the correct verdict for all cases: spam mixed with
// an allowed URL is flagged, while the allowed URLs themselves still pass.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_urlcheck_exception_bypass test_urlcheck_exception_bypass.cpp
//   /tmp/test_urlcheck_exception_bypass

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

using std::string;
using std::vector;

static const vector<string> kUrlStrings = {"http://", ".com", ".net", ".org", ".de"};
static const vector<string> kUrlExceptions = {
    "http://www.esl.", "https://www.pokerth.net", "pokerth.net"};

static string toLower(string s)
{
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

static bool contains(const string &haystack, const string &needle)
{
    return !needle.empty() && haystack.find(needle) != string::npos;
}

static bool isSpace(char c)
{
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

// Original UrlCheck::run(): the exception scan is applied to the whole
// message for every matched URL pattern.
static bool runBuggy(string msg, const vector<string> &urls,
                     const vector<string> &exceptions)
{
    msg = toLower(msg);
    for (const string &url : urls) {
        if (contains(msg, url)) {
            bool exception = false;
            for (const string &e : exceptions) {
                if (contains(msg, e)) {
                    exception = true;
                    break;
                }
            }
            if (!exception) {
                return true;
            }
        }
    }
    return false;
}

// Fixed UrlCheck::run(): strip whitelisted URL tokens first, then look for
// any remaining URL pattern.
static bool runFixed(string msg, const vector<string> &urls,
                     const vector<string> &exceptions)
{
    msg = toLower(msg);
    for (const string &exception : exceptions) {
        if (exception.empty()) {
            continue;
        }
        size_t exPos = msg.find(exception);
        while (exPos != string::npos) {
            size_t start = exPos;
            while (start > 0 && !isSpace(msg[start - 1])) {
                --start;
            }
            size_t end = exPos + exception.size();
            while (end < msg.size() && !isSpace(msg[end])) {
                ++end;
            }
            msg.erase(start, end - start);
            exPos = msg.find(exception, start);
        }
    }
    for (const string &url : urls) {
        if (contains(msg, url)) {
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
    // The bypass: spam URLs alongside one whitelisted token must be flagged.
    {"visit pokerth.net and www.spamcasino.com", true},
    {"pokerth.net and evil.net", true},
    {"http://evil.com", true},
    {"www.spamcasino.com", true},
    {"www.spam.org", true},
    // Whitelisted URLs must still pass.
    {"pokerth.net", false},
    {"https://www.pokerth.net", false},
    {"http://www.pokerth.net", false},
    {"http://www.esl.com", false},
    {"PokerTH.NET is great", false},
    {"pokerth.net http://www.esl.de", false},
    // No URL at all.
    {"hello world, no links here", false},
    {"", false},
};

int main()
{
    int failures = 0;
    int reproduced = 0;

    printf("Testing UrlCheck whitelist exception handling:\n\n");
    printf("%-45s %8s %8s %8s\n", "message", "expected", "buggy", "fixed");

    for (const TestCase &tc : kCases) {
        const bool buggy = runBuggy(tc.msg, kUrlStrings, kUrlExceptions);
        const bool fixed = runFixed(tc.msg, kUrlStrings, kUrlExceptions);

        if (buggy != tc.expected) {
            ++reproduced;
        }
        if (fixed != tc.expected) {
            ++failures;
        }
        printf("%-45s %8s %8s %8s\n",
               tc.msg,
               tc.expected ? "flag" : "allow",
               buggy ? "flag" : "allow",
               fixed ? "flag" : "allow");
    }

    printf("\nBypass cases reproduced by original logic: %d/%zu\n",
           reproduced, kCases.size());
    if (reproduced == 0) {
        printf("FAIL: original logic no longer diverges (bug not reproduced)\n");
        ++failures;
    }
    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    printf("\nTEST PASSED: whitelist bypass reproduced and fix verified\n");
    return 0;
}
