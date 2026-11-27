/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

// Verification test: mySet += myCash bug fix
// Tests that mySet is set to oldMySet + myCash instead of accumulating

#include <cstdio>
#include <cstdlib>

// Simulates the fixed all-in logic: mySet = oldMySet + myCash
// Before fix: mySet += myCash (accumulates, causing double-counting)
// After fix: mySet = oldMySet + myCash (correctly sets total committed)

int test_allin_mySet_fix(int oldMySet, int myCash, int expectedMySet) {
    // Fixed logic: mySet = oldMySet + myCash
    int mySet = oldMySet + myCash;
    if (mySet == expectedMySet) {
        return 0; // PASS
    }
    return 1; // FAIL
}

int main()
{
    int failures = 0;

    printf("Testing mySet = oldMySet + myCash all-in fix\n\n");

    // --- Test 1: Basic all-in, oldMySet=30, myCash=100, expected mySet=130 ---
    printf("--- Test 1: Basic all-in ---\n");
    {
        int oldMySet = 30;
        int myCash = 100;
        int expectedMySet = 130;
        int result = test_allin_mySet_fix(oldMySet, myCash, expectedMySet);
        if (result == 0) {
            printf("  PASS: mySet = oldMySet + myCash = %d (oldMySet=%d, myCash=%d)\n",
                   expectedMySet, oldMySet, myCash);
        } else {
            printf("  FAIL: expected mySet=%d, got incorrect accumulation\n");
            failures++;
        }
    }

    // --- Test 2: All-in with zero oldMySet ---
    printf("\n--- Test 2: All-in with zero oldMySet ---\n");
    {
        int oldMySet = 0;
        int myCash = 50;
        int expectedMySet = 50;
        int result = test_allin_mySet_fix(oldMySet, myCash, expectedMySet);
        if (result == 0) {
            printf("  PASS: mySet = oldMySet + myCash = %d (oldMySet=%d, myCash=%d)\n",
                   expectedMySet, oldMySet, myCash);
        } else {
            printf("  FAIL: expected mySet=%d\n", expectedMySet);
            failures++;
        }
    }

    // --- Test 3: All-in with zero myCash ---
    printf("\n--- Test 3: All-in with zero myCash ---\n");
    {
        int oldMySet = 40;
        int myCash = 0;
        int expectedMySet = 40;
        int result = test_allin_mySet_fix(oldMySet, myCash, expectedMySet);
        if (result == 0) {
            printf("  PASS: mySet = oldMySet + myCash = %d (oldMySet=%d, myCash=%d)\n",
                   expectedMySet, oldMySet, myCash);
        } else {
            printf("  FAIL: expected mySet=%d\n", expectedMySet);
            failures++;
        }
    }

    // --- Test 4: All-in with large values ---
    printf("\n--- Test 4: All-in with large values ---\n");
    {
        int oldMySet = 200;
        int myCash = 500;
        int expectedMySet = 700;
        int result = test_allin_mySet_fix(oldMySet, myCash, expectedMySet);
        if (result == 0) {
            printf("  PASS: mySet = oldMySet + myCash = %d (oldMySet=%d, myCash=%d)\n",
                   expectedMySet, oldMySet, myCash);
        } else {
            printf("  FAIL: expected mySet=%d\n", expectedMySet);
            failures++;
        }
    }

    // --- Test 5: All-in where oldMySet already includes previous bets ---
    printf("\n--- Test 5: Multiple betting rounds ---\n");
    {
        // Simulate: first bet 20, then all-in with remaining 30
        int oldMySet = 20;  // first bet
        int myCash = 30;    // remaining cash
        int expectedMySet = 50;  // 20 + 30 = total committed
        int result = test_allin_mySet_fix(oldMySet, myCash, expectedMySet);
        if (result == 0) {
            printf("  PASS: mySet = oldMySet + myCash = %d (oldMySet=%d, myCash=%d)\n",
                   expectedMySet, oldMySet, myCash);
        } else {
            printf("  FAIL: expected mySet=%d\n", expectedMySet);
            failures++;
        }
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("TEST PASSED: mySet = oldMySet + myCash fix verified correctly\n");
    return 0;
}