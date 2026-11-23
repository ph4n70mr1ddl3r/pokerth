// Regression test: ServerBanManager::GetBanList() under-reports the remaining
// duration of timed bans by exactly one hour.
//
// Bug hunt
// --------
// src/net/serverbanmanager.cpp, ServerBanManager::InternalRegisterTimedBan():
//
//     tmpTimer->expires_after(hours(durationHours));
//
// sets the ban expiry to t0 + N hours. GetBanList() then renders it as
//
//     auto expiry = timer->expiry();
//     auto now = steady_clock::now();
//     banText << " duration: " << duration_cast<hours>(expiry - now).count() << "h";
//
// Because "now" is always a little after "t0", the remaining duration is
// N hours minus a small delta. std::chrono::duration_cast<hours> truncates
// toward zero, so a ban just created for 1 hour shows "duration: 0h", and a
// fresh 12-hour ban shows "duration: 11h". The "listban" admin command always
// under-reports the remaining time by one hour.
//
// The fix
// -------
// Round the remaining duration UP to the nearest whole hour instead of
// truncating. A shared durationCastCeil<> helper is used by both the nickname
// and IP ban loops.
//
// This standalone test models both the buggy (truncating) and fixed (ceiling)
// computations with deterministic time points and asserts the correct output.
//
// Compile:
//   g++ -std=c++23 -Wall -Wextra -o /tmp/test_ban_duration_display_offbyone test_ban_duration_display_offbyone.cpp
//   /tmp/test_ban_duration_display_offbyone

#include <chrono>
#include <cstdio>

using namespace std::chrono;

// The BUGGY computation from GetBanList(): truncate toward zero.
static long long buggyRemainingHours(steady_clock::time_point expiry,
                                     steady_clock::time_point now)
{
	return duration_cast<hours>(expiry - now).count();
}

// The FIXED computation: round up to the nearest whole hour.
template <typename TargetDuration, typename Duration>
static TargetDuration durationCastCeil(Duration d)
{
	TargetDuration result = duration_cast<TargetDuration>(d);
	if (result < d) {
		++result;
	}
	return result;
}

static long long fixedRemainingHours(steady_clock::time_point expiry,
                                     steady_clock::time_point now)
{
	return durationCastCeil<hours>(expiry - now).count();
}

struct Case {
	int banHours;
	long long elapsedNanos;
	long long expected;
	const char *description;
};

int main()
{
	const steady_clock::time_point base{};
	const Case cases[] = {
		{ 1, 1, 1, "fresh 1h ban must show 1h (buggy shows 0h)" },
		{ 2, 1, 2, "fresh 2h ban must show 2h (buggy shows 1h)" },
		{ 12, 1, 12, "fresh 12h ban must show 12h (buggy shows 11h)" },
		{ 24, 1, 24, "fresh 24h ban must show 24h (buggy shows 23h)" },
		{ 1, 500000000LL, 1, "half a second into a 1h ban -> 1h" },
		{ 12, 3600000000000LL - 1, 12, "almost one hour into a 12h ban -> still 12h" },
		{ 12, 3600000000000LL, 11, "exactly one hour elapsed -> 11h" },
		{ 12, 0, 12, "exact expiry with no elapsed time -> 12h" },
	};

	int failures = 0;
	std::printf("ServerBanManager ban-duration display regression test\n\n");

	for (const Case &c : cases) {
		auto expiry = base + hours(c.banHours);
		auto now = base + nanoseconds(c.elapsedNanos);
		long long buggy = buggyRemainingHours(expiry, now);
		long long fixed = fixedRemainingHours(expiry, now);
		std::printf("ban=%2dh elapsed=%13lldns expected=%2lld buggy=%2lld fixed=%2lld  %s\n",
			c.banHours, c.elapsedNanos, c.expected, buggy, fixed, c.description);
		if (fixed != c.expected) {
			std::printf("  FAIL: fixed implementation returned wrong result\n");
			++failures;
		}
	}

	// Demonstrate the original defect is real for a freshly created ban.
	{
		auto expiry = base + hours(1);
		auto now = base + nanoseconds(1);
		if (buggyRemainingHours(expiry, now) != 0) {
			std::printf("  FAIL: expected buggy implementation to reproduce the 0h defect\n");
			++failures;
		}
	}

	std::printf("\n");
	if (failures == 0) {
		std::printf("ALL TESTS PASSED\n");
		return 0;
	}
	std::printf("TESTS FAILED: %d failures\n", failures);
	return 1;
}
