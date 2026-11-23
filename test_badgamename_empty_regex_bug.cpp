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

// Regression test: an empty entry in GameNameBadWordList rejects every game name.
//
// Background
// ----------
// ServerBanManager::InitGameNameBadWordList() compiles every configured string
// into a boost::regex. ServerBanManager::IsBadGameName() then uses regex_search
// to look for those patterns anywhere in a candidate game name (changed from
// regex_match by commit f63df94).
//
// The bug
// -------
// An empty string is a valid POSIX ERE that matches the zero-length substring
// at position 0. With regex_match the empty entry was harmless (the whole name
// is not the empty string), but with regex_search it matches EVERY name.
// A single blank/empty row in GameNameBadWordList (e.g. an admin left a blank
// row, or a trailing comma in the config) therefore makes IsBadGameName()
// return true for every possible name, silently blocking all game creation.
//
//   config {"", "shit"}   name "Friday Poker"   buggy=true (WRONG)   fixed=false
//   config {"", "shit"}   name "Poker Night"    buggy=true (WRONG)   fixed=false
//   config {"", "shit"}   name "SHIT happens"   buggy=true           fixed=true
//
// The fix
// -------
// Ignore empty strings when building the filter. An empty bad word carries no
// filtering intent.
//
// This standalone test models the filter with std::regex. Empty-pattern
// semantics are identical between boost::regex::extended|icase and
// std::regex::ECMAScript|icase. Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_badgamename_empty_regex_bug test_badgamename_empty_regex_bug.cpp
//   /tmp/test_badgamename_empty_regex_bug

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

// Models ServerBanManager's filter. skipEmpty selects the fixed behaviour;
// with skipEmpty=false even empty config entries are compiled (the bug).
struct NameFilter {
	vector<std::regex> patterns;

	void build(const vector<string> &words, bool skipEmpty)
	{
		for (const string &word : words) {
			if (skipEmpty && word.empty())
				continue;
			patterns.push_back(std::regex(word, std::regex::ECMAScript | std::regex::icase));
		}
	}

	bool isBad(const string &name) const
	{
		for (const auto &re : patterns) {
			if (std::regex_search(name, re))
				return true;
		}
		return false;
	}

	bool isBadOldMatch(const string &name) const
	{
		for (const auto &re : patterns) {
			if (std::regex_match(name, re))
				return true;
		}
		return false;
	}
};

int main()
{
	// 1. A lone empty entry poisons the whole list under regex_search (bug),
	//    while the fix lets clean names through.
	{
		const vector<string> config = { "" };
		NameFilter buggy, fixed;
		buggy.build(config, false);
		fixed.build(config, true);
		expect("buggy: empty entry rejects 'Friday Poker'", buggy.isBad("Friday Poker"), true);
		expect("buggy: empty entry rejects 'Poker Night'", buggy.isBad("Poker Night"), true);
		expect("fixed: empty entry accepts 'Friday Poker'", fixed.isBad("Friday Poker"), false);
		expect("fixed: empty entry accepts 'Poker Night'", fixed.isBad("Poker Night"), false);
	}

	// 2. The same regression with an empty entry alongside a real bad word.
	{
		const vector<string> config = { "shit", "" };
		NameFilter buggy, fixed;
		buggy.build(config, false);
		fixed.build(config, true);
		expect("buggy: blank row rejects clean name", buggy.isBad("Friendly Table"), true);
		expect("fixed: blank row accepts clean name", fixed.isBad("Friendly Table"), false);
		expect("fixed: blank row keeps 'shit' detection", fixed.isBad("what the SHIT"), true);
		expect("fixed: no false positive on 'dickinson'", fixed.isBad("dickinson"), false);
	}

	// 3. Regression origin: the old regex_match did not treat an empty pattern
	//    as matching every name. Switching to regex_search exposed this.
	{
		const vector<string> config = { "" };
		NameFilter old;
		old.build(config, false);
		expect("old regex_match: empty entry harmless", old.isBadOldMatch("Friday Poker"), false);
		expect("new regex_search: empty entry poisons", old.isBad("Friday Poker"), true);
	}

	// 4. Normal configuration (no empty entries) is unaffected by the fix.
	{
		const vector<string> config = { "shit", "fuck" };
		NameFilter fixed;
		fixed.build(config, true);
		expect("control: clean name accepted", fixed.isBad("Friday Poker"), false);
		expect("control: embedded bad word caught", fixed.isBad("what the shit"), true);
		expect("control: case-insensitive bad word caught", fixed.isBad("FUCK this"), true);
	}

	if (failures == 0) {
		std::printf("ALL TESTS PASSED\n");
		return 0;
	}
	std::printf("%d TEST(S) FAILED\n", failures);
	return 1;
}
