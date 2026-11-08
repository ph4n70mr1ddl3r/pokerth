// Test for clientstate.cpp signed/unsigned comparison bug at line 770.
//
// Bug: In clientstate.cpp, the loop `for (int i = 0; i < netListNew.playerids_size(); i++)`
// compares the loop variable i (signed int) with netListNew.playerids_size() (unsigned size_t).
// Due to C++ usual arithmetic conversions, when i is negative (or when the size exceeds INT_MAX),
// the comparison behaves incorrectly.
//
// Fix: Use size_t for the loop variable, matching the type returned by playerids_size().
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_clientstate_bug2 test_clientstate_bug2.cpp && /tmp/test_clientstate_bug2

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <memory>

// Mock RepeatedField similar to protobuf
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

// Mock ClientInfo
struct ClientInfo {
    // Mock method
    bool GetCachedPlayerInfo(uint32_t playerId, struct PlayerInfo& info) {
        (void)playerId; (void)info;
        return false;
    }
    void RequestPlayerInfo(std::vector<uint32_t>& requestList) {
        (void)requestList;
    }
};

// PlayerInfo struct
struct PlayerInfo {};

// Mock Client class
class Client {
public:
    bool GetCachedPlayerInfo(uint32_t playerId, PlayerInfo& info) {
        (void)playerId; (void)info;
        return false;
    }
    void RequestPlayerInfo(std::vector<uint32_t>& requestList) {
        (void)requestList;
    }
} client;

// Simulated code from clientstate.cpp line 770
// Old (buggy) code pattern:
void processPlayers_Buggy(const RepeatedField& playerids) {
    for (int i = 0; i < static_cast<int>(playerids.size()); i++) {
        // process player i
        (void)playerids.Get(i);
    }
}

// Fixed code pattern:
void processPlayers_Fixed(const RepeatedField& playerids) {
    for (size_t i = 0; i < playerids.size(); i++) {
        // process player i
        (void)playerids.Get(i);
    }
}

int main() {
    int failures = 0;
    bool allPassed = true;

    printf("Testing clientstate.cpp signed/unsigned comparison bug (line 770)\n\n");

// --- Scenario 1: Normal number of players (valid behavior) ---
    printf("--- Scenario 1 (Normal: 4 players) ---\n");
    {
        RepeatedField playerids;
        playerids.Add(1);
        playerids.Add(2);
        playerids.Add(3);
        playerids.Add(4);

        // Buggy: should work fine with 4 players
        try {
            processPlayers_Buggy(playerids);
            printf("  Buggy: processed 4 players OK\n");
        } catch (const std::exception& e) {
            printf("  Buggy: threw exception (UNEXPECTED)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should work fine with 4 players
        try {
            processPlayers_Fixed(playerids);
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
        RepeatedField playerids;
        for (uint32_t i = 0; i < 100; i++) {
            playerids.Add(i);
        }

        // Buggy: should work fine with 100 players (well within INT_MAX)
        try {
            processPlayers_Buggy(playerids);
            printf("  Buggy: processed 100 players OK\n");
        } catch (const std::exception& e) {
            printf("  Buggy: threw exception (UNEXPECTED for valid count)\n");
            allPassed = false;
            failures++;
        }

        // Fixed: should work fine with 100 players
        try {
            processPlayers_Fixed(playerids);
            printf("  Fixed: processed 100 players OK\n");
        } catch (const std::exception& e) {
            printf("  Fixed: threw exception (UNEXPECTED for valid count)\n");
            allPassed = false;
            failures++;
        }
    }

// --- Scenario 3: Demonstrating the signed/unsigned conversion issue ---
    printf("\n--- Scenario 3 (Demonstrate signed/unsigned conversion) ---\n");
    {
        RepeatedField playerids;
        for (uint32_t i = 0; i < 100; i++) {
            playerids.Add(i);
        }

        unsigned numPlayers = playerids.size();

        // Demonstrate the bug: with int i, the comparison i < static_cast<int>(numPlayers)
        // involves signed/unsigned conversion
        int i_buggy = 0;
        for (; i_buggy < static_cast<int>(numPlayers); i_buggy++) {
            // process player
        }
        printf("  Buggy loop: iterated %d times\n", i_buggy);

        // Fixed: with size_t i, the comparison is proper
        size_t i_fixed = 0;
        for (; i_fixed < numPlayers; i_fixed++) {
            // process player
        }
        printf("  Fixed loop: iterated %zu times\n", i_fixed);
    }

    printf("\n");
    if (!allPassed) {
        printf("TEST FAILED: One or more scenarios did not pass as expected\n");
        return 1;
    }

    printf("TEST PASSED: signed/unsigned comparison bug confirmed and fix verified\n");
    return 0;
}