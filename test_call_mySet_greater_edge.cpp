// Edge case test for PLAYER_ACTION_CALL when mySet > highestSet and targetBet > myCash.
//
// Verifies that the call is correctly rejected when the bet amount exceeds
// the player's available cash, even when mySet >= highestSet.
#include <cstdio>
#include <cstdlib>

struct MockPlayer {
    int mySet{0};
    int myCash{100};

    int getMySet() const { return mySet; }
    int getMyCash() const { return myCash; }
};

// Fixed implementation from checkMyAction
static int checkMyActionCallFixed(MockPlayer& p, int targetBet, int highestSet) {
    if (p.getMySet() >= highestSet) {
        if (targetBet <= p.getMyCash()) {
            return 0;
        }
    }
    else if (targetBet <= p.getMyCash()) {
        if (p.getMyCash() + p.getMySet() >= highestSet && targetBet == highestSet - p.getMySet()) {
            return 0;
        }
        if (p.getMyCash() + p.getMySet() <= highestSet && targetBet == p.getMyCash()) {
            return 0;
        }
    }
    return 1;
}

int main() {
    // Scenario: mySet > highestSet, targetBet > myCash - should be invalid
    MockPlayer p;
    p.mySet = 150;
    p.myCash = 100;
    int result = checkMyActionCallFixed(p, 200, 100);
    
    printf("Test: mySet > highestSet, targetBet > myCash\n");
    printf("  mySet = %d, highestSet = %d, targetBet = %d, myCash = %d\n", p.mySet, 100, 200, p.myCash);
    printf("  Result = %d (expected 1, invalid call)\n", result);
    
    if (result == 1) {
        printf("PASS: Call correctly rejected when targetBet > myCash\n");
        return 0;
    } else {
        printf("FAIL: Call should be invalid when targetBet > myCash\n");
        return 1;
    }
}
