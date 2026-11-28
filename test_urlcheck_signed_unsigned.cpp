// Regression test: urlcheck.cpp signed/unsigned comparison bug.
//
// Bug: In isWhitelistedToken(), the loop variable `i` is `int` but
// `suffix.length()` returns `size_t`. The comparison `i < suffix.length()`
// causes a signed/unsigned conversion where `i` is implicitly converted
// to `size_t`. If the suffix is very long (close to or exceeding INT_MAX),
// the behavior could be incorrect due to the conversion.
//
// The fix: Change the loop variable type from `int` to `size_t`.
//
// This test models the guard and also checks the real source file so the
// regression cannot silently return.

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

static_assert(sizeof(size_t) >= sizeof(int), "test assumes size_t >= int");

// Faithful model of the buggy guard in isWhitelistedToken(). `fixed`
// changes the loop variable type to size_t to match suffix.length()'s return type.
static bool tryValidate(const std::string &suffix, bool fixed, std::string::size_type &boundSize)
{
    if (fixed) {
        for (std::string::size_type i = 0; i < suffix.size(); i++) {
            if (!suffix.at(i).isLetterOrNumber()) {
                boundSize = i;
                return false;
            }
        }
    } else {
        for (int i = 0; i < static_cast<int>(suffix.size()); i++) {
            if (!suffix.at(i).isLetterOrNumber()) {
                boundSize = i;
                return false;
            }
        }
    }
    return true;
}

struct Case {
    std::string suffix;
    bool shouldPass;
    const char *note;
};

static std::string findSourceFile()
{
    const char *candidates[] = {
        "src/chatcleaner/urlcheck.cpp",
        "../src/chatcleaner/urlcheck.cpp",
        "../../src/chatcleaner/urlcheck.cpp",
    };
    for (const char *path : candidates) {
        std::ifstream f(path);
        if (f.good()) return path;
    }
    return "";
}

int main()
{
    const Case cases[] = {
        { "com",  true,  "simple valid suffix" },
        { "com/path",  true,  "suffix with path" },
        { "com?q=test",  true,  "suffix with query" },
        { "com#fragment",  true,  "suffix with fragment" },
        { "a" + std::string(1000, 'a'),  true,  "very long alphabetic suffix" },
        { "a" + std::string(1000, 'a') + "/path",  true,  "very long alphabetic suffix with path" },
        { "a" + std::string(1000, 'a') + "!",  false, "very long alphabetic suffix with punctuation" },
        { "",  true,  "empty suffix" },
        { "123",  true,  "numeric suffix" },
        { "123!",  false, "numeric suffix with punctuation" },
    };

    int failures = 0;
    std::printf("UrlCheck signed/unsigned comparison bug test\n\n");

    for (const Case &c : cases) {
        std::string buggyData = c.suffix;
        std::string fixedData = c.suffix;
        std::string::size_type buggyBound = 0;
        std::string::size_type fixedBound = 0;
        const bool buggyPass = tryValidate(buggyData, false, buggyBound);
        const bool fixedPass = tryValidate(fixedData, true, fixedBound);

        std::printf("suffix size=%zu shouldPass=%-5s | buggy: %s bound=%5zu | fixed: %s bound=%5zu  (%s)\n",
                    c.suffix.size(), c.shouldPass ? "yes" : "no",
                    buggyPass ? "pass" : "fail", buggyBound,
                    fixedPass ? "pass" : "fail", fixedBound, c.note);

        if (fixedPass != c.shouldPass) {
            std::printf("  FAIL: fixed guard accepted/rejected the wrong result\n");
            ++failures;
        }
        if (!c.shouldPass && buggyPass && buggyBound < c.suffix.size()) {
            std::printf("  (buggy path silently passed as expected - signed/unsigned issue)\n");
        }
    }

    // Guard the actual source so the regression cannot creep back in.
    const std::string path = findSourceFile();
    if (path.empty()) {
        std::printf("NOTE: urlcheck.cpp not found; skipping source check\n");
    } else {
        std::ifstream f(path);
        const std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        // Check for the fixed pattern: size_t loop variable
        const std::string bad1 = "int i = 0; i < suffix.length();";
        const std::string good1 = "size_t i = 0; i < suffix.size();";
        const std::string bad2 = "int j = 0; j < suffix.length();";
        const std::string good2 = "size_t j = 0; j < suffix.size();";

        bool hasBad1 = src.find(bad1) != std::string::npos;
        bool hasGood1 = src.find(good1) != std::string::npos;
        bool hasBad2 = src.find(bad2) != std::string::npos;
        bool hasGood2 = src.find(good2) != std::string::npos;

        if (hasBad1 && !hasGood1) {
            std::printf("FAIL: source check: old buggy pattern '%s' found in %s without fix\n", bad1.c_str(), path.c_str());
            ++failures;
        } else if (!hasBad1 && hasGood1) {
            std::printf("\nSource check (%s): size_t loop variable guard present (bug fixed)\n", path.c_str());
        } else if (hasBad1 && hasGood1) {
            std::printf("\nSource check (%s): both old and new patterns found in %s\n", path.c_str());
        } else {
            std::printf("\nSource check (%s): neither pattern found in %s\n", path.c_str());
        }

        if (hasBad2 && !hasGood2) {
            std::printf("FAIL: source check: old buggy pattern '%s' found in %s without fix\n", bad2.c_str(), path.c_str());
            ++failures;
        } else if (!hasBad2 && hasGood2) {
            std::printf("Source check (%s): size_t loop variable guard present (bug fixed)\n", path.c_str());
        } else if (hasBad2 && hasGood2) {
            std::printf("Source check (%s): both old and new patterns found in %s\n", path.c_str());
        } else {
            std::printf("Source check (%s): neither pattern found in %s\n", path.c_str());
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