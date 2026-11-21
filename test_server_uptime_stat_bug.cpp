/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

// Regression test for ServerAdminBot "stat" uptime formatting bug.
//
// In ServerAdminBot::SignalIrcChatMsg() the uptime was formatted as:
//   timeDiff.hours()/24 << " days " << timeDiff.hours()%24 << " hours "
//   << timeDiff.minutes() << " minutes " << timeDiff.seconds() << " seconds"
//
// boost::posix_time::time_duration::minutes() and ::seconds() return the
// TOTAL number of minutes / seconds in the whole duration, not the remaining
// component. Only hours() was reduced (via /24 and %24). As a result an
// uptime of 1 day 2 hours 30 minutes 45 seconds was reported as
// "1 days 2 hours 1590 minutes 95445 seconds".
//
// Fix: take minutes()%60 and seconds()%60.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_server_uptime_stat_bug test_server_uptime_stat_bug.cpp && /tmp/test_server_uptime_stat_bug

#include <cstdio>
#include <sstream>
#include <string>

// Minimal model of boost::posix_time::time_duration. The accessors intentionally
// return the TOTAL count in their unit, exactly like Boost does.
class FakeTimeDuration
{
public:
    FakeTimeDuration(int hours, int minutes, int seconds)
        : myTotalSeconds(hours * 3600 + minutes * 60 + seconds)
    {
    }

    long hours() const { return myTotalSeconds / 3600; }
    long minutes() const { return myTotalSeconds / 60; }
    long seconds() const { return myTotalSeconds; }

private:
    long myTotalSeconds;
};

static std::string formatUptimeBuggy(const FakeTimeDuration &timeDiff)
{
    std::ostringstream out;
    out << timeDiff.hours() / 24 << " days " << timeDiff.hours() % 24 << " hours "
        << timeDiff.minutes() << " minutes " << timeDiff.seconds() << " seconds";
    return out.str();
}

static std::string formatUptimeFixed(const FakeTimeDuration &timeDiff)
{
    std::ostringstream out;
    out << timeDiff.hours() / 24 << " days " << timeDiff.hours() % 24 << " hours "
        << timeDiff.minutes() % 60 << " minutes " << timeDiff.seconds() % 60 << " seconds";
    return out.str();
}

struct TestCase
{
    int hours;
    int minutes;
    int seconds;
    const char *expected;
};

int main()
{
    const TestCase cases[] = {
        {26, 30, 45, "1 days 2 hours 30 minutes 45 seconds"},
        {50, 0, 0, "2 days 2 hours 0 minutes 0 seconds"},
        {1, 5, 9, "0 days 1 hours 5 minutes 9 seconds"},
        {0, 0, 42, "0 days 0 hours 0 minutes 42 seconds"},
        {2, 59, 59, "0 days 2 hours 59 minutes 59 seconds"},
    };

    int failures = 0;
    printf("Testing ServerAdminBot \"stat\" uptime formatting bug\n\n");

    for (const TestCase &test : cases) {
        FakeTimeDuration duration(test.hours, test.minutes, test.seconds);
        const std::string buggy = formatUptimeBuggy(duration);
        const std::string fixed = formatUptimeFixed(duration);

        printf("uptime %dh %dm %ds\n", test.hours, test.minutes, test.seconds);
        printf("  buggy: %s\n", buggy.c_str());
        printf("  fixed: %s\n", fixed.c_str());
        printf("  expect: %s\n", test.expected);

        if (fixed != test.expected) {
            printf("  FAIL: fixed formatting did not produce the expected value\n");
            ++failures;
        } else {
            printf("  PASS\n");
        }

        // For uptimes of at least one hour the buggy string must differ from
        // the correct one; otherwise this test would not actually cover a bug.
        if (test.hours > 0 && buggy == test.expected) {
            printf("  FAIL: buggy formatting unexpectedly matches expected value\n");
            ++failures;
        }
        printf("\n");
    }

    if (failures > 0) {
        printf("TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("TEST PASSED: uptime stat formatting bug confirmed and fix verified\n");
    return 0;
}
