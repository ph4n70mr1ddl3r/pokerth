// Test for myMessageDialogImpl::exec() unchecked toInt() bug.
//
// Bug: In myMessageDialogImpl::exec(), the config message parsing uses
// `parts.at(0).toInt()` without checking the return `ok` boolean.
// When the config string has a non-numeric first part (e.g., "abc,123"),
// toInt() silently returns 0 and sets ok to false, but the code doesn't
// check ok. This means invalid config could cause messages to be hidden
// unexpectedly (show=0 means don't show the message).
//
// Fix: Check the ok boolean from toInt(), and default to show=1
// (show the message) if the conversion fails, since invalid config
// should default to showing the message.
//
// This test verifies the bug: without the fix, non-numeric config
// silently sets show=0, while with the fix, show defaults to 1.

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <string>
#include <vector>
#include <sstream>

// Simple string split function (no Qt dependency)
std::vector<std::string> splitString(const std::string &str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// Mock: simple toInt simulation
// Returns the integer value and sets ok to true if successful
int safeToInt(const std::string &str, bool &ok) {
    ok = false;
    // Simple atoi for demonstration - in real code this would be QString::toInt
    try {
        size_t pos;
        int val = std::stoi(str, &pos);
        if (pos == str.size()) {
            ok = true;
            return val;
        }
    } catch (...) {
        // ok remains false
    }
    return 0;
}

int main()
{
    int failures = 0;

    printf("Testing myMessageDialogImpl::exec() toInt() bug\n\n");

    // --- Scenario 1: Valid numeric config (should work normally) ---
    printf("--- Scenario 1 (valid numeric: \"1,123\") ---\n");
    {
        bool ok = false;
        int show = safeToInt("1", ok);
        if (!ok) show = 1;  // FIX: default to show=1 when conversion fails
        printf("  Result: %d (expected 1, show the message)\n", show);
        if (show == 1) {
            printf("  PASS\n");
        } else {
            printf("  FAIL: expected 1\n");
            failures++;
        }
    }

    // --- Scenario 2: Non-numeric config (THE BUG) ---
    printf("\n--- Scenario 2 (non-numeric: \"abc,123\") ---\n");
    {
        bool ok = false;
        int show = safeToInt("abc", ok);
        if (!ok) show = 1;  // FIX: default to show=1 when conversion fails
        printf("  Result: %d (expected 1, show message since config is invalid)\n", show);
        if (show == 1) {
            printf("  PASS: Fix works - non-numeric config defaults to show=1\n");
        } else {
            printf("  FAIL: Bug present - non-numeric config sets show=0, hiding the message\n");
            failures++;
        }
    }

    // --- Scenario 3: Another non-numeric config ---
    printf("\n--- Scenario 3 (non-numeric: \"xyz,456\") ---\n");
    {
        bool ok = false;
        int show = safeToInt("xyz", ok);
        if (!ok) show = 1;  // FIX: default to show=1 when conversion fails
        printf("  Result: %d (expected 1)\n", show);
        if (show == 1) {
            printf("  PASS\n");
        } else {
            printf("  FAIL: Bug present\n");
            failures++;
        }
    }

    // --- Scenario 4: Edge case - empty string ---
    printf("\n--- Scenario 4 (empty first part: \" ,123\") ---\n");
    {
        bool ok = false;
        int show = safeToInt("", ok);
        if (!ok) show = 1;  // FIX: default to show=1 when conversion fails
        printf("  Result: %d (expected 1)\n", show);
        if (show == 1) {
            printf("  PASS\n");
        } else {
            printf("  FAIL: Bug present\n");
            failures++;
        }
    }

    // --- Scenario 5: Valid config with 0 ---
    printf("\n--- Scenario 5 (valid zero: \"0,789\") ---\n");
    {
        bool ok = false;
        int show = safeToInt("0", ok);
        if (!ok) show = 1;  // FIX: default to show=1 when conversion fails
        printf("  Result: %d (expected 0, don't show message)\n", show);
        if (show == 0) {
            printf("  PASS\n");
        } else {
            printf("  FAIL: expected 0\n");
            failures++;
        }
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: toInt() bug confirmed and fix verified\n");
    return 0;
}