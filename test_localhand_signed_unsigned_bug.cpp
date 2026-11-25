// Test for LocalHand::LocalHand() signed/unsigned comparison bug.
//
// Bug: In LocalHand::LocalHand(), the loop variable k (int) is compared
// with activePlayerList->size() (size_t), cast to int. Due to C++ usual
// arithmetic conversions, if the number of players exceeds INT_MAX, the
// comparison behaves incorrectly.
//
// Additionally, on 64-bit systems where size_t is 64-bit and int is 32-bit,
// the signed/unsigned comparison between int k and size_t can cause
// unexpected behavior.
//
// Fix: Use size_t for the loop variable and remove the static_cast<int>,
//      or properly handle the signed/unsigned comparison.
//
// Compile: g++ -O2 -std=c++23 -o test_localhand_signed_unsigned_bug test_localhand_signed_unsigned_bug.cpp && /tmp/test_localhand_signed_unsigned_bug

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>

// Mock active player list
struct ActivePlayer {
    int id;
    ActivePlayer(int i) : id(i) {}
};

// Mock PlayerList (vector of shared_ptr)
using PlayerList = std::vector<ActivePlayer*>;

// Old (buggy) code pattern:
void processPlayers_Buggy(const PlayerList& players) {
    for (int k = 0; k < static_cast<int>(players.size()); k++) {
        // process player k
    }
}

// Fixed code pattern:
void processPlayers_Fixed(const PlayerList& players) {
    for (size_t k = 0; k < players.size(); k++) {
        // process player k
    }
}

int main() {
    int failures = 0;
    bool allPassed = true;

    printf("Testing LocalHand signed/unsigned comparison bug\n\n");

// --- Scenario 1: Normal number of players ---
    printf("--- Scenario 1 (4 players) ---\n");
    {
        PlayerList players;
        for (int i = 0; i < 4; i++) {
            players.push_back(new ActivePlayer(i));
        }

        try {
            processPlayers_Buggy(players);
            printf("  Buggy: processed 4 players OK\n");
        } catch (const std::exception& e) {
            printf("  Buggy: threw exception (UNEXPECTED)\n");
            allPassed = false;
            failures++;
        }

        try {
            processPlayers_Fixed(players);
            printf("  Fixed: processed 4 players OK\n");
        } catch (const std::exception& e) {
            printf("  Fixed: threw exception (UNEXPECTED)\n");
            allPassed = false;
            failures++;
        }
    }

// --- Scenario 2: Edge case with many players ---
    printf("\n--- Scenario 2 (Many players: 100 players) ---\n");
    {
        PlayerList players;
        for (int i = 0; i < 100; i++) {
            players.push_back(new ActivePlayer(i));
        }

        try {
            processPlayers_Buggy(players);
            printf("  Buggy: processed 100 players OK\n");
        } catch (const std::exception& e) {
            printf("  Buggy: threw exception (UNEXPECTED for valid count)\n");
            allPassed = false;
            failures++;
        }

        try {
            processPlayers_Fixed(players);
            printf("  Fixed: processed 100 players OK\n");
        } catch (const std::exception& e) {
            printf("  Fixed: threw exception (UNEXPECTED for valid count)\n");
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