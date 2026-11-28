/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

// Regression test: capfloodcheck.cpp signed/unsigned comparison fix.
//
// Verifies that the fix `static_cast<int>(token.length())` is present in the code.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <algorithm>

int main()
{
    int failures = 0;
    printf("Testing capfloodcheck.cpp signed/unsigned comparison fix\n\n");

    bool allPassed = true;

    // --- Scenario 1: Verify the fix is present in capfloodcheck.cpp ---
    printf("--- Scenario 1: Check fix present in capfloodcheck.cpp ---\n");
    {
        std::ifstream file("/home/riddler/pokerth/src/chatcleaner/capsfloodcheck.cpp");
        if (!file.is_open()) {
            printf("  FAIL: Could not open capfloodcheck.cpp\n");
            allPassed = false;
        } else {
            std::string line;
            bool foundFix = false;
            while (std::getline(file, line)) {
                if (line.find("static_cast<int>(token.length())") != std::string::npos) {
                    foundFix = true;
                    break;
                }
            }
            file.close();
            
            if (foundFix) {
                printf("  PASS: Fix 'static_cast<int>(token.length())' found in capfloodcheck.cpp\n");
            } else {
                printf("  FAIL: Fix 'static_cast<int>(token.length())' NOT found in capfloodcheck.cpp\n");
                allPassed = false;
            }
        }
    }

    // --- Scenario 2: Verify the old buggy pattern is NOT present ---
    printf("\n--- Scenario 2: Verify old buggy pattern is gone ---\n");
    {
        std::ifstream file2("/home/riddler/pokerth/src/chatcleaner/capsfloodcheck.cpp");
        if (!file2.is_open()) {
            printf("  FAIL: Could not open capfloodcheck.cpp\n");
            allPassed = false;
        } else {
            std::string line2;
            bool foundBuggy = false;
            while (std::getline(file2, line2)) {
                if (line2.find("i < token.length();") != std::string::npos &&
                    line2.find("static_cast") == std::string::npos) {
                    foundBuggy = true;
                    break;
                }
            }
            file2.close();
            
            if (!foundBuggy) {
                printf("  PASS: Old buggy pattern 'i < token.length()' not found\n");
            } else {
                printf("  FAIL: Old buggy pattern still present\n");
                allPassed = false;
            }
        }
    }

    printf("\n");
    if (!allPassed) {
        printf("TEST FAILED\n");
        return 1;
    }

    printf("ALL TESTS PASSED: capfloodcheck.cpp signed/unsigned comparison fix verified\n");
    return 0;
}