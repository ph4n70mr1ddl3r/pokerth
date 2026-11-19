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

// Regression test: ConfigFile::readConfigInt() rejects a perfectly valid
// integer when the value has trailing whitespace.
//
// src/config/configfile.cpp:767-774 (buggy):
//
//     istringstream isst(tempString);
//     isst >> tempInt;
//     if (isst.fail() || !isst.eof()) {   // <-- the bug
//         LOG_ERROR(...);
//         return 0;
//     }
//     return tempInt;
//
// operator>> for int stops at the first non-digit. When the digits are followed
// by whitespace, the stream is left positioned on that whitespace with neither
// failbit nor eofbit set: fail() == false but eof() == false. The "!isst.eof()"
// clause therefore treats a fully parsed number as an error and returns 0.
//
// Config values are read straight from the XML "value" attribute without
// trimming (configfile.cpp, readConfigFile: conf.attribute("value", "")), and
// XML attribute-value normalization turns embedded tabs/newlines into plain
// spaces. A value such as value="7234 " thus silently becomes port 0.
//
// The sibling CleanerConfig::readConfigInt() only checks the stream's fail bit
// and accepts trailing whitespace, so the two config readers disagree.
//
// The fix keeps the original strictness against real trailing garbage while
// accepting trailing whitespace: parse, check fail(), skip std::ws, then
// require eof(). This standalone test models both rules with std::istringstream
// and asserts that the bug is reproduced, the fix removes it, and genuine
// garbage is still rejected.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_configfile_int_whitespace_bug test_configfile_int_whitespace_bug.cpp
//   /tmp/test_configfile_int_whitespace_bug

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

// Verbatim model of the current (buggy) configfile.cpp:767-774 logic.
int readConfigIntBuggy(const std::string& value)
{
    int tempInt = 0;
    std::istringstream isst(value);
    isst >> tempInt;
    if (isst.fail() || !isst.eof())
        return 0;
    return tempInt;
}

// Model of the fix: ignore trailing whitespace, still reject non-whitespace
// garbage after the number.
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
    std::printf("Testing ConfigFile::readConfigInt() trailing-whitespace handling:\n\n");

    struct Case {
        const char* value;
        int expected;
    };

    const Case cases[] = {
        {"7234", 7234},     // clean value: works today
        {" 7234", 7234},    // leading whitespace: works today
        {"0", 0},           // zero: works today
        {"7234 ", 7234},    // trailing space: BUG today
        {"5000 ", 5000},    // trailing space: BUG today
        {"10 ", 10},        // trailing space: BUG today
        {"7234\n", 7234},   // trailing newline: BUG today
        {"7234abc", 0},     // real trailing garbage: must stay rejected
        {"abc", 0},         // non-numeric: must stay rejected
        {"", 0},            // empty: must stay rejected
    };

    std::printf("1) Clean values parse identically with buggy and fixed logic:\n");
    check("clean \"7234\"", readConfigIntBuggy("7234") == 7234 && readConfigIntFixed("7234") == 7234);
    check("leading whitespace \" 7234\"",
          readConfigIntBuggy(" 7234") == 7234 && readConfigIntFixed(" 7234") == 7234);
    check("zero \"0\"", readConfigIntBuggy("0") == 0 && readConfigIntFixed("0") == 0);

    std::printf("\n2) Trailing whitespace is misread by current code but fixed correctly:\n");
    for (const Case& c : cases) {
        const bool hasTrailingWhitespace =
            (std::string(c.value) == "7234 " || std::string(c.value) == "5000 " ||
             std::string(c.value) == "10 " || std::string(c.value) == "7234\n");
        if (!hasTrailingWhitespace)
            continue;
        char label[96];
        std::snprintf(label, sizeof(label), "\"%s\": buggy=0 fixed=%d", c.value, c.expected);
        check(label, readConfigIntBuggy(c.value) == 0 && readConfigIntFixed(c.value) == c.expected);
    }

    std::printf("\n3) Genuine garbage is still rejected by the fixed logic:\n");
    check("\"7234abc\"", readConfigIntFixed("7234abc") == 0);
    check("\"abc\"", readConfigIntFixed("abc") == 0);
    check("\"\"", readConfigIntFixed("") == 0);

    if (failures) {
        std::printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    std::printf("\nTEST PASSED: trailing-whitespace bug reproduced and fix verified\n");
    return 0;
}
