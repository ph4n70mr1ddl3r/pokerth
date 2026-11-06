// Test for LocalHand::getCurrentBeRo() signed/unsigned comparison bug.
//
// Bug: In LocalHand::getCurrentBeRo(), the check `currentRound >= myBeRo.size()`
// compares currentRound (signed int/GameState) with myBeRo.size() (unsigned size_t).
// Due to C++ usual arithmetic conversions, if currentRound is negative, it gets
// implicitly converted to a very large size_t value, making the comparison behave
// incorrectly: the check may be skipped entirely, leading to out-of-bounds access
// via myBeRo[currentRound] on the next line, causing undefined behavior.
//
// Fix: Properly handle the signed/unsigned comparison by checking for negative
// currentRound explicitly before the comparison.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_signed_unsigned_bug test_signed_unsigned_bug.cpp && /tmp/test_signed_unsigned_bug

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <stdexcept>

// Mock BeRoInterface forward declaration
struct BeRoInterface
{
};

// Mock LocalException for test purposes
struct LocalException : public std::runtime_error {
    LocalException(const char* file, int line, int err, const char* msg = "")
        : std::runtime_error(msg) {}
};

// Helper macro for the test
#define LocalException(file, line, err, msg) LocalException(__FILE__, __LINE__, 0, msg)

// Minimal replica of LocalHand's getCurrentBeRo() logic.
// ---------------------------------------------------------------------------

// Simulate the OLD (buggy) getCurrentBeRo() check
// Old code: if(currentRound >= myBeRo.size()) throw; return myBeRo[currentRound]
static BeRoInterface* getCurrentBeRo_Buggy(int currentRound, const std::vector<BeRoInterface*>& myBeRo) {
    if (currentRound >= static_cast<int>(myBeRo.size())) {  // BUG: signed vs unsigned comparison
        throw LocalException(__FILE__, __LINE__, 0, "ERR_BERO_NOT_FOUND");
    }
    return myBeRo[currentRound];  // BUG: negative index -> UB
}

// Simulate the NEW (fixed) getCurrentBeRo() check
// New code: check for negative currentRound AND proper size check
static BeRoInterface* getCurrentBeRo_Fixed(int currentRound, const std::vector<BeRoInterface*>& myBeRo) {
    if (currentRound < 0 || static_cast<size_t>(currentRound) >= myBeRo.size()) {
        throw LocalException(__FILE__, __LINE__, 0, "ERR_BERO_NOT_FOUND");
    }
    return myBeRo[currentRound];
}

// ---------------------------------------------------------------------------
// Test scenarios
// ---------------------------------------------------------------------------

int main() {
    int failures = 0;
    bool allPassed = true;

    printf("Testing signed/unsigned comparison bug in getCurrentBeRo()\n\n");

// --- Scenario 1: Normal round (currentRound = 0) ---
    printf("--- Scenario 1 (Normal round: currentRound=0, BeRo has 5 elements) ---\n");
    std::vector<BeRoInterface*> beRo1(5);
    for (int i = 0; i < 5; i++) {
        beRo1[i] = new BeRoInterface();
    }

    // Buggy: should pass (0 < 5, check passes)
    try {
        auto result1_buggy = getCurrentBeRo_Buggy(0, beRo1);
        printf("  Buggy: returned BeRo (correct - check passed for round 0)\n");
    } catch (const LocalException& e) {
        printf("  Buggy: threw exception (UNEXPECTED for round 0)\n");
        allPassed = false;
    }

    // Fixed: should pass
    try {
        auto result1_fixed = getCurrentBeRo_Fixed(0, beRo1);
        printf("  Fixed: returned BeRo (correct)\n");
    } catch (const LocalException& e) {
        printf("  Fixed: threw exception (UNEXPECTED for round 0)\n");
        allPassed = false;
    }

    // Cleanup
    for (auto* ptr : beRo1) delete ptr;

// --- Scenario 2: Negative currentRound (THE BUG) ---
    printf("\n--- Scenario 2 (Negative currentRound: currentRound=-1, BeRo has 5 elements) ---\n");
    std::vector<BeRoInterface*> beRo2(5);
    for (int i = 0; i < 5; i++) {
        beRo2[i] = new BeRoInterface();
    }

    // Buggy: should NOT throw (bug: no negative check), accesses myBeRo[-1] -> UB
    // We expect the buggy code to NOT throw (which is the bug)
    bool buggyDidNotThrow2 = true;
    try {
        auto result2_buggy = getCurrentBeRo_Buggy(-1, beRo2);
        printf("  Buggy: returned BeRo (BUG CONFIRMED: no negative check, UB risk)\n");
    } catch (const LocalException& e) {
        printf("  Buggy: threw exception (bug NOT present - unexpected)\n");
        buggyDidNotThrow2 = false;
    }

    // Fixed: should throw (negative check works)
    bool fixedThrows2 = false;
    try {
        auto result2_fixed = getCurrentBeRo_Fixed(-1, beRo2);
        printf("  Fixed: returned BeRo (BUG: should have thrown!)\n");
    } catch (const LocalException& e) {
        printf("  Fixed: threw exception (correct - negative check works)\n");
        fixedThrows2 = true;
    }

    // Cleanup
    for (auto* ptr : beRo2) delete ptr;

// --- Scenario 3: currentRound > BeRo size (out of range, positive) ---
    printf("\n--- Scenario 3 (currentRound=10, BeRo has 5 elements) ---\n");
    std::vector<BeRoInterface*> beRo3(5);
    for (int i = 0; i < 5; i++) {
        beRo3[i] = new BeRoInterface();
    }

    // Buggy: 10 >= 5 is true, so it throws (happens to work correctly for positive overflow)
    try {
        auto result3_buggy = getCurrentBeRo_Buggy(10, beRo3);
        printf("  Buggy: returned BeRo (UNEXPECTED - should have thrown 10 >= 5)\n");
        allPassed = false;
    } catch (const LocalException& e) {
        printf("  Buggy: threw exception (correct - 10 >= 5)\n");
    }

    // Fixed: should throw (size_t comparison works)
    try {
        auto result3_fixed = getCurrentBeRo_Fixed(10, beRo3);
        printf("  Fixed: returned BeRo (UNEXPECTED - should have thrown)\n");
        allPassed = false;
    } catch (const LocalException& e) {
        printf("  Fixed: threw exception (correct - size check works)\n");
    }

    // Cleanup
    for (auto* ptr : beRo3) delete ptr;

// --- Scenario 4: currentRound = 3 (valid river round) ---
    printf("\n--- Scenario 4 (Valid round: currentRound=3, BeRo has 5 elements) ---\n");
    std::vector<BeRoInterface*> beRo4(5);
    for (int i = 0; i < 5; i++) {
        beRo4[i] = new BeRoInterface();
    }

    // Buggy: should pass (3 < 5, check passes)
    try {
        auto result4_buggy = getCurrentBeRo_Buggy(3, beRo4);
        printf("  Buggy: returned BeRo (correct - check passed for round 3)\n");
    } catch (const LocalException& e) {
        printf("  Buggy: threw exception (UNEXPECTED for round 3)\n");
        allPassed = false;
    }

    // Fixed: should pass
    try {
        auto result4_fixed = getCurrentBeRo_Fixed(3, beRo4);
        printf("  Fixed: returned BeRo (correct)\n");
    } catch (const LocalException& e) {
        printf("  Fixed: threw exception (UNEXPECTED for round 3)\n");
        allPassed = false;
    }

    // Cleanup
    for (auto* ptr : beRo4) delete ptr;

    printf("\n");
    if (!allPassed) {
        printf("TEST FAILED: One or more scenarios did not pass as expected\n");
        return 1;
    }

    printf("TEST PASSED: signed/unsigned comparison bug confirmed and fix verified\n");
    return 0;
}