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

// Regression test: CleanerConfig::readConfigInt() silently accepts trailing
// garbage.
//
// src/chatcleaner/cleanerconfig.cpp:524-531 (buggy):
//
//     istringstream isst;
//     isst.str(tempString);
//     if (!(isst >> tempInt)) {       // <-- only the fail bit is checked
//         LOG_ERROR(...);
//         return 0;
//     }
//     return tempInt;
//
// operator>> for int stops at the first non-digit. When the digits are
// followed by non-numeric garbage ("2junk"), the stream is left positioned on
// that garbage with neither failbit nor eofbit set. The function therefore
// returns the numeric prefix (2) and never reports the malformed value.
//
// The client-side twin ConfigFile::readConfigInt() was already fixed for this
// exact class of bug (commit 2ff28c4) by skipping trailing whitespace and then
// requiring eof(). This test models both the current buggy logic and that fix,
// asserts that the bug is reproduced, and verifies that the fix rejects real
// garbage while still accepting legitimately whitespace-padded values.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_cleanerconfig_int_garbage_bug test_cleanerconfig_int_garbage_bug.cpp
//   /tmp/test_cleanerconfig_int_garbage_bug

#include <cstdio>
#include <sstream>
#include <string>

namespace {

int failures = 0;

void check(const char* label, bool ok)
{
    std::printf("  %-72s %s\n", label, ok ? "PASS" : "FAIL");
    if (!ok)
        ++failures;
}

// Verbatim model of the current (buggy) cleanerconfig.cpp:524-531 logic.
int readConfigIntBuggy(const std::string& value)
{
    int tempInt = 0;
    std::istringstream isst;
    isst.str(value);
    if (!(isst >> tempInt))
        return 0;
    return tempInt;
}

// Model of the fix already applied to ConfigFile::readConfigInt(): reject
// genuine trailing garbage while tolerating trailing whitespace.
int readConfigIntFixed(const std::string& value)
{
    int tempInt = 0;
    std::istringstream isst(value);
    isst >> tempInt;
    if (isst.fail())
        return 0;
    isst >> std::ws;
    if (!isst.eof())
        return 0;
    return tempInt;
}

} // namespace

int main()
{
    std::printf("Testing CleanerConfig::readConfigInt() trailing-garbage handling:\n\n");

    std::printf("1) Clean values parse identically with buggy and fixed logic:\n");
    check("\"10\"", readConfigIntBuggy("10") == 10 && readConfigIntFixed("10") == 10);
    check("\" 10\"", readConfigIntBuggy(" 10") == 10 && readConfigIntFixed(" 10") == 10);
    check("\"-5\"", readConfigIntBuggy("-5") == -5 && readConfigIntFixed("-5") == -5);
    check("\"0\"", readConfigIntBuggy("0") == 0 && readConfigIntFixed("0") == 0);

    std::printf("\n2) Trailing garbage is accepted today but rejected by the fix:\n");
    check("\"10abc\": buggy=10 fixed=0",
          readConfigIntBuggy("10abc") == 10 && readConfigIntFixed("10abc") == 0);
    check("\"4327xyz\": buggy=4327 fixed=0",
          readConfigIntBuggy("4327xyz") == 4327 && readConfigIntFixed("4327xyz") == 0);
    check("\"2junk\": buggy=2 fixed=0",
          readConfigIntBuggy("2junk") == 2 && readConfigIntFixed("2junk") == 0);
    check("\"-5junk\": buggy=-5 fixed=0",
          readConfigIntBuggy("-5junk") == -5 && readConfigIntFixed("-5junk") == 0);

    std::printf("\n3) Trailing whitespace is still accepted by the fixed logic:\n");
    check("\"10 \"", readConfigIntFixed("10 ") == 10);
    check("\"7234 \"", readConfigIntFixed("7234 ") == 7234);
    check("\"10\\n\"", readConfigIntFixed("10\n") == 10);

    std::printf("\n4) Non-numeric and empty values stay rejected by the fix:\n");
    check("\"abc\"", readConfigIntFixed("abc") == 0);
    check("\"\"", readConfigIntFixed("") == 0);

    if (failures) {
        std::printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    std::printf("\nTEST PASSED: trailing-garbage bug reproduced and fix verified\n");
    return 0;
}
