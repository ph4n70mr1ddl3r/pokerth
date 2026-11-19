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

// Regression test: ServerBanManager name-pattern checks use regex_match()
// instead of regex_search(), so an admin-configured bad word / ban pattern
// is only enforced when it matches the ENTIRE name.
//
// Background
// ----------
// Two checks in src/net/serverbanmanager.cpp compile an admin-provided list of
// regular expressions (config keys "GameNameBadWordList" and the player-name
// regex bans) and then test a candidate name against them:
//
//   IsBadGameName()  (serverbanmanager.cpp:262):  regex_match(name, regex)
//   IsPlayerBanned() (serverbanmanager.cpp:208):  regex_match(name, nameRegex)
//
// boost::regex_match() requires the whole string to match. The intent of a
// "bad word list" / regex ban is to detect the pattern ANYWHERE in the name,
// which is what boost::regex_search() does. The patterns are compiled with
// regex::icase, so a plain entry like "shit" should reject "what the shit",
// "SHIT happens", etc. With regex_match it only rejects the exact name "shit".
//
// The bug
// -------
//   name                 pattern   regex_match (buggy)   regex_search (fixed)
//   "what the shit"      "shit"    allowed (WRONG)       blocked
//   "SHIT happens"       "shit"    allowed (WRONG)       blocked
//   "cheater123"         "cheat"   not banned (WRONG)    banned
//
// The fix
// -------
// Use regex_search() in both places. It is a strict superset for detection:
// whole-string patterns an admin may already use (e.g. ".*shit.*") and
// anchored patterns (e.g. "^shit$") keep working exactly as before.
//
// This standalone test models the "buggy" (regex_match) and "fixed"
// (regex_search) semantics with std::regex. boost::regex::extended|icase and
// std::regex::ECMAScript|icase are equivalent for the literals, ".*" and "^$"
// anchors used here. It asserts the false negatives are reproducible, the fix
// removes them, and clean-name / anchored-pattern behavior is preserved.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_badgamename_regex_match_bug test_badgamename_regex_match_bug.cpp
//   /tmp/test_badgamename_regex_match_bug

#include <cstdio>
#include <regex>
#include <string>
#include <vector>

using std::string;
using std::vector;

static int failures = 0;

static void expect(const char *label, bool got, bool want)
{
	if (got != want) {
		std::printf("FAIL: %s (got %s, want %s)\n", label,
			got ? "true" : "false", want ? "true" : "false");
		++failures;
	}
}

static std::regex makePattern(const string &word)
{
	return std::regex(word, std::regex::ECMAScript | std::regex::icase);
}

// Models a name filter / ban list. For each configured pattern, returns true
// if any pattern matches according to `useSearch`.
static bool check(const string &name, const vector<string> &patterns, bool useSearch)
{
	for (const string &word : patterns) {
		const std::regex re = makePattern(word);
		if (useSearch ? std::regex_search(name, re) : std::regex_match(name, re)) {
			return true;
		}
	}
	return false;
}

int main()
{
	const vector<string> badWords = {"shit", "fuck", "bitch"};
	const vector<string> banPatterns = {"cheat", "hacker"};

	// 1. The bug is reproducible: embedded bad words / ban substrings pass
	//    with regex_match() (buggy) but are caught with regex_search() (fixed).
	expect("buggy: 'what the shit' allowed", check("what the shit", badWords, false), false);
	expect("fixed: 'what the shit' blocked", check("what the shit", badWords, true), true);
	expect("buggy: 'SHIT happens' allowed", check("SHIT happens", badWords, false), false);
	expect("fixed: 'SHIT happens' blocked", check("SHIT happens", badWords, true), true);
	expect("buggy: 'bullshit poker' allowed", check("bullshit poker", badWords, false), false);
	expect("fixed: 'bullshit poker' blocked", check("bullshit poker", badWords, true), true);

	expect("buggy: 'cheater123' not banned", check("cheater123", banPatterns, false), false);
	expect("fixed: 'cheater123' banned", check("cheater123", banPatterns, true), true);
	expect("buggy: 'elite_HACKER_99' not banned", check("elite_HACKER_99", banPatterns, false), false);
	expect("fixed: 'elite_HACKER_99' banned", check("elite_HACKER_99", banPatterns, true), true);

	// 2. Whole-string matches worked before and still work after the fix.
	expect("control: exact 'shit' buggy", check("shit", badWords, false), true);
	expect("control: exact 'shit' fixed", check("shit", badWords, true), true);
	expect("control: exact 'cheat' buggy", check("cheat", banPatterns, false), true);
	expect("control: exact 'cheat' fixed", check("cheat", banPatterns, true), true);

	// 3. Clean names are accepted by both models (no new false positives).
	expect("clean name buggy", check("friendly poker table", badWords, false), false);
	expect("clean name fixed", check("friendly poker table", badWords, true), false);
	expect("clean nick buggy", check("newbie42", banPatterns, false), false);
	expect("clean nick fixed", check("newbie42", banPatterns, true), false);

	// 4. Admin patterns that already relied on full-match semantics are
	//    unchanged by switching to regex_search().
	const vector<string> wildcard = {".*shit.*"};
	expect("wildcard pattern still matches embedded (buggy)", check("what the shit", wildcard, false), true);
	expect("wildcard pattern still matches embedded (fixed)", check("what the shit", wildcard, true), true);
	const vector<string> anchored = {"^shit$"};
	expect("anchored pattern still rejects exact (fixed)", check("shit", anchored, true), true);
	expect("anchored pattern still allows embedded (fixed)", check("what the shit", anchored, true), false);

	if (failures == 0) {
		std::printf("ALL TESTS PASSED\n");
		return 0;
	}
	std::printf("%d TEST(S) FAILED\n", failures);
	return 1;
}
