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

// Regression test: UrlCheck flags a whitelisted URL wrapped in matching
// opening punctuation (parentheses, brackets, braces, quotes) as URL spam.
//
// Background
// ----------
// UrlCheck::run() (src/chatcleaner/urlcheck.cpp) flags a message when a
// whitespace-delimited token contains a blocked URL pattern (shipped defaults:
// "http://", ".com", ".net", ".org", ".de"), unless the token is recognised as
// a whitelisted site by isWhitelistedToken(). The shipped exceptions
// (cleanerconfig.cpp:202-205) include "https://www.pokerth.net" and
// "pokerth.net".
//
// A token is whitelisted only when the exception is anchored to a domain
// boundary, i.e. the text BEFORE the exception (the "prefix") is empty, ends in
// '.', or is exactly "http://" / "https://".
//
// The bug
// -------
// Sentence punctuation on the right hand side of a whitelisted URL is already
// accepted (isSentencePunctuation() covers '.', ',', ..., and the CLOSING
// delimiters ')' ']' '}' '\'' '"'). There is no symmetric handling of the
// matching OPENING delimiters on the left hand side. For a single token such
// as "(pokerth.net)":
//   - exception "pokerth.net" matches at index 1,
//   - prefix is "(", which is not empty, does not end in '.', and is not an
//     http(s) scheme -> domainBoundary is false,
//   - the token is not whitelisted, token.contains(".net") is true,
//   - run() returns true -> URL-spam offence.
//
// So "pokerth.net)" is fine but "(pokerth.net)" is flagged, even though both
// are the same whitelisted host merely wrapped in punctuation.
//
// The fix
// -------
// Also accept a prefix made entirely of opening punctuation ('(' '[' '{'
// '\'' '"'), mirroring the closing characters already accepted on the suffix.
// These characters cannot occur inside a hostname, so embedded-substring
// bypasses ("evilpokerth.net") and host continuations ("pokerth.net.evil.com")
// remain rejected.
//
// This standalone test models the current (buggy) and fixed domain-boundary
// rules with std::string and the shipped default lists. It checks that the
// false positives are reproducible with the current rule, gone with the fix,
// and that all negative/control cases behave identically under both.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_urlcheck_leading_punctuation_bug test_urlcheck_leading_punctuation_bug.cpp
//   /tmp/test_urlcheck_leading_punctuation_bug

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

// The characters the fix accepts at the START of a token before an exception.
static bool isLeadingPunctuation(char c)
{
    return c == '(' || c == '[' || c == '{' || c == '\'' || c == '"';
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

static bool domainBoundary(const string &token, size_t exPos, bool fixed)
{
    const string prefix = token.substr(0, exPos);
    if (!fixed) {
        return prefix.empty() || prefix.back() == '.' || prefix == "http://" ||
               prefix == "https://";
    }
    bool onlyLeadingPunctuation = true;
    for (char c : prefix) {
        if (!isLeadingPunctuation(c)) {
            onlyLeadingPunctuation = false;
            break;
        }
    }
    return prefix.empty() || prefix.back() == '.' || prefix == "http://" ||
           prefix == "https://" || onlyLeadingPunctuation;
}

// Faithful model of validSuffix in urlcheck.cpp:70-110 (unchanged by this fix).
static bool validSuffix(const string &suffix, bool exceptionEndsWithDot)
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
    for (char c : suffix) {
        if (!isSentencePunctuation(c)) {
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
            if (domainBoundary(token, exPos, fixed) && validSuffix(suffix, endsWithDot)) {
                return true;
            }
            exPos = token.find(exception, exPos + 1);
        }
    }
    return false;
}

static bool runUrlCheck(string msg, bool fixed)
{
    msg = toLower(msg);
    for (const string &token : splitWhitespace(msg)) {
        if (isWhitelistedToken(token, kUrlExceptions, fixed)) {
            continue;
        }
        for (const string &url : kUrlStrings) {
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
    // Whitelisted URLs wrapped in opening punctuation: currently flagged,
    // must be allowed after the fix.
    {"(pokerth.net)", false, true},
    {"[pokerth.net]", false, true},
    {"{pokerth.net}", false, true},
    {"\"pokerth.net\"", false, true},
    {"'pokerth.net'", false, true},
    {"((pokerth.net))", false, true},
    // Already allowed today via the shorter "pokerth.net" exception (the
    // prefix "(https://www." ends in '.') - keep it as a control case.
    {"(https://www.pokerth.net)", false, false},
    {"[http://www.esl.com]", false, true},
    {"(\"pokerth.net\")", false, true},
    // Embedded-substring bypasses stay flagged with the fixed boundary rule.
    {"(evilpokerth.net)", true, false},
    {"[evilpokerth.net]", true, false},
    {"(pokerth.net.evil.com)", true, false},
    {"http://evil.com", true, false},
    {"(http://evil.com)", true, false},
    {"(www.spam.org)", true, false},
    {"pokerth.net and (www.spamcasino.com)", true, false},
    // Control cases unaffected by the fix.
    {"pokerth.net", false, false},
    {"pokerth.net.", false, false},
    {"pokerth.net)", false, false},
    {"(see pokerth.net)", false, false},
    {"(www.pokerth.net)", false, false},
    {"hello world, no links here", false, false},
    {"", false, false},
};

int main()
{
    int failures = 0;
    int reproduced = 0;
    int falsePositiveTotal = 0;

    printf("Testing UrlCheck whitelisted-URL leading-punctuation false positive:\n\n");
    printf("%-42s %8s %8s %8s\n", "message", "expected", "current", "fixed");

    for (const TestCase &tc : kCases) {
        const bool current = runUrlCheck(tc.msg, false);
        const bool fixed = runUrlCheck(tc.msg, true);

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
        printf("%-42s %8s %8s %8s%s\n",
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
