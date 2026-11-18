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

// Regression test: UrlCheck whitelists a bare ESL host but flags the very same
// whitelisted host as soon as it carries a path, query or fragment.
//
// Background
// ----------
// UrlCheck::run() (src/chatcleaner/urlcheck.cpp:95-117) flags a message when a
// whitespace-delimited token contains a blocked URL pattern (the shipped
// defaults are "http://", ".com", ".net", ".org", ".de"), unless the token is
// recognised as a whitelisted site. The shipped exceptions
// (cleanerconfig.cpp:203-205) are:
//
//   "http://www.esl.", "https://www.pokerth.net", "pokerth.net"
//
// "http://www.esl." is an incomplete host: the trailing dot expects a TLD
// label ("com", "de", ...) to complete it. The contract is documented right
// above isWhitelistedToken() (urlcheck.cpp:39-46): the text following an
// exception may be empty, "a path/query on the whitelisted host", or -- for a
// prefix exception ending in '.' -- "a single TLD-like label completing the
// host".
//
// The bug
// -------
// In the dot-ending branch of isWhitelistedToken() (urlcheck.cpp:67-80), the
// code scans the whole remaining suffix and requires EVERY character to be a
// letter or number. Once the TLD label is consumed, a following '/', '?' or
// '#' therefore invalidates the whitelist match:
//
//   "http://www.esl.com"                -> allowed (correct)
//   "http://www.esl.com/"               -> flagged (WRONG)
//   "http://www.esl.com/de/index.php"   -> flagged (WRONG)
//   "http://www.esl.de?x=1"             -> flagged (WRONG)
//
// So ordinary links to a *whitelisted* site are reported as URL spam whenever
// they point to anything but the site root. This is the opposite of the
// bypasses fixed in 50b82e8 / 1813e91 / 97b6334: those were false negatives
// (bad hosts let through), this one is a false positive (good hosts blocked).
//
// The fix
// -------
// Consume only the single TLD-like label, then accept a path/query/fragment
// delimiter ('/', '?', '#') immediately after it. Anything else -- in
// particular another '.' introducing an extra host label -- stays invalid, so
// the already-fixed "http://www.esl.com.evil.com" bypass remains blocked.
//
// This standalone test models both the current (buggy) and the fixed suffix
// rules with std::string and the shipped default lists. It asserts the false
// positive is reproducible, the fix removes it, and every previously covered
// behavior (bypasses, cross-contamination, plain spam, TLD-ending exceptions)
// is preserved.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_urlcheck_esl_path_false_positive test_urlcheck_esl_path_false_positive.cpp
//   /tmp/test_urlcheck_esl_path_false_positive

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

// Current suffix rule (urlcheck.cpp:67-80 before the fix): for a dot-ending
// exception the ENTIRE suffix must be alphanumeric, so a path after the TLD
// label wrongly invalidates the whitelist match.
static bool validSuffixCurrent(const string &suffix, bool exceptionEndsWithDot)
{
    if (suffix.empty()) {
        return true;
    }
    if (isPathDelimiter(suffix[0])) {
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

// Fixed suffix rule: consume exactly one TLD-like label, then allow a
// path/query/fragment delimiter. A '.' after the label still means an extra
// host label and is rejected.
static bool validSuffixFixed(const string &suffix, bool exceptionEndsWithDot)
{
    if (suffix.empty()) {
        return true;
    }
    if (isPathDelimiter(suffix[0])) {
        return true;
    }
    if (!exceptionEndsWithDot) {
        return false;
    }
    const size_t labelStart = (suffix[0] == '.') ? 1 : 0;
    size_t i = labelStart;
    while (i < suffix.size() && isAlnum(suffix[i])) {
        ++i;
    }
    if (i == labelStart) {
        return false;
    }
    return i == suffix.size() || isPathDelimiter(suffix[i]);
}

static bool isWhitelistedToken(const string &token,
                               const vector<string> &exceptions,
                               bool fixed)
{
    for (const string &exception : exceptions) {
        if (exception.empty()) {
            continue;
        }
        const bool endsWithDot = exception.back() == '.';
        size_t exPos = token.find(exception);
        while (exPos != string::npos) {
            const string suffix = token.substr(exPos + exception.size());
            const bool valid = fixed
                ? validSuffixFixed(suffix, endsWithDot)
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
    // Whitelisted ESL hosts WITH a path/query/fragment: currently flagged,
    // must be allowed after the fix.
    {"http://www.esl.com/", false, true},
    {"http://www.esl.com/de/index.php", false, true},
    {"http://www.esl.de/startseite", false, true},
    {"http://www.esl.com?x=1", false, true},
    {"http://www.esl.com#top", false, true},
    {"http://www.esl.org/tournaments?lang=en#live", false, true},
    {"see http://www.esl.com/de/index.php thanks", false, true},
    // Extra host label after the TLD is still a different (attacker) host and
    // must stay flagged, even when a path follows it.
    {"http://www.esl.com.evil.com", true, false},
    {"http://www.esl.de.attacker.net/path", true, false},
    {"http://www.esl.com.evil.org/steal", true, false},
    // Previously covered bypasses must remain flagged.
    {"pokerth.net.xyz", true, false},
    {"pokerth.net.co", true, false},
    {"https://www.pokerth.net.io", true, false},
    {"pokerth.net.evil.com", true, false},
    {"http://pokerth.net.attacker.com", true, false},
    {"evilpokerth.net", true, false},
    // Cross-contamination and plain spam must remain flagged.
    {"pokerth.net and www.spamcasino.com", true, false},
    {"http://evil.com", true, false},
    {"www.spam.org", true, false},
    {"http://www.esl.com.evil.de and pokerth.net", true, false},
    // Legitimate whitelisted URLs must remain allowed (bare hosts, and paths
    // on the exceptions that already end in a TLD).
    {"http://www.esl.com", false, false},
    {"http://www.esl.de", false, false},
    {"pokerth.net", false, false},
    {"https://www.pokerth.net", false, false},
    {"http://www.pokerth.net/forums/showthread.php?t=1", false, false},
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
    int falsePositiveTotal = 0;

    printf("Testing UrlCheck whitelisted ESL host with path (false positive):\n\n");
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
