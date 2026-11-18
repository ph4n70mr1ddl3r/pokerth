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

// Regression test: BadWordCheck cannot detect space-delimited bad words when
// they appear at the start or end of a message.
//
// Background
// ----------
// BadWordCheck::run() (src/chatcleaner/badwordcheck.cpp:39-68) lowercases the
// message, removes whitelisted phrases, then flags the message if it contains
// any configured bad word as a plain substring:
//
//     msg = msg.toLower();
//     ... remove exceptions ...
//     if (!bw.isEmpty() && msg.contains(bw)) return true;
//
// Three of the shipped default bad words (cleanerconfig.cpp:154,156,161) carry
// a leading AND trailing space so the match is restricted to a standalone word
// and does not fire inside longer words such as "dickinson":
//
//     " dick ",  " fag ",  " fuk "
//
// The bug
// -------
// The message is never padded, so a space-delimited word can only match when
// it has a real space on BOTH sides. At a message boundary that space is
// missing, and the insult slips through:
//
//     "dick"           -> not flagged (WRONG)
//     "you are a dick" -> not flagged (WRONG)
//     "dick face"      -> not flagged (WRONG)
//     "a dick lol"     -> flagged     (the only placement that works)
//
// The raw chat message is handed straight to the filter
// (cleanerserver.cpp:231), so no layer adds the missing padding. This is a
// false negative in the content filter, distinct from the whitelist bypass
// already fixed in 1b9b913.
//
// The fix
// -------
// Pad the message with a single space at each end after lowercasing. A word at
// the boundary then has the surrounding spaces its configured entry requires,
// while the inner-word guard is preserved ("dickinson" still does not match
// " dick " because the padded form is " dickinson ", which has no trailing
// space after the "dick" substring).
//
// This standalone test models both the current (buggy) and the fixed matching
// rules with std::string and the shipped default lists. It asserts the false
// negative is reproducible, the fix removes it, and the existing behavior
// (inner-word guard, whitelist handling, non-delimited words) is preserved.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_badword_boundary_false_negative test_badword_boundary_false_negative.cpp
//   /tmp/test_badword_boundary_false_negative

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

using std::string;
using std::vector;

static const vector<string> kBadWords = {
	"arsch", "asshole", "bastard", "bitch", "cunt", " dick ", "drecksau",
	" fag ", "fagget", "fotze", "fuker", "fuck", " fuk ", "gay", "horny",
	"hure", "idiot", "mistgeburt", "missgeburt", "motherfucker", "nazi",
	"nigga", "nigger", "nutte", "ommak", "penis", "pussy", "schlampe",
	"schwanz", "sex", "shit", "sieg heil", "slut", "suck", "whore"};

static const vector<string> kExceptions = {
	"idiots end", "all-in-idiot", "all-in idiot", "allin idiot", "all in idiot"};

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

static bool runCheck(string msg, bool padBoundaries)
{
	msg = toLower(msg);
	if (padBoundaries) {
		msg = " " + msg + " ";
	}

	// Remove whitelisted phrases, replacing each with a single space (mirrors
	// BadWordCheck::run() so the two models stay comparable).
	for (const string &exception : kExceptions) {
		if (exception.empty()) {
			continue;
		}
		size_t pos = msg.find(exception);
		while (pos != string::npos) {
			msg.replace(pos, exception.size(), " ");
			pos = msg.find(exception, pos + 1);
		}
	}

	for (const string &bw : kBadWords) {
		if (contains(msg, bw)) {
			return true;
		}
	}
	return false;
}

static int failures = 0;

static void expect(const char *label, bool got, bool want)
{
	if (got != want) {
		std::printf("FAIL: %s (got %s, want %s)\n", label,
			got ? "true" : "false", want ? "true" : "false");
		++failures;
	}
}

int main()
{
	// 1. The false negative is reproducible with the current (unpadded) rule.
	expect("buggy: bare 'dick'", runCheck("dick", false), false);
	expect("buggy: 'you are a dick'", runCheck("you are a dick", false), false);
	expect("buggy: 'dick face'", runCheck("dick face", false), false);
	expect("buggy: bare 'fag'", runCheck("fag", false), false);
	expect("buggy: bare 'fuk'", runCheck("fuk", false), false);
	expect("buggy: case-insensitive 'DICK'", runCheck("DICK", false), false);

	// 2. The fix detects all of the above boundary cases.
	expect("fixed: bare 'dick'", runCheck("dick", true), true);
	expect("fixed: 'you are a dick'", runCheck("you are a dick", true), true);
	expect("fixed: 'dick face'", runCheck("dick face", true), true);
	expect("fixed: bare 'fag'", runCheck("fag", true), true);
	expect("fixed: bare 'fuk'", runCheck("fuk", true), true);
	expect("fixed: case-insensitive 'DICK'", runCheck("DICK", true), true);

	// 3. The inner-word guard is preserved: no new false positives.
	expect("fixed: 'dickinson' not flagged", runCheck("dickinson", true), false);
	expect("fixed: 'dickens' not flagged", runCheck("dickens", true), false);
	expect("fixed: neutral text", runCheck("hello world", true), false);

	// 4. Whitelist behavior is unchanged, and a real insult next to a
	//    whitelisted phrase is now caught.
	expect("fixed: whitelisted 'all-in-idiot'", runCheck("all-in-idiot", true), false);
	expect("fixed: insult after whitelist", runCheck("all-in-idiot dick", true), true);
	expect("buggy: insult after whitelist", runCheck("all-in-idiot dick", false), false);

	// 5. Non-delimited bad words keep working in both models.
	expect("control: 'fuck' buggy", runCheck("fuck", false), true);
	expect("control: 'fuck' fixed", runCheck("fuck", true), true);
	expect("control: 'what the fuck' fixed", runCheck("what the fuck", true), true);

	if (failures == 0) {
		std::printf("ALL TESTS PASSED\n");
		return 0;
	}
	std::printf("%d TEST(S) FAILED\n", failures);
	return 1;
}
