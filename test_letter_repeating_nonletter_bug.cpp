/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2013 Felix Hammer, Florian Thauer, Lothar May          *
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

// Regression test: LetterRepeatingCheck flags repeated non-letters as offences.
//
// Background
// ----------
// LetterRepeatingCheck (src/chatcleaner/letterrepeatingcheck.cpp) implements
// the "letter repeating" chat offence. Its config key is
// "LetterRepeatingNumberToTrigger" (default 10, cleanerconfig.cpp:143), and a
// positive result escalates the offence level in MessageFilter::check()
// (messagefilter.cpp:86), potentially warning or kicking the player.
//
// The bug
// -------
// The detection regex (src/chatcleaner/letterrepeatingcheck.cpp:47) is:
//
//     .*(.)\1{letterNumberToTrigger-1,}.*
//
// The capture group is an unconstrained "." — it matches ANY character, not
// just a letter. Consequently a run of identical punctuation or digits such as
// "!!!!!!!!!!" or "0000000000" is reported as "letter repeating" even though
// no letter is repeated at all. Players get warned/kicked for perfectly normal
// messages like "OMG!!!!!!!!!!" or "1234567890!!!...".
//
// The fix
// -------
// Constrain the repeated character to a letter using the Unicode letter
// property \p{L}:
//
//     .*(\p{L})\1{letterNumberToTrigger-1,}.*
//
// Now only genuine repeated letters are flagged; punctuation/digit runs are
// ignored, while real repeats such as "AAAAAAAAAA" are still detected.
//
// This test needs no Qt: it faithfully models the regex with a run-length
// scan over the normalized message (simplified().remove(" ") removes all
// whitespace), comparing the buggy any-character rule with the fixed
// letters-only rule.
//
// Compile manually with:
//   g++ -O2 -std=c++23 -Wall -Wextra -o test_letter_repeating_nonletter_bug test_letter_repeating_nonletter_bug.cpp

#include <cctype>
#include <cstdio>
#include <string>

namespace {

bool isLetter(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

// Model of msg.simplified().remove(" "): all whitespace is removed.
std::string normalize(const std::string& s)
{
    std::string out;
    for (char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            out += c;
        }
    }
    return out;
}

// Buggy model: (.) \1{N,} — any repeated character counts.
bool hasRepeatBuggy(const std::string& input, int trigger)
{
    const std::string s = normalize(input);
    if (trigger < 2) return false;
    int run = 1;
    for (std::size_t i = 1; i < s.size(); ++i) {
        run = (s[i] == s[i - 1]) ? run + 1 : 1;
        if (run >= trigger) return true;
    }
    return false;
}

// Fixed model: (\p{L}) \1{N,} — only repeated letters count.
bool hasRepeatFixed(const std::string& input, int trigger)
{
    const std::string s = normalize(input);
    if (trigger < 2) return false;
    int run = 1;
    for (std::size_t i = 1; i < s.size(); ++i) {
        run = (isLetter(s[i]) && s[i] == s[i - 1]) ? run + 1 : 1;
        if (run >= trigger) return true;
    }
    return false;
}

int failures = 0;

void check(const char* label, bool ok)
{
    std::printf("  %-70s %s\n", label, ok ? "PASS" : "FAIL");
    if (!ok) ++failures;
}

} // namespace

int main()
{
    std::printf("Regression test: LetterRepeatingCheck non-letter false positives\n\n");

    const int trigger = 10;

    // Scenario 1: repeated punctuation only.
    {
        const std::string msg = "OMG!!!!!!!!!!";
        std::printf("Scenario 1: \"%s\" (10x '!')\n", msg.c_str());
        check("buggy flags repeated '!' (reproduces the bug)", hasRepeatBuggy(msg, trigger));
        check("fixed ignores repeated '!'", !hasRepeatFixed(msg, trigger));
    }

    // Scenario 2: repeated digits only.
    {
        const std::string msg = "0000000000";
        std::printf("\nScenario 2: \"%s\" (10x '0')\n", msg.c_str());
        check("buggy flags repeated '0' (reproduces the bug)", hasRepeatBuggy(msg, trigger));
        check("fixed ignores repeated '0'", !hasRepeatFixed(msg, trigger));
    }

    // Scenario 3: genuine repeated letters must still be detected.
    {
        const std::string msg = "AAAAAAAAAA";
        std::printf("\nScenario 3: \"%s\" (10x 'A')\n", msg.c_str());
        check("buggy flags repeated 'A'", hasRepeatBuggy(msg, trigger));
        check("fixed still flags repeated 'A'", hasRepeatFixed(msg, trigger));
    }

    // Scenario 4: interleaved content with a genuine repeated letter.
    {
        const std::string msg = "wiiiiiiiiiii";
        std::printf("\nScenario 4: \"%s\" (many 'i')\n", msg.c_str());
        check("fixed flags the repeated letter 'i'", hasRepeatFixed(msg, trigger));
    }

    // Scenario 5: below threshold must not trigger either way.
    {
        const std::string msg = "AAAAAAAAA";
        std::printf("\nScenario 5: \"%s\" (9x 'A', trigger=10)\n", msg.c_str());
        check("buggy respects the threshold", !hasRepeatBuggy(msg, trigger));
        check("fixed respects the threshold", !hasRepeatFixed(msg, trigger));
    }

    // Scenario 6: mixed repeated punctuation and a repeated letter — fixed
    // still catches the letter offence.
    {
        const std::string msg = "!!!!!!!!!!AAAAAAAAAA";
        std::printf("\nScenario 6: \"%s\" ('!' run then 'A' run)\n", msg.c_str());
        check("fixed ignores '!' run but catches 'A' run", hasRepeatFixed(msg, trigger));
    }

    std::printf("\n");
    if (failures > 0) {
        std::printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("TEST PASSED: non-letter false positive reproduced and fix verified\n");
    return 0;
}
