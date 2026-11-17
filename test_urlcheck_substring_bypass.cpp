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

// Regression test: UrlCheck whitelist exception matched as an arbitrary
// substring, with the entire containing token removed.
//
// Background
// ----------
// UrlCheck::run() flags a message when it contains a URL pattern from
// UrlStringsList (defaults: "http://", ".com", ".net", ".org", ".de"), unless
// the URL is one of the allowed sites in UrlExceptionStringsList (defaults:
// "http://www.esl.", "https://www.pokerth.net", "pokerth.net").
//
// The previous fix for the cross-contamination bypass (commit 50b82e8) changed
// run() to remove whitelisted URL tokens from a working copy before scanning
// for URL patterns. A token is the whitespace-delimited word containing the
// exception string; removing the whole token lets a prefix exception such as
// "http://www.esl." cover its trailing TLD.
//
// The bug
// -------
// The exception is located with msg.indexOf(exception), i.e. as a plain
// substring anywhere in the message, and then the WHOLE whitespace-delimited
// token containing it is erased (src/chatcleaner/urlcheck.cpp:54-64). Any URL
// that merely contains an exception string as a substring therefore has its
// entire token removed -- including the http:// and TLD parts that would
// otherwise flag it. Examples with the shipped defaults:
//
//   "pokerth.net.evil.com"            -> exception "pokerth.net" found,
//                                        whole token erased, message allowed
//   "http://pokerth.net.attacker.com" -> same, allowed
//   "evilpokerth.net"                 -> exception found mid-token, allowed
//
// The whitelist is meant to allow the *site* pokerth.net, not arbitrary hosts
// that happen to contain that text; these spam/phishing URLs must be flagged.
//
// The fix
// -------
// Match exceptions per token, anchored to a domain boundary: the exception may
// start at the beginning of the token, right after a '.', or right after an
// http(s):// scheme prefix. The text following the exception may only be empty
// or a single TLD-like label (".com", "com", ".de"); a second domain label or
// any path/punctuation means the token is not the whitelisted site. Only tokens
// that satisfy this rule are skipped; every other token is scanned for URL
// patterns as before.
//
// This standalone test models both the current (buggy) and the fixed
// algorithms with std::string and the shipped default lists. It asserts the
// bypass is reproducible with the current logic and that the fixed logic flags
// the bypass URLs while still allowing the legitimate whitelisted URLs.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_urlcheck_substring_bypass test_urlcheck_substring_bypass.cpp
//   /tmp/test_urlcheck_substring_bypass

#include <cctype>
#include <cstdio>
#include <sstream>
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

static bool isAlnum(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

static vector<string> splitWhitespace(const string &s)
{
    vector<string> tokens;
    std::istringstream in(s);
    string token;
    while (in >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Current UrlCheck::run(): locate an exception as a substring anywhere in the
// message and erase the entire whitespace-delimited token containing it.
static bool runBuggy(string msg, const vector<string> &urls,
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

// True when the exception occurrence in token starts at a domain boundary.
static bool domainBoundary(const string &token, size_t exPos)
{
    const string prefix = token.substr(0, exPos);
    return prefix.empty()
        || prefix.back() == '.'
        || prefix == "http://"
        || prefix == "https://";
}

// True when the text after the exception is empty, a single TLD-like label,
// or a path/query on the whitelisted host.
static bool validSuffix(const string &suffix)
{
    if (suffix.empty()) {
        return true;
    }
    if (suffix[0] == '/' || suffix[0] == '?' || suffix[0] == '#') {
        return true;
    }
    size_t i = (suffix[0] == '.') ? 1 : 0;
    if (i == suffix.size()) {
        return false;
    }
    for (; i < suffix.size(); ++i) {
        if (!isAlnum(suffix[i])) {
            return false;
        }
    }
    return true;
}

// A token is whitelisted only if an exception is domain-anchored within it and
// followed by at most a single TLD label.
static bool isWhitelistedToken(const string &token,
                               const vector<string> &exceptions)
{
    for (const string &exception : exceptions) {
        if (exception.empty()) {
            continue;
        }
        size_t exPos = token.find(exception);
        while (exPos != string::npos) {
            const string suffix = token.substr(exPos + exception.size());
            if (domainBoundary(token, exPos) && validSuffix(suffix)) {
                return true;
            }
            exPos = token.find(exception, exPos + 1);
        }
    }
    return false;
}

// Fixed UrlCheck::run(): skip whitelisted tokens, scan every other token for
// URL patterns.
static bool runFixed(string msg, const vector<string> &urls,
                     const vector<string> &exceptions)
{
    msg = toLower(msg);
    for (const string &token : splitWhitespace(msg)) {
        if (isWhitelistedToken(token, exceptions)) {
            continue;
        }
        for (const string &url : urls) {
            if (contains(token, url)) {
                return true;
            }
        }
    }
    return false;
}

struct TestCase {
    const char *msg;
    bool expected;
};

static const vector<TestCase> kCases = {
    // The substring/whole-token bypass: these must be flagged.
    {"pokerth.net.evil.com", true},
    {"http://pokerth.net.attacker.com", true},
    {"https://pokerth.net.phish.org/path", true},
    {"evilpokerth.net", true},
    {"notpokerth.net.com", true},
    {"www.pokerth.net.evil.org", true},
    {"x pokerth.net.attacker.de y", true},
    {"evil.com/pokerth.net", true},
    // Cross-contamination must still be flagged.
    {"visit pokerth.net and www.spamcasino.com", true},
    {"pokerth.net and evil.net", true},
    {"http://evil.com", true},
    {"www.spamcasino.com", true},
    {"www.spam.org", true},
    // Legitimate whitelisted URLs must still pass.
    {"pokerth.net", false},
    {"https://www.pokerth.net", false},
    {"http://www.pokerth.net", false},
    {"http://pokerth.net", false},
    {"http://www.esl.com", false},
    {"http://www.esl.de", false},
    {"http://www.pokerth.net/forums", false},
    {"http://pokerth.net/path?x=1", false},
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

    printf("Testing UrlCheck substring/whole-token whitelist bypass:\n\n");
    printf("%-40s %8s %8s %8s\n", "message", "expected", "current", "fixed");

    for (const TestCase &tc : kCases) {
        const bool buggy = runBuggy(tc.msg, kUrlStrings, kUrlExceptions);
        const bool fixed = runFixed(tc.msg, kUrlStrings, kUrlExceptions);

        if (buggy != tc.expected) {
            ++reproduced;
        }
        if (fixed != tc.expected) {
            ++failures;
            printf("  ^ FIXED MISMATCH\n");
        }
        printf("%-40s %8s %8s %8s\n",
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
