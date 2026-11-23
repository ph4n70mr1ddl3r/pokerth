// Test for CapsFloodCheck false positive caused by stripping all spaces.
//
// Bug hunt: CapsFloodCheck::run() simplified the message and then called
// remove(" ") before matching [A-Z]{N,}. Removing every space merges
// separate words into one long uppercase run. With the default threshold of
// 10, a perfectly ordinary message made of short capitalized words/acronyms
// such as "NASA ESA CNN" becomes "NASAESACNN" (10 consecutive capitals) and
// is falsely reported as caps flooding, even though no single word contains
// more than four consecutive capitals.
//
// The fix validates each whitespace-delimited token on its own, while still
// recognising the spaced-out-caps evasion pattern ("H E L L O") by counting
// consecutive single-letter uppercase tokens as one run.
//
// Compile:
//   g++ -std=c++23 -Wall -Wextra -o /tmp/test_capsflood_word_boundary test_capsflood_word_boundary.cpp
//   /tmp/test_capsflood_word_boundary

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

static std::vector<std::string> splitWords(const std::string &s)
{
	std::vector<std::string> words;
	std::string current;
	for (char c : s) {
		if (c == ' ') {
			if (!current.empty()) {
				words.push_back(current);
				current.clear();
			}
		} else {
			current.push_back(c);
		}
	}
	if (!current.empty()) words.push_back(current);
	return words;
}

static bool isUpper(char c) { return c >= 'A' && c <= 'Z'; }
static bool isPunct(char c)
{
	return c == '.' || c == ',' || c == ';' || c == ':' || c == '!' || c == '?'
		|| c == ')' || c == ']' || c == '}' || c == '\'' || c == '"';
}

// The BUGGY implementation: simplify, strip every space, then look for a run
// of N consecutive uppercase letters over the whole remaining string.
static bool capsFloodBuggy(std::string msg, int threshold)
{
	if (threshold < 1) return false;
	std::string stripped;
	for (char c : msg) {
		if (c != ' ') stripped.push_back(c);
	}
	int run = 0;
	for (char c : stripped) {
		if (isUpper(c)) {
			if (++run >= threshold) return true;
		} else {
			run = 0;
		}
	}
	return false;
}

// The FIXED implementation: check each token independently. A sequence of
// single-letter uppercase tokens is treated as one run so spaced-out caps
// ("H E L L O") are still caught.
static bool capsFloodFixed(std::string msg, int threshold)
{
	if (threshold < 1) return false;
	int maxRun = 0;
	int singleLetterRun = 0;
	for (const std::string &token : splitWords(msg)) {
		int run = 0;
		int tokenMax = 0;
		int upperCount = 0;
		bool onlyUpperOrPunct = true;
		for (char c : token) {
			if (isUpper(c)) {
				++run;
				++upperCount;
				tokenMax = std::max(tokenMax, run);
			} else if (isPunct(c)) {
				run = 0;
			} else {
				run = 0;
				onlyUpperOrPunct = false;
			}
		}
		maxRun = std::max(maxRun, tokenMax);
		if (upperCount == 1 && onlyUpperOrPunct) {
			++singleLetterRun;
			maxRun = std::max(maxRun, singleLetterRun);
		} else {
			singleLetterRun = 0;
		}
	}
	return maxRun >= threshold;
}

struct Case {
	const char *message;
	int threshold;
	bool expected;
	const char *description;
};

int main()
{
	const Case cases[] = {
		{ "ABCDEFGHIJ", 10, true, "single word with 10 caps -> flag" },
		{ "NASA ESA CNN", 10, false, "three short acronyms must NOT flag (BUG)" },
		{ "ABCD EFGH IJ", 10, false, "three short caps words must NOT flag (BUG)" },
		{ "NASA ESA CNN.", 10, false, "acronyms with sentence punctuation must NOT flag (BUG)" },
		{ "H E L L O W O R L D", 10, true, "spaced-out caps evasion must still flag" },
		{ "hello world", 10, false, "lowercase text -> no flag" },
		{ "OK NASA", 10, false, "too few caps -> no flag" },
		{ "NASA ESA", 8, false, "8 caps across two words -> no flag" },
		{ "HELLO, WORLD!", 5, true, "long caps word with punctuation -> flag" },
		{ "A", 1, true, "threshold 1 -> single cap flags" },
	};

	int failures = 0;
	printf("CapsFloodCheck word-boundary regression test\n\n");

	for (const Case &c : cases) {
		bool buggy = capsFloodBuggy(c.message, c.threshold);
		bool fixed = capsFloodFixed(c.message, c.threshold);
		printf("msg=\"%s\" threshold=%d expected=%s buggy=%s fixed=%s  %s\n",
			c.message, c.threshold, c.expected ? "true" : "false",
			buggy ? "true" : "false", fixed ? "true" : "false", c.description);
		if (fixed != c.expected) {
			printf("  FAIL: fixed implementation returned wrong result\n");
			++failures;
		}
	}

	printf("\n");
	if (failures == 0) {
		printf("ALL TESTS PASSED\n");
		return 0;
	}
	printf("TESTS FAILED: %d failures\n", failures);
	return 1;
}
