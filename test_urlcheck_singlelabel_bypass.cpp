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

// Regression test: UrlCheck whitelist bypass via a single extra host label.
//
// Background
// ----------
// UrlCheck::run() flags a message when a whitespace-delimited token contains a
// blocked URL pattern (defaults: "http://", ".com", ".net", ".org", ".de"),
// unless that token is recognised as one of the whitelisted sites. The shipped
// exceptions (cleanerconfig.cpp:203-205) are:
//
//   "http://www.esl.", "https://www.pokerth.net", "pokerth.net"
//
// Commit 1813e91 fixed a substring bypass by requiring an exception to be
// domain-anchored in the token and by accepting only a single trailing
// TLD-like label after it. That "single label" allowance is needed for the
// prefix exception "http://www.esl.", which ends with a dot and expects a TLD
// (e.g. "com", "de") to complete the host.
//
// The bug
// -------
// The single-label allowance is applied to *every* exception, including the
// two that already end in a TLD ("pokerth.net" and "https://www.pokerth.net").
// For those, an extra ".LABEL" is not a TLD completing the whitelisted host --
// it is a different host. Because the suffix check in
// isWhitelistedToken() (src/chatcleaner/urlcheck.cpp:60-79) accepts any single
// alphanumeric label after the exception, all of these are wrongly allowed:
//
//   "pokerth.net.xyz"             -> ".xyz" accepted as a TLD-like label
//   "pokerth.net.co"              -> distinct ccTLD host
//   "https://www.pokerth.net.io"  -> distinct host, still matches ".net"
//   "pokerth.netxyz"              -> no-dot form is accepted too
//
// Each token still contains ".net", so without the faulty whitelist match it
// would be flagged. An attacker can therefore post hosts of the form
// pokerth.net.<label> unfiltered.
//
// The fix
// -------
// Accept a single trailing TLD-like label only when the matched exception ends
// with a dot (an incomplete host such as "http://www.esl."). For an exception
// that already ends in a TLD, any non-empty suffix that is not a path/query
// ("/", "?", "#") means a different host and must be rejected.
//
// This standalone test models both the current (buggy) and the fixed
// algorithms with std::string and the shipped default lists. It asserts the
// bypass is reproducible with the current logic and that the fixed logic flags
// the bypass URLs while continuing to allow the legitimate whitelisted ones.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_urlcheck_singlelabel_bypass test_urlcheck_singlelabel_bypass.cpp
//   /tmp/test_urlcheck_singlelabel_bypass

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

// True when the exception occurrence in the token starts at a domain boundary.
static bool domainBoundary(const string &token, size_t exPos)
{
    const string prefix = token.substr(0, exPos);
    return prefix.empty()
        || prefix.back() == '.'
        || prefix == "http://"
        || prefix == "https://";
}

// Current suffix rule: any single TLD-like label is accepted, for every
// exception. This is a verbatim model of urlcheck.cpp:60-79 before the fix.
static bool validSuffixCurrent(const string &suffix)
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

// Fixed suffix rule: a single TLD-like label is only meaningful for an
// exception ending in '.' (an incomplete host like "http://www.esl."). For an
// exception that already ends in a TLD, any other non-path suffix is a
// different host and must be rejected.
static bool validSuffixFixed(const string &suffix, bool exceptionEndsWithDot)
{
    if (suffix.empty()) {
        return true;
    }
    if (suffix[0] == '/' || suffix[0] == '?' || suffix[0] == '#') {
        return true;
    }
    if (!exceptionEndsWithDot) {
        return false;
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

static bool isWhitelistedToken(const string &token,
                               const vector<string> &exceptions,
                               bool fixed)
{
    for (const string &exception : exceptions) {
        if (exception.empty()) {
            continue;
        }
        const bool endsWithDot = !exception.empty() && exception.back() == '.';
        size_t exPos = token.find(exception);
        while (exPos != string::npos) {
            const string suffix = token.substr(exPos + exception.size());
            const bool valid = fixed
                ? validSuffixFixed(suffix, endsWithDot)
                : validSuffixCurrent(suffix);
            if (domainBoundary(token, exPos) && valid) {
                return true;
            }
            exPos = token.find(exception, exPos + 1);
        }
    }
    return false;
}

static bool runUrlCheck(string msg, const vector<string> &urls,
                        const vector<string> &exceptions, bool fixed)
{
    msg = toLower(msg);
    for (const string &token : splitWhitespace(msg)) {
        if (isWhitelistedToken(token, exceptions, fixed)) {
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
    bool singleLabelBypass;
};

static const vector<TestCase> kCases = {
    // New: a single extra label after an exception that already has a TLD.
    // These hosts are NOT whitelisted and must be flagged.
    {"pokerth.net.xyz", true, true},
    {"pokerth.net.co", true, true},
    {"pokerth.net.io", true, true},
    {"https://www.pokerth.net.xyz", true, true},
    {"http://pokerth.net.tk", true, true},
    {"pokerth.netxyz", true, true},
    {"x pokerth.net.io y", true, true},
    // Previously-covered bypasses must remain flagged.
    {"pokerth.net.evil.com", true, false},
    {"http://pokerth.net.attacker.com", true, false},
    {"https://pokerth.net.phish.org/path", true, false},
    {"evilpokerth.net", true, false},
    {"notpokerth.net.com", true, false},
    {"www.pokerth.net.evil.org", true, false},
    {"x pokerth.net.attacker.de y", true, false},
    {"evil.com/pokerth.net", true, false},
    // Cross-contamination must still be flagged.
    {"visit pokerth.net and www.spamcasino.com", true, false},
    {"pokerth.net and evil.net", true, false},
    {"http://evil.com", true, false},
    {"www.spamcasino.com", true, false},
    {"www.spam.org", true, false},
    // Legitimate whitelisted URLs must still pass.
    {"pokerth.net", false, false},
    {"https://www.pokerth.net", false, false},
    {"http://www.pokerth.net", false, false},
    {"http://pokerth.net", false, false},
    {"http://www.esl.com", false, false},
    {"http://www.esl.de", false, false},
    {"http://www.pokerth.net/forums", false, false},
    {"http://pokerth.net/path?x=1", false, false},
    {"PokerTH.NET is great", false, false},
    {"pokerth.net http://www.esl.de", false, false},
    // No URL at all.
    {"hello world, no links here", false, false},
    {"", false, false},
};

int main()
{
    int failures = 0;
    int reproduced = 0;
    int bypassReproduced = 0;
    int bypassTotal = 0;

    printf("Testing UrlCheck single-label whitelist bypass:\n\n");
    printf("%-32s %8s %8s %8s\n", "message", "expected", "current", "fixed");

    for (const TestCase &tc : kCases) {
        const bool buggy = runUrlCheck(tc.msg, kUrlStrings, kUrlExceptions, false);
        const bool fixed = runUrlCheck(tc.msg, kUrlStrings, kUrlExceptions, true);

        if (buggy != tc.expected) {
            ++reproduced;
            if (tc.singleLabelBypass) {
                ++bypassReproduced;
            }
        }
        if (tc.singleLabelBypass) {
            ++bypassTotal;
        }
        if (fixed != tc.expected) {
            ++failures;
            printf("  ^ FIXED MISMATCH\n");
        }
        printf("%-32s %8s %8s %8s%s\n",
               tc.msg,
               tc.expected ? "flag" : "allow",
               buggy ? "flag" : "allow",
               fixed ? "flag" : "allow",
               tc.singleLabelBypass ? "  <- bypass" : "");
    }

    printf("\nSingle-label bypass cases reproduced by current logic: %d/%d\n",
           bypassReproduced, bypassTotal);
    printf("Total divergences from expected in current logic: %d/%zu\n",
           reproduced, kCases.size());
    if (bypassReproduced != bypassTotal) {
        printf("FAIL: not all single-label bypass cases reproduce\n");
        ++failures;
    }
    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    printf("\nTEST PASSED: single-label whitelist bypass reproduced and fix verified\n");
    return 0;
}
