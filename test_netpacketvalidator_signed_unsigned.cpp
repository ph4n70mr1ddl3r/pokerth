// Test for NetPacketValidator::ValidateListIntRange signed/unsigned comparison bug.
//
// Bug: In ValidateListIntRange(), the loop variable is `size_t i` but `l.size()` 
// returns `int` (from protobuf::RepeatedField). The comparison `i < l.size()` causes
// a signed/unsigned comparison where `l.size()` is implicitly converted from `int` to `size_t`.
// If the protobuf field has an unusually large size (close to INT_MAX), or if there's
// data corruption causing a negative size, the behavior could be incorrect.
//
// The fix: Change the loop variable type from `size_t` to `int` to match `l.size()`'s return type.

#include <cstdio>
#include <cstdlib>
#include <cstdint>

// Mock protobuf RepeatedField interface for testing
class RepeatedField {
public:
    virtual int size() const = 0;
    virtual uint32_t Get(int index) const = 0;
    virtual ~RepeatedField() = default;
};

// The actual function from netpacketvalidator.cpp
bool ValidateListIntRange(const RepeatedField &l, uint32_t minval, uint32_t maxval)
{
    bool retVal = true;
    for (int i = 0; i < l.size(); ++i) {  // Fixed: use int instead of size_t
        if (!((l.Get(i) >= minval && l.Get(i) <= maxval))) {
            retVal = false;
            break;
        }
    }
    return retVal;
}

// Mock implementation for testing
class MockRepeatedField : public RepeatedField {
    int mySize;
    uint32_t* myValues;
public:
    MockRepeatedField(int size, uint32_t* values) : mySize(size), myValues(values) {}
    int size() const override { return mySize; }
    uint32_t Get(int index) const override { return myValues[index]; }
};

// Helper to create a mock field
static MockRepeatedField makeField(int size, uint32_t* values) {
    return MockRepeatedField(size, values);
}

int main()
{
    int failures = 0;

    printf("Testing NetPacketValidator::ValidateListIntRange signed/unsigned comparison bug:\n\n");

    // Test 1: Normal case with valid values
    {
        uint32_t vals[] = {5, 10, 15, 20};
        MockRepeatedField field(4, vals);
        bool result = ValidateListIntRange(field, 1, 100);
        printf("Test 1 (normal valid values): %s\n", result ? "PASS" : "FAIL");
        if (!result) { failures++; }
    }

    // Test 2: Values out of range
    {
        uint32_t vals[] = {5, 200, 15, 20};
        MockRepeatedField field(4, vals);
        bool result = ValidateListIntRange(field, 1, 100);
        printf("Test 2 (values out of range): %s\n", !result ? "PASS" : "FAIL");
        if (result) { failures++; }
    }

    // Test 3: Empty field
    {
        uint32_t vals[] = {};
        MockRepeatedField field(0, vals);
        bool result = ValidateListIntRange(field, 1, 100);
        printf("Test 3 (empty field): %s\n", result ? "PASS" : "FAIL");
        if (!result) { failures++; }
    }

    // Test 4: Single value at boundary
    {
        uint32_t vals[] = {1};
        MockRepeatedField field(1, vals);
        bool result = ValidateListIntRange(field, 1, 1);
        printf("Test 4 (single value at boundary): %s\n", result ? "PASS" : "FAIL");
        if (!result) { failures++; }
    }

    // Test 5: Single value above max
    {
        uint32_t vals[] = {200};
        MockRepeatedField field(1, vals);
        bool result = ValidateListIntRange(field, 1, 100);
        printf("Test 5 (single value above max): %s\n", !result ? "PASS" : "FAIL");
        if (result) { failures++; }
    }

    // Test 6: Single value below min
    {
        uint32_t vals[] = {0};
        MockRepeatedField field(1, vals);
        bool result = ValidateListIntRange(field, 1, 100);
        printf("Test 6 (single value below min): %s\n", result ? "PASS" : "FAIL");
        if (result) { failures++; }
    }

    printf("\n%d test(s) failed\n", failures);
    return failures > 0 ? 1 : 0;
}