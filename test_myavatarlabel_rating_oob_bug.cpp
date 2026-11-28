// Regression test: MyAvatarLabel::getPlayerRating() out-of-bounds star write.
//
// Bug: getPlayerRating() returns the unchecked result of QString::toInt() on
// the user-editable "PlayerTooltips" config value. refreshStars() then uses
// that value as the upper bound of `for(int i=1; i<=playerStars; i++)` and
// indexes myW->playerStarsArray[i][seatPlace]. The array has 6 rows but only
// rows [1..5] are ever initialised (gametableimpl.cpp), so a rating greater
// than 5 makes setText() run on an out-of-bounds QLabel* -> undefined
// behaviour / crash. A malformed or hand-edited config entry is enough to
// trigger it.
//
// The fix: clamp the parsed rating to the valid [0,5] range in
// getPlayerRating() before returning it (the sole caller is refreshStars()).
//
// This test models the buggy vs. fixed bounds logic and also guards the real
// source file so the regression cannot silently return.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

static int parseRating(const std::string &text, bool fixed)
{
    int rating = std::atoi(text.c_str());
    if (fixed) {
        rating = std::clamp(rating, 0, 5);
    }
    return rating;
}

// Mirrors refreshStars(): it touches playerStarsArray[i] for every i in
// [1, rating]. Report whether that would step outside the initialised [1,5].
static bool wouldAccessOutOfBounds(int rating)
{
    for (int i = 1; i <= rating; ++i) {
        if (i < 1 || i > 5) {
            return true;
        }
    }
    return false;
}

static std::string findSourceFile()
{
    const char *candidates[] = {
        "src/gui/qt/gametable/myavatarlabel.cpp",
        "../src/gui/qt/gametable/myavatarlabel.cpp",
        "../../src/gui/qt/gametable/myavatarlabel.cpp",
    };
    for (const char *path : candidates) {
        std::ifstream f(path);
        if (f.good()) return path;
    }
    return "";
}

struct Case {
    const char *configValue;
    int expectedRating;
};

int main()
{
    const Case cases[] = {
        { "0",   0 },  // no stars
        { "1",   1 },  // minimum rating
        { "5",   5 },  // maximum valid rating
        { "6",   5 },  // one past the end of playerStarsArray[6][N]
        { "100", 5 },  // badly corrupted / hand-edited config
        { "-1",  0 },  // negative value: loop would not run, but clamp for safety
        { "abc", 0 },  // non-numeric: toInt()/atoi() yields 0
        { "3x",  3 },  // trailing garbage: numeric prefix
    };

    int failures = 0;
    std::printf("MyAvatarLabel::getPlayerRating() out-of-bounds star write test\n\n");

    for (const Case &c : cases) {
        const int buggyRating = parseRating(c.configValue, false);
        const int fixedRating = parseRating(c.configValue, true);

        std::printf("config=\"%-4s\" | buggy rating=%4d oob=%-5s | fixed rating=%d oob=%s\n",
                    c.configValue, buggyRating,
                    wouldAccessOutOfBounds(buggyRating) ? "YES" : "no",
                    fixedRating,
                    wouldAccessOutOfBounds(fixedRating) ? "YES" : "no");

        if (fixedRating != c.expectedRating) {
            std::printf("  FAIL: fixed rating should be %d, got %d\n", c.expectedRating, fixedRating);
            ++failures;
        }
        if (wouldAccessOutOfBounds(fixedRating)) {
            std::printf("  FAIL: fixed rating would access playerStarsArray out of bounds\n");
            ++failures;
        }
    }

    // Guard the actual source so the regression cannot creep back in.
    const std::string path = findSourceFile();
    if (path.empty()) {
        std::printf("\nNOTE: myavatarlabel.cpp not found; skipping source check\n");
    } else {
        std::ifstream f(path);
        const std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        const std::string goodFix = "return std::clamp(result.toInt(), 0, 5);";
        const std::string badCode = "return result.toInt();";
        const std::string goodInclude = "#include <algorithm>";

        const bool hasFix = src.find(goodFix) != std::string::npos;
        const bool hasBad = src.find(badCode) != std::string::npos;
        const bool hasInclude = src.find(goodInclude) != std::string::npos;

        std::printf("\nSource check (%s)\n", path.c_str());
        if (hasFix) {
            std::printf("  PASS: clamped rating '%s' found\n", goodFix.c_str());
        } else {
            std::printf("  FAIL: clamped rating '%s' NOT found\n", goodFix.c_str());
            ++failures;
        }
        if (!hasBad) {
            std::printf("  PASS: old unclamped '%s' is gone\n", badCode.c_str());
        } else {
            std::printf("  FAIL: old unclamped '%s' is still present\n", badCode.c_str());
            ++failures;
        }
        if (hasInclude) {
            std::printf("  PASS: '<algorithm>' include present for std::clamp\n");
        } else {
            std::printf("  FAIL: '<algorithm>' include missing\n");
            ++failures;
        }
    }

    std::printf("\n");
    if (failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("TESTS FAILED: %d failures\n", failures);
    return 1;
}
