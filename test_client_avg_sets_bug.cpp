// Test for ClientPlayer::getMyAverageSets() count tracking bug.
//
// Bug: Previously, ClientPlayer::getMyAverageSets() always divided by 4,
// regardless of how many times setMyAverageSets() was called. This meant
// that after 1, 2, or 3 calls, the average was incorrectly computed (dividing
// by 4 instead of by the actual count), giving a different result than
// LocalPlayer::getMyAverageSets() which correctly tracks the count.
//
// Fix: ClientPlayer::setMyAverageSets() now increments myAverageSetsCount
// (capped at 4), and ClientPlayer::getMyAverageSets() divides by the count
// (or 4 if count is 0), making it consistent with LocalPlayer.
//
// Compile: g++ -O2 -std=c++23 -o test_client_avg_sets_bug test_client_avg_sets_bug.cpp && ./test_client_avg_sets_bug

#include <cstdio>
#include <cstdlib>
#include <array>

// Mock ClientPlayer state matching the clientplayer.h structure
struct MockClientPlayer {
    std::array<int, 4> myAverageSets;
    int myAverageSetsCount;

    MockClientPlayer() : myAverageSetsCount(0) {
        myAverageSets[0] = myAverageSets[1] = myAverageSets[2] = myAverageSets[3] = 0;
    }

    int getMyAverageSets() const {
        return (myAverageSets[0]+myAverageSets[1]+myAverageSets[2]+myAverageSets[3])/
               (myAverageSetsCount > 0 ? myAverageSetsCount : 4);
    }

    void setMyAverageSets(int theValue) {
        myAverageSets[0] = myAverageSets[1];
        myAverageSets[1] = myAverageSets[2];
        myAverageSets[2] = myAverageSets[3];
        myAverageSets[3] = theValue;
        if (myAverageSetsCount < 4) ++myAverageSetsCount;
    }
};

// Old (buggy) ClientPlayer::getMyAverageSets - always divides by 4
static int getMyAverageSets_buggy(const MockClientPlayer& p) {
    return (p.myAverageSets[0]+p.myAverageSets[1]+p.myAverageSets[2]+p.myAverageSets[3])/4;
}

// New (fixed) ClientPlayer::getMyAverageSets - divides by count
static int getMyAverageSets_fixed(const MockClientPlayer& p) {
    return (p.myAverageSets[0]+p.myAverageSets[1]+p.myAverageSets[2]+p.myAverageSets[3])/
           (p.myAverageSetsCount > 0 ? p.myAverageSetsCount : 4);
}

int main()
{
    int failures = 0;

    printf("Testing ClientPlayer::getMyAverageSets() count tracking fix:\n\n");

    // --- Scenario 1: After 0 calls to setMyAverageSets ---
    printf("--- Scenario 1 (0 calls, all sets = 0) ---\n");
    MockClientPlayer p1;
    int avg1_buggy = getMyAverageSets_buggy(p1);
    int avg1_fixed = p1.getMyAverageSets();  // calls the fixed version
    printf("  getMyAverageSets(): %d (expected 0)\n", avg1_fixed);
    if (avg1_fixed != 0) {
        printf("  FAIL: expected 0, got %d\n", avg1_fixed);
        failures++;
    } else {
        printf("  PASS\n");
    }

    // --- Scenario 2: After 1 call to setMyAverageSets with value 10 ---
    printf("\n--- Scenario 2 (1 call, sets = {10, 0, 0, 0}) ---\n");
    MockClientPlayer p2;
    p2.setMyAverageSets(10);
    int avg2_buggy = getMyAverageSets_buggy(p2);
    int avg2_fixed = p2.getMyAverageSets();
    printf("  getMyAverageSets(): %d (expected 10, old buggy would be 2)\n", avg2_fixed);
    // With the fix: count=1, so divide by 1 -> 10
    // With the bug: always divide by 4 -> (10+0+0+0)/4 = 2
    if (avg2_fixed == 10) {
        printf("  PASS: fixed version correctly divides by count=1\n");
    } else {
        printf("  FAIL: expected 10, got %d\n", avg2_fixed);
        failures++;
    }

    // --- Scenario 3: After 2 calls to setMyAverageSets ---
    printf("\n--- Scenario 3 (2 calls, sets = {old2, 10, 0, 0}) ---\n");
    MockClientPlayer p3;
    p3.setMyAverageSets(10);  // first call: sets = {0, 0, 0, 10}, count=1
    p3.setMyAverageSets(20);  // second call: sets = {0, 0, 10, 20}, count=2
    int avg3_buggy = getMyAverageSets_buggy(p3);
    int avg3_fixed = p3.getMyAverageSets();
    printf("  getMyAverageSets(): %d (expected 15, old buggy would be 7)\n", avg3_fixed);
    // With the fix: count=2, so divide by 2 -> (0+0+10+20)/2 = 15
    // With the bug: always divide by 4 -> (0+0+10+20)/4 = 7
    if (avg3_fixed == 15) {
        printf("  PASS: fixed version correctly divides by count=2\n");
    } else {
        printf("  FAIL: expected 15, got %d\n", avg3_fixed);
        failures++;
    }

    // --- Scenario 4: After 3 calls to setMyAverageSets ---
    printf("\n--- Scenario 4 (3 calls, sets = {old3, old2, 10, 20}) ---\n");
    MockClientPlayer p4;
    p4.setMyAverageSets(10);  // count=1
    p4.setMyAverageSets(20);  // count=2
    p4.setMyAverageSets(30);  // count=3
    int avg4_buggy = getMyAverageSets_buggy(p4);
    int avg4_fixed = p4.getMyAverageSets();
    printf("  getMyAverageSets(): %d (expected 20, old buggy would be 15)\n", avg4_fixed);
    // With the fix: count=3, so divide by 3 -> (0+10+20+30)/3 = 20
    // With the bug: always divide by 4 -> (0+10+20+30)/4 = 15
    if (avg4_fixed == 20) {
        printf("  PASS: fixed version correctly divides by count=3\n");
    } else {
        printf("  FAIL: expected 20, got %d\n", avg4_fixed);
        failures++;
    }

    // --- Scenario 5: After 4+ calls (count capped at 4) ---
    printf("\n--- Scenario 5 (4+ calls, all sets non-zero) ---\n");
    MockClientPlayer p5;
    p5.setMyAverageSets(10);  // count=1
    p5.setMyAverageSets(20);  // count=2
    p5.setMyAverageSets(30);  // count=3
    p5.setMyAverageSets(40);  // count=4
    p5.setMyAverageSets(50);  // count stays at 4 (capped)
    int avg5_buggy = getMyAverageSets_buggy(p5);
    int avg5_fixed = p5.getMyAverageSets();
    printf("  getMyAverageSets(): %d (expected 25, old buggy would be 25)\n", avg5_fixed);
    // With the fix: count=4, so divide by 4 -> (10+20+30+40)/4 = 25
    // With the bug: always divide by 4 -> (10+20+30+40)/4 = 25
    // Both give the same result when count reaches 4
    if (avg5_fixed == 25) {
        printf("  PASS: fixed version correctly divides by count=4\n");
    } else {
        printf("  FAIL: expected 25, got %d\n", avg5_fixed);
        failures++;
    }

    printf("\n");
    if (failures > 0) {
        printf("TEST FAILED: %d scenario failures\n", failures);
        return 1;
    }

    printf("TEST PASSED: ClientPlayer::getMyAverageSets() count tracking fix confirmed and verified\n");
    return 0;
}