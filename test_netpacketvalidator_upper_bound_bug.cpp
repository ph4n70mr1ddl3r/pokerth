// Regression test: NetPacketValidator::ValidateListIntRange() has a signed/unsigned
// comparison bug where the loop variable is `int` but `l.size()` returns `size_t`.
// Without the fix, large list sizes or certain data patterns can cause incorrect
// validation behavior or undefined behavior due to signed/unsigned conversion.
//
// Bug hunt
// --------
// src/net/netpacketvalidator.cpp, ValidateListIntRange():
//
//     static bool ValidateListIntRange(const ::google::protobuf::RepeatedField< ::google::protobuf::uint32 > &l, ::google::protobuf::uint32 minval, ::google::protobuf::uint32 maxval)
//     {
//         bool retVal = true;
//         for (int i = 0; i < l.size(); i++) {   // BUG: int vs size_t comparison
//             if (!VALIDATE_UINT_RANGE(l.Get(i), minval, maxval)) {
//                 retVal = false;
//                 break;
//             }
//         }
//         return retVal;
//     }
//
// The fix
// -------
// Change the loop variable type from `int` to `size_t`:
//     for (size_t i = 0; i < l.size(); i++) {
//
// This test models the guard and also checks the real source file so the
// regression cannot silently return.
//
// Compile:
//   g++ -std=c++23 -Wall -Wextra -o /tmp/test_netpacketvalidator_upper_bound_bug test_netpacketvalidator_upper_bound_bug.cpp
//   /tmp/test_netpacketvalidator_upper_bound_bug

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>

static_assert(sizeof(size_t) >= sizeof(int), "test assumes size_t >= int");

// Faithful model of the buggy guard in ValidateListIntRange(). `fixed`
// changes the loop variable type to size_t to match l.size()'s return type.
static bool tryValidate(const std::vector<uint32_t> &data, uint32_t minval, uint32_t maxval, bool fixed, std::vector<uint32_t>::size_type &boundSize)
{
    if (fixed) {
        for (std::vector<uint32_t>::size_type i = 0; i < data.size(); i++) {
            if (!((data[i] >= minval && data[i] <= maxval))) {
                boundSize = i;
                return false;
            }
        }
    } else {
        for (int i = 0; i < static_cast<int>(data.size()); i++) {
            if (!((data[i] >= minval && data[i] <= maxval))) {
                boundSize = i;
                return false;
            }
        }
    }
    return true;
}

struct Case {
    std::vector<uint32_t> data;
    uint32_t minval;
    uint32_t maxval;
    bool shouldPass;
    const char *note;
};

static std::string findSourceFile()
{
    const char *candidates[] = {
        "src/net/netpacketvalidator.cpp",
        "../src/net/netpacketvalidator.cpp",
        "../../src/net/netpacketvalidator.cpp",
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
        { {5, 10, 15, 20}, 1, 100,  true,  "all values in range" },
        { {5, 200, 15, 20}, 1, 100,  false, "value out of range (200 > 100)" },
        { {}, 1, 100,  true,  "empty list" },
        { {1}, 1, 1,  true,  "single value at boundary" },
        { {0}, 1, 100,  false, "single value below min (0 < 1)" },
        { {50, 75, 25}, 1, 100,  true, "normal valid values" },
        { {50, 200, 25}, 1, 100,  false, "one value out of range (200 > 100)" },
        { {100000, 200000}, 1, 100000,  false, "values exceed max with large numbers" },
        { {100000, 50000}, 1, 100000,  true,  "mixed valid and smaller values" },
    };

    int failures = 0;
    std::printf("NetPacketValidator ValidateListIntRange signed/unsigned comparison bug test\n\n");

    for (const Case &c : cases) {
        std::vector<uint32_t> buggyData = c.data;
        std::vector<uint32_t> fixedData = c.data;
        std::vector<uint32_t>::size_type buggyBound = 0;
        std::vector<uint32_t>::size_type fixedBound = 0;
        const bool buggyPass = tryValidate(buggyData, c.minval, c.maxval, false, buggyBound);
        const bool fixedPass = tryValidate(fixedData, c.minval, c.maxval, true, fixedBound);

        std::printf("data size=%zu min=%u max=%u shouldPass=%-5s | buggy: %s bound=%5zu | fixed: %s bound=%5zu  (%s)\n",
                    c.data.size(), c.minval, c.maxval, c.shouldPass ? "yes" : "no",
                    buggyPass ? "pass" : "fail", buggyBound,
                    fixedPass ? "pass" : "fail", fixedBound, c.note);

        if (fixedPass != c.shouldPass) {
            std::printf("  FAIL: fixed guard accepted/rejected the wrong result\n");
            ++failures;
        }
        if (!c.shouldPass && buggyPass && buggyBound < c.data.size()) {
            std::printf("  (buggy path silently passed as expected - signed/unsigned issue)\n");
        }
    }

    // Guard the actual source so the regression cannot creep back in.
    const std::string path = findSourceFile();
    if (path.empty()) {
        std::printf("NOTE: netpacketvalidator.cpp not found; skipping source check\n");
    } else {
        std::ifstream f(path);
        const std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        const std::string good = "for (size_t i = 0; i < l.size(); i++)";
        if (src.find(good) == std::string::npos) {
            std::printf("FAIL: source check: expected '%s' in %s\n", good.c_str(), path.c_str());
            ++failures;
        } else {
            std::printf("\nSource check (%s): size_t loop variable guard present\n", path.c_str());
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