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

// Regression test: UrlCheck flags a whitelisted URL followed by ordinary
// sentence punctuation as URL spam.
//
// Background
// ----------
// UrlCheck::run() (src/chatcleaner/urlcheck.cpp) flags a message when a
// whitespace-delimited token contains a blocked URL pattern (shipped defaults:
// "http://", ".com", ".net", ".org", ".de"), unless the token is recognised as
// a whitelisted site by isWhitelistedToken(). The shipped exceptions
// (cleanerconfig.cpp:202-205) are:
//
//   "http://www.esl.", "https://www.pokerth.net", "pokerth.net"
//
// The bug
// -------
// In isWhitelistedToken() (urlcheck.cpp:61-89) the text following a matched
// exception (the "suffix") is validated. A suffix starting with a path
// delimiter ('/', '?', '#') is accepted. For a prefix exception ending in '.'
// ("http://www.esl.") a single TLD label is accepted. For every other
// exception (notably "pokerth.net") ANY other suffix is rejected:
//
//   else { validSuffix = false; }
//
// Sentence punctuation is not a path delimiter, so a perfectly ordinary
// message such as "visit pokerth.net." produces the token "pokerth.net.":
//   - exception "pokerth.net" matches at a domain boundary,
//   - suffix "." is rejected by the else branch,
//   - the token is therefore not whitelisted,
//   - token.contains(".net") is true -> run() returns true -> URL offence.
//
// The same false positive hits "pokerth.net,", "https://www.pokerth.net."
// and "http://www.esl.com." (the dot-ending branch also rejects a trailing
// '.'). Players get a URL-spam warning simply for punctuating a sentence that
// mentions the project's own site.
//
// The fix
// -------
// Accept a suffix that consists solely of sentence punctuation (.,;:!?)]}'")
// after a whitelisted exception. Host-like continuations (letters/digits) stay
// rejected, so all previously fixed bypasses ("pokerth.net.xyz",
// "http://www.esl.com.evil.com", ...) remain blocked.
//
// This standalone test models the current (buggy) and fixed suffix rules with
// std::string and the shipped default lists. It asserts the false positive is
// reproducible, the fix removes it, and spam/bypass/cross-token behavior is
// preserved.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_urlcheck_trailing_punctuation_bug test_urlcheck_trailing_punctuation_bug.cpp
//   /tmp/test_urlcheck_trailing_punctuation_bug

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

static bool isPathDelimiter(char c)
{
    return c == '/' || c == '?' || c == '#';
}

static bool isSentencePunctuation(char c)
{
    return c == '.' || c == ',' || c == ';' || c == ':' || c == '!' ||
           c == '?' || c == ')' || c == ']' || c == '}' || c == '\'' ||
           c == '"';
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

static bool domainBoundary(const string &token, size_t exPos)
{
    const string prefix = token.substr(0, exPos);
    return prefix.empty() || prefix.back() == '.' || prefix == "http://" ||
           prefix == "https://";
}

// Current (buggy) rule, verbatim from urlcheck.cpp:61-89. The dot-ending
// branch already accepts a single TLD label followed by a path delimiter
// (an earlier fix), but rejects trailing sentence punctuation.
static bool validSuffixCurrent(const string &suffix, bool exceptionEndsWithDot)
{
    if (suffix.empty()) {
        return true;
    }
    if (isPathDelimiter(suffix[0])) {
        return true;
    }
    if (exceptionEndsWithDot) {
        const size_t labelStart = (suffix[0] == '.') ? 1 : 0;
        size_t i = labelStart;
        while (i < suffix.size() && isAlnum(suffix[i])) {
            ++i;
        }
        if (i == labelStart) {
            return false;
        }
        if (i < suffix.size() && !isPathDelimiter(suffix[i])) {
            return false;
        }
        return true;
    }
    return false;
}

// Fixed rule: tolerate trailing sentence punctuation in both branches, while
// still rejecting any host-like (alphanumeric) continuation.
static bool validSuffixFixed(const string &suffix, bool exceptionEndsWithDot)
{
    if (suffix.empty()) {
        return true;
    }
    if (isPathDelimiter(suffix[0])) {
        return true;
    }
    if (exceptionEndsWithDot) {
        const size_t labelStart = (suffix[0] == '.') ? 1 : 0;
        size_t i = labelStart;
        while (i < suffix.size() && isAlnum(suffix[i])) {
            ++i;
        }
        if (i == labelStart) {
            return false;
        }
        for (size_t j = i; j < suffix.size(); ++j) {
            if (isPathDelimiter(suffix[j])) {
                break;
            }
            if (!isSentencePunctuation(suffix[j])) {
                return false;
            }
        }
        return true;
    }
    for (size_t j = 0; j < suffix.size(); ++j) {
        if (!isSentencePunctuation(suffix[j])) {
            return false;
        }
    }
    return true;
}

static bool isWhitelistedToken(const string &token,
                               const vector<string> &exceptions, bool fixed)
{
    for (const string &exception : exceptions) {
        if (exception.empty()) {
            continue;
        }
        const bool endsWithDot = exception.back() == '.';
        size_t exPos = token.find(exception);
        while (exPos != string::npos) {
            const string suffix = token.substr(exPos + exception.size());
            const bool valid = fixed ? validSuffixFixed(suffix, endsWithDot)
                                     : validSuffixCurrent(suffix, endsWithDot);
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
    bool falsePositive;
};

static const vector<TestCase> kCases = {
    // Whitelisted URLs followed by ordinary sentence punctuation: currently
    // flagged (false positive), must be allowed after the fix.
    {"pokerth.net.", false, true},
    {"pokerth.net,", false, true},
    {"pokerth.net!", false, true},
    {"pokerth.net;", false, true},
    {"pokerth.net...", false, true},
    {"visit pokerth.net.", false, true},
    {"(see pokerth.net)", false, true},
    {"www.pokerth.net.", false, true},
    {"https://www.pokerth.net.", false, true},
    {"http://www.esl.com.", false, true},
    {"PokerTH.NET.", false, true},
    // Host-like continuations still introduce a different host: must stay
    // flagged even when trailing punctuation is present.
    {"pokerth.net.xyz", true, false},
    {"pokerth.net.xyz.", true, false},
    {"pokerth.net.co", true, false},
    {"pokerth.net.evil.com", true, false},
    {"https://www.pokerth.net.io", true, false},
    {"http://www.esl.com.evil.com", true, false},
    {"http://www.esl.com.de", true, false},
    // Previously covered bypasses and cross-token contamination stay flagged.
    {"evilpokerth.net", true, false},
    {"http://evil.com", true, false},
    {"www.spam.org", true, false},
    {"pokerth.net and www.spamcasino.com", true, false},
    {"pokerth.net., http://evil.com", true, false},
    // Legitimate whitelisted URLs remain allowed.
    {"pokerth.net", false, false},
    {"https://www.pokerth.net", false, false},
    {"http://www.esl.com", false, false},
    {"http://www.esl.com/de/index.php", false, false},
    {"http://www.pokerth.net/forums/showthread.php?t=1", false, false},
    {"hello world, no links here", false, false},
    {"", false, false},
};

int main()
{
    int failures = 0;
    int reproduced = 0;
    int falsePositiveTotal = 0;

    printf("Testing UrlCheck whitelisted-URL trailing-punctuation false positive:\n\n");
    printf("%-48s %8s %8s %8s\n", "message", "expected", "current", "fixed");

    for (const TestCase &tc : kCases) {
        const bool current = runUrlCheck(tc.msg, kUrlStrings, kUrlExceptions, false);
        const bool fixed = runUrlCheck(tc.msg, kUrlStrings, kUrlExceptions, true);

        if (tc.falsePositive) {
            ++falsePositiveTotal;
            if (current != tc.expected) {
                ++reproduced;
            }
        }
        if (fixed != tc.expected) {
            ++failures;
            printf("  ^ FIXED MISMATCH\n");
        }
        printf("%-48s %8s %8s %8s%s\n",
               tc.msg,
               tc.expected ? "flag" : "allow",
               current ? "flag" : "allow",
               fixed ? "flag" : "allow",
               tc.falsePositive ? "  <- false positive" : "");
    }

    printf("\nFalse positives reproduced by current logic: %d/%d\n",
           reproduced, falsePositiveTotal);
    if (reproduced != falsePositiveTotal) {
        printf("FAIL: not all false-positive cases reproduce\n");
        ++failures;
    }
    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    printf("\nTEST PASSED: false positive reproduced and fix verified\n");
    return 0;
}
