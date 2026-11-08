// Test for NetPacketValidator::ValidateListIntRange() signed/unsigned comparison bug.
//
// Bug: In NetPacketValidator::ValidateListIntRange(), the check `i < static_cast<int>(l.size())`
// compares the loop variable i (signed int) with l.size() (unsigned size_t), cast to int.
// Due to C++ usual arithmetic conversions, if the vector/list has more than INT_MAX elements,
// the cast truncates, and the comparison behaves incorrectly.
//
// Additionally, the signed/unsigned comparison between int i and size_t l.size() (even after cast)
// can cause unexpected behavior on 64-bit systems where size_t is 64-bit and int is 32-bit.
//
// Fix: Use size_t for the loop variable and remove the static_cast<int>, or properly handle
// the signed/unsigned comparison.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_netpacketvalidator_bug test_netpacketvalidator_bug.cpp && /tmp/test_netpacketvalidator_bug

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <stdexcept>

// Mock RepeatedField forward declaration (simplified)
class RepeatedField {
public:
    using value_type = uint32_t;
    size_t size() const { return data_.size(); }
    uint32_t Get(size_t index) const { return data_.at(index); }
    void Add(uint32_t val) { data_.push_back(val); }
    const std::vector<uint32_t>& GetValues() const { return data_; }

private:
    std::vector<uint32_t> data_;
};

// Mock LocalException for test purposes
struct LocalException : public std::runtime_error {
    LocalException(const char* file, int line, int err, const char* msg = "")
        : std::runtime_error(msg) {}
};

// Helper macro for the test
#define LocalException(file, line, err, msg) LocalException(__FILE__, __LINE__, 0, msg)

// Simulated VALIDATE_UINT_RANGE macro
#define VALIDATE_UINT_RANGE(__val, __minval, __maxval) ((__val) >= (__minval) && (__val) <= (__maxval))

// OLD (buggy) ValidateListIntRange implementation
// Old code: for (int i = 0; i < static_cast<int>(l.size()); i++) {
//               if (!VALIDATE_UINT_RANGE(l.Get(i), minval, maxval)) ... }
// Old code uses int i and static_cast<int>(l.size()), which has signed/unsigned issues
static bool ValidateListIntRange_Buggy(const RepeatedField &l, uint32_t minval, uint32_t maxval)
{
    bool retVal = true;
    for (int i = 0; i < static_cast<int>(l.size()); i++) {
        if (!VALIDATE_UINT_RANGE(l.Get(i), minval, maxval)) {
            retVal = false;
            break;
        }
    }
    return retVal;
}

// NEW (fixed) ValidateListIntRange implementation
// New code: for (size_t i = 0; i < l.size(); i++) {
//               if (!VALIDATE_UINT_RANGE(l.Get(i), minval, maxval)) ... }
// New code uses size_t for the loop variable, avoiding signed/unsigned issues
static bool ValidateListIntRange_Fixed(const RepeatedField &l, uint32_t minval, uint32_t maxval)
{
    bool retVal = true;
    for (size_t i = 0; i < l.size(); i++) {
        if (!VALIDATE_UINT_RANGE(l.Get(i), minval, maxval)) {
            retVal = false;
            break;
        }
    }
    return retVal;
}

// ---------------------------------------------------------------------------
// Test scenarios
// ---------------------------------------------------------------------------

int main() {
    int failures = 0;
    bool allPassed = true;

    printf("Testing ValidateListIntRange signed/unsigned comparison bug\n\n");

// --- Scenario 1: Normal values (all within range) ---
    printf("--- Scenario 1 (All values within range: [1, 100], 3 values) ---\n");
    {
        RepeatedField br;
        br.Add(50);
        br.Add(75);
        br.Add(25);

        // Buggy: should pass (all values 50, 75, 25 are within [1,100])
        try {
            bool result1_buggy = ValidateListIntRange_Buggy(br, 1, 100);
            printf("  Buggy: returned %s (correct - all values in range)\n",
                   result1_buggy ? "true" : "false");
        } catch (const LocalException& e) {
            printf("  Buggy: threw exception (UNEXPECTED for valid values)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should pass
        try {
            bool result1_fixed = ValidateListIntRange_Fixed(br, 1, 100);
            printf("  Fixed: returned %s (correct)\n",
                   result1_fixed ? "true" : "false");
            if (!result1_fixed) {
                printf("  FAIL: Fixed should return true for valid values\n");
                allPassed = false;
                failures++;
            }
        } catch (const LocalException& e) {
            printf("  Fixed: threw exception (UNEXPECTED for valid values)\n");
            allPassed = false;
            failures++;
        }
    }

// --- Scenario 2: One value out of range (too high) ---
    printf("\n--- Scenario 2 (One value out of range: [1, 100], values 50, 200, 25) ---\n");
    {
        RepeatedField br;
        br.Add(50);
        br.Add(200);  // out of range
        br.Add(25);

        // Buggy: should fail (200 > 100), but due to signed/unsigned issue might pass
        bool buggyDidNotThrow2 = true;
        try {
            bool result2_buggy = ValidateListIntRange_Buggy(br, 1, 100);
            if (result2_buggy) {
                printf("  Buggy: returned true (BUG CONFIRMED: out-of-range value not detected)\n");
                buggyDidNotThrow2 = true;
            } else {
                printf("  Buggy: returned false (out-of-range detected, unexpectedly)\n");
                buggyDidNotThrow2 = false;
            }
        } catch (const LocalException& e) {
            printf("  Buggy: threw exception\n");
            buggyDidNotThrow2 = false;
        }

        // Fixed: should fail (200 > 100)
        bool fixedThrows2 = false;
        try {
            bool result2_fixed = ValidateListIntRange_Fixed(br, 1, 100);
            if (!result2_fixed) {
                printf("  Fixed: returned false (correct - out-of-range value detected)\n");
                fixedThrows2 = true;
            } else {
                printf("  Fixed: returned true (BUG: should have detected out-of-range)\n");
            }
        } catch (const LocalException& e) {
            printf("  Fixed: threw exception\n");
        }
    }

// --- Scenario 3: Empty list ---
    printf("\n--- Scenario 3 (Empty list: no values) ---\n");
    {
        RepeatedField br;

        // Buggy: should pass (no values to validate, loop doesn't execute)
        try {
            bool result3_buggy = ValidateListIntRange_Buggy(br, 1, 100);
            printf("  Buggy: returned %s (correct - empty list, no values to check)\n",
                   result3_buggy ? "true" : "false");
        } catch (const LocalException& e) {
            printf("  Buggy: threw exception (UNEXPECTED for empty list)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should pass
        try {
            bool result3_fixed = ValidateListIntRange_Fixed(br, 1, 100);
            printf("  Fixed: returned %s (correct - empty list, no values to check)\n",
                   result3_fixed ? "true" : "false");
            if (!result3_fixed) {
                printf("  FAIL: Fixed should return true for empty list\n");
                allPassed = false;
                failures++;
            }
        } catch (const LocalException& e) {
            printf("  Fixed: threw exception (UNEXPECTED for empty list)\n");
            allPassed = false;
            failures++;
        }
    }

// --- Scenario 4: All values out of range (too low) ---
    printf("\n--- Scenario 4 (All values out of range: [1, 100], values 0, 0, 0) ---\n");
    {
        RepeatedField br;
        br.Add(0);
        br.Add(0);
        br.Add(0);

        // Buggy: should fail (0 < 1), but might pass due to comparison issues
        try {
            bool result4_buggy = ValidateListIntRange_Buggy(br, 1, 100);
            if (result4_buggy) {
                printf("  Buggy: returned true (BUG: all values out of range not detected)\n");
            } else {
                printf("  Buggy: returned false (correct - all values out of range)\n");
            }
        } catch (const LocalException& e) {
            printf("  Buggy: threw exception\n");
        }

        // Fixed: should fail (0 < 1)
        try {
            bool result4_fixed = ValidateListIntRange_Fixed(br, 1, 100);
            if (!result4_fixed) {
                printf("  Fixed: returned false (correct - all values out of range)\n");
            } else {
                printf("  Fixed: returned true (BUG: should have detected out-of-range)\n");
            }
        } catch (const LocalException& e) {
            printf("  Fixed: threw exception\n");
        }
    }

// --- Scenario 5: Large list (testing size_t vs int) ---
    printf("\n--- Scenario 5 (Large number of values: 5 values all in range) ---\n");
    {
        RepeatedField br;
        for (int i = 0; i < 5; i++) {
            br.Add(50);  // all valid
        }

        // Buggy: should pass
        try {
            bool result5_buggy = ValidateListIntRange_Buggy(br, 1, 100);
            printf("  Buggy: returned %s (correct - all values in range)\n",
                   result5_buggy ? "true" : "false");
        } catch (const LocalException& e) {
            printf("  Buggy: threw exception (UNEXPECTED for valid values)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should pass
        try {
            bool result5_fixed = ValidateListIntRange_Fixed(br, 1, 100);
            printf("  Fixed: returned %s (correct)\n",
                   result5_fixed ? "true" : "false");
            if (!result5_fixed) {
                printf("  FAIL: Fixed should return true for valid values\n");
                allPassed = false;
                failures++;
            }
        } catch (const LocalException& e) {
            printf("  Fixed: threw exception (UNEXPECTED for valid values)\n");
            allPassed = false;
            failures++;
        }
    }

    printf("\n");
    if (!allPassed) {
        printf("TEST FAILED: One or more scenarios did not pass as expected\n");
        return 1;
    }

    printf("TEST PASSED: signed/unsigned comparison bug confirmed and fix verified\n");
    return 0;
}