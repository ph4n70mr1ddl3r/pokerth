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

// Regression test: ConfigFile::readConfigIntList() and
// CleanerConfig::readConfigIntList() silently accept trailing garbage in
// numeric list entries.
//
// Background
// ----------
// The scalar readers were hardened in earlier bug hunts:
//
//   ConfigFile::readConfigInt()        (src/config/configfile.cpp:748-783)
//   CleanerConfig::readConfigInt()     (src/chatcleaner/cleanerconfig.cpp:524-540)
//
// Both now parse, check fail(), skip std::ws, then require eof(): a padded
// value such as "7234 " stays valid while real garbage ("7234abc") is
// rejected and logged.
//
// The bug
// -------
// The list variants still use the original, naive loop:
//
//   ConfigFile::readConfigIntList()    (src/config/configfile.cpp:797-806)
//   CleanerConfig::readConfigIntList() (src/chatcleaner/cleanerconfig.cpp:576-586)
//
//     istringstream isst;
//     for (each string value in the list) {
//         int tempInt = 0;
//         isst.str(value);
//         isst >> tempInt;          // stops at the first non-digit
//         tempIntList.push_back(tempInt);   // <-- pushed unconditionally
//         isst.str("");
//         isst.clear();
//     }
//
// operator>> for int stops at the first character that cannot be part of the
// number. No fail()/eof() check is performed, so a hand-edited config list
// entry such as "200junk" is silently accepted as 200 and "abc" becomes 0
// with no error logged. This matters for the blinds lists
// ("ManualBlindsList" / "NetManualBlindsList") that are read straight from
// the user-editable config XML: garbage is laundered into a plausible blind
// value instead of being flagged.
//
// The fix
// -------
// Apply the exact same validation as the scalar readers to every list entry:
// parse, check fail() first, then skip std::ws and require eof(). An invalid
// entry is logged and replaced by 0 (the scalar readers' fallback), keeping
// list length and positions stable.
//
// Note: fail() must be checked *before* ">> ws". When ">> int" reaches the end
// of a clean value like "200" it sets eofbit but not failbit; ">> ws" on an
// eofbit stream then sets failbit, so checking fail() afterwards would
// incorrectly reject valid values. The scalar fix checks only eof() after ws.
//
// This standalone test models the current (buggy) and fixed loops with
// std::istringstream. It asserts the false acceptance is reproducible, the
// fix removes it, and valid/padded values still parse identically.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_configfile_intlist_garbage_bug test_configfile_intlist_garbage_bug.cpp
//   /tmp/test_configfile_intlist_garbage_bug

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(const char* label, bool ok)
{
    std::printf("  %-74s %s\n", label, ok ? "PASS" : "FAIL");
    if (!ok)
        ++failures;
}

std::vector<int> parseListBuggy(const std::vector<std::string>& values)
{
    std::vector<int> result;
    std::istringstream isst;
    for (const std::string& value : values) {
        int tempInt = 0;
        isst.str(value);
        isst >> tempInt;
        result.push_back(tempInt);
        isst.str("");
        isst.clear();
    }
    return result;
}

std::vector<int> parseListFixed(const std::vector<std::string>& values)
{
    std::vector<int> result;
    std::istringstream isst;
    for (const std::string& value : values) {
        int tempInt = 0;
        isst.str(value);
        isst >> tempInt;
        if (isst.fail()) {
            tempInt = 0; // would LOG_ERROR in the real code
        } else {
            isst >> std::ws;
            if (!isst.eof()) {
                tempInt = 0; // would LOG_ERROR in the real code
            }
        }
        result.push_back(tempInt);
        isst.str("");
        isst.clear();
    }
    return result;
}

bool equals(const std::vector<int>& actual, const std::vector<int>& expected)
{
    return actual == expected;
}

} // namespace

int main()
{
    std::printf("Testing ConfigFile/CleanerConfig readConfigIntList() garbage handling:\n\n");

    std::printf("1) Valid values parse identically with buggy and fixed logic:\n");
    check("clean list {100,200,300}",
          equals(parseListBuggy({"100", "200", "300"}), {100, 200, 300}) &&
          equals(parseListFixed({"100", "200", "300"}), {100, 200, 300}));
    check("padded values {\"100 \",\"200\\t\",\" 300\"}",
          equals(parseListBuggy({"100 ", "200\t", " 300"}), {100, 200, 300}) &&
          equals(parseListFixed({"100 ", "200\t", " 300"}), {100, 200, 300}));
    check("negative and zero {\"-50\",\"0\",\"300\"}",
          equals(parseListBuggy({"-50", "0", "300"}), {-50, 0, 300}) &&
          equals(parseListFixed({"-50", "0", "300"}), {-50, 0, 300}));

    std::printf("\n2) Trailing garbage is silently laundered today, rejected by the fix:\n");
    {
        const std::vector<std::string> values = {"100", "200junk", "300"};
        const auto buggy = parseListBuggy(values);
        const auto fixed = parseListFixed(values);
        check("buggy turns \"200junk\" into 200", buggy == std::vector<int>({100, 200, 300}));
        check("fixed rejects \"200junk\" -> 0, neighbors intact",
              fixed == std::vector<int>({100, 0, 300}));
    }
    {
        const std::vector<std::string> values = {"10x", "20y"};
        const auto buggy = parseListBuggy(values);
        const auto fixed = parseListFixed(values);
        check("buggy turns \"10x\",\"20y\" into 10,20", buggy == std::vector<int>({10, 20}));
        check("fixed rejects both entries -> 0,0", fixed == std::vector<int>({0, 0}));
    }
    {
        const std::vector<std::string> values = {"10/2", "2abc", "5#x"};
        const auto buggy = parseListBuggy(values);
        const auto fixed = parseListFixed(values);
        check("buggy accepts mixed garbage", buggy == std::vector<int>({10, 2, 5}));
        check("fixed rejects all mixed garbage", fixed == std::vector<int>({0, 0, 0}));
    }

    std::printf("\n3) Non-numeric / empty entries are rejected and list shape is preserved:\n");
    check("\"abc\" -> 0 in both",
          equals(parseListBuggy({"abc"}), {0}) && equals(parseListFixed({"abc"}), {0}));
    check("empty string -> 0 in both",
          equals(parseListBuggy({""}), {0}) && equals(parseListFixed({""}), {0}));
    check("fixed keeps list length for garbage entries",
          parseListFixed({"1", "bad", "3", "4oops"}).size() == 4);
    check("fixed accepts fully padded valid list",
          equals(parseListFixed({" 10 ", "20 ", "\t30"}), {10, 20, 30}));

    if (failures) {
        std::printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    std::printf("\nTEST PASSED: int-list trailing-garbage bug reproduced and fix verified\n");
    return 0;
}
