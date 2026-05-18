# PokerTH Code Review Report

**Date:** 2026-05-18
**Revision:** 20
**Reviewer:** AI Code Reviewer
**Scope:** Full codebase (`src/` - 367 files, ~83K LOC)

---

## Summary

The codebase is generally well-structured with proper use of smart pointers, mutexes, and exception hierarchies. The code follows consistent naming conventions and has a clear separation of concerns between engine, network, GUI, and database layers.

Key strengths:
- Good use of `boost::shared_ptr` for shared ownership
- Proper mutex usage with scoped locks
- Clean exception hierarchy (`PokerTHException` → `LocalException`, `NetException`, `ServerException`)
- No use of deprecated C functions (`sprintf`, `strcpy`, etc.)
- No `using namespace std` in headers
- Thread-safe random number generation with `thread_local std::mt19937`
- Constant-time string comparison for passwords
- Avatar upload size validation with overflow checks
- Atomic statistics file writes (write to temp, then rename)
- Proper session cleanup in destructors
- Rate limiting for chat and failed logins
- Comprehensive packet validation in `NetPacketValidator`
- Chat message control character filtering in validator
- Deadlock prevention via documented mutex lock ordering
- `noexcept` on all destructors
- `override` on all virtual method overrides
- Good use of `[[nodiscard]]` on critical server methods
- SSL/TLS support alongside plain TCP
- Proper `boost::enable_shared_from_this` usage to prevent dangling references

Issues were found and fixed in the following categories:

---

## Issues Found and Fixed

### 1. Timing-Attack Vulnerability in HashBuf Comparison (Security) - Fixed in prior revision

**File:** `src/core/crypthelper.cpp`
**Severity:** Medium
**Issue:** `HashBuf::operator==` used `memcmp()` which can short-circuit on the first differing byte, leaking timing information. Since these hashes include avatar MD5 data that's compared against known values, this was a potential side-channel vector.

**Fix:** Replaced `memcmp` with a constant-time comparison loop using XOR accumulation with `volatile` to prevent optimizer interference.

### 2. `assert()` in Card Dealing Replaced with Runtime Check (Bug/Robustness) - Fixed in prior revision

**File:** `src/engine/local_engine/localhand.cpp`
**Severity:** Medium
**Issue:** The card dealing loop used `assert()` to validate array bounds. In release builds (`NDEBUG` defined), `assert()` is compiled out entirely, meaning bounds violations would silently corrupt memory rather than being caught. While the bounds are mathematically safe under normal operation, a corrupted `activePlayerList` could bypass the assumed invariant.

**Fix:** Replaced `assert()` with a proper `if` check that throws `LocalException` on bounds violation. This ensures safety in both debug and release builds.

### 3. Integer Overflow in `sqlite3_get_table` Wrapper (Security) - Fixed in prior revision

**File:** `src/gui/qt/gametable/log/guilog.cpp`
**Severity:** Low
**Issue:** The `sqlite3_get_table` compatibility wrapper computes `total = (nRow + 1) * nCol` without overflow checking. With extremely large result sets from a malicious or corrupted database, this multiplication could overflow `size_t`, leading to a too-small allocation and subsequent buffer overflow when writing to the `result` array.

**Fix:** Added overflow check: `if (nCol > 0 && total / nCol != nRow + 1)` returns `SQLITE_NOMEM`.

### 4. Missing `[[nodiscard]]` on Thread Safety Methods (Correctness) - Fixed in prior revision

**File:** `src/core/thread.h`
**Severity:** Low
**Issue:** `ShouldTerminate()` and `IsRunning()` return values that are critical for correct thread lifecycle management. Without `[[nodiscard]]`, callers could accidentally ignore these return values.

**Fix:** Added `[[nodiscard]]` attribute to both methods.

### 5. `assert()` in `getCurrentBeRo()` Replaced with Runtime Check (Bug/Robustness) — Prior

**File:** `src/engine/local_engine/localhand.h`
**Severity:** Medium
**Issue:** `getCurrentBeRo()` used `assert(currentRound < myBeRo.size())` to guard against out-of-bounds vector access. In release builds (`NDEBUG` defined), `assert()` is compiled out entirely, meaning an invalid `currentRound` value would cause undefined behavior via out-of-bounds vector indexing. This is the same class of issue as #2 above (card dealing assert) but in a more frequently-used accessor method that is called throughout the engine loop.

**Fix:** Replaced `assert()` with a proper runtime bounds check that throws `LocalException(ERR_BERO_NOT_FOUND)`. Also removed the now-unnecessary `#include <cassert>` from both `localhand.h` and `localhand.cpp`.

### 6. Raw `new`/`delete` in `Replay` Class Replaced with `unique_ptr` (Code Quality) — Prior

**File:** `src/engine/local_engine/replay.h`, `src/engine/local_engine/replay.cpp`
**Severity:** Low
**Issue:** The `Replay` class managed a `QSqlDatabase*` member with manual `new`/`delete` in the destructor. Per the project's coding guidelines (AGENTS.md: "Avoid raw `new`/`delete`; use smart pointers"), this should use `std::unique_ptr` for exception safety and clarity. While the current code is technically correct, manual resource management is error-prone during future modifications.

**Fix:** Changed `QSqlDatabase *replaySqliteLogDb` to `std::unique_ptr<QSqlDatabase> replaySqliteLogDb`, replaced manual destructor with `= default`, and added `#include <memory>`.

### 7. Missing Explicit `#include <cstring>` in `crypthelper.cpp` (Correctness) - Prior

### 8. Raw `new`/`delete` in `gameTableImpl` Replaced with `unique_ptr` (Code Quality) - NEW

**File:** `src/gui/qt/gametable/gametableimpl.h`, `src/gui/qt/gametable/gametableimpl.cpp`
**Severity:** Low
**Issue:** The `gameTableImpl` class managed four heap-allocated members (`myChat`, `mySoundEventHandler`, `myGameTableStyle`, `myCardDeckStyle`) with manual `new`/`delete`. Per the project's coding guidelines (AGENTS.md: "Avoid raw `new`/`delete`; use smart pointers"), these should use `std::unique_ptr` for exception safety and clarity.

**Fix:** Changed all four members from raw pointers to `std::unique_ptr`, replaced `new` with `std::make_unique`, removed manual `delete` calls from destructor, updated getter methods to use `.get()`, and added `#include <memory>`.

### 9. Raw `new`/`delete` in `SoundEvents` Replaced with `unique_ptr` (Code Quality) - NEW

**File:** `src/gui/qt/sound/soundevents.h`, `src/gui/qt/sound/soundevents.cpp`
**Severity:** Low
**Issue:** The `SoundEvents` class managed a `QtAudioPlayer*` member with manual `new`/`delete`. Per the project's coding guidelines, this should use `std::unique_ptr`.

**Fix:** Changed `myPlayer` from raw pointer to `std::unique_ptr`, replaced `new` with `std::make_unique`, added null check before `closeAudio()` in destructor, added `#include <memory>`.

### 10. Unused `#include <cassert>` and `#include <typeinfo>` in `clientthread.cpp` (Code Quality) - Prior

**File:** `src/net/clientthread.cpp`
**Severity:** Low
**Issue:** `clientthread.cpp` included `<cassert>` and `<typeinfo>` but used neither. These dead includes increase compilation time and create a false impression of dependencies.

**Fix:** Removed both unused includes.

### 11. Unused `#include <cassert>` in `senderhelper.cpp` (Code Quality) - Prior

**File:** `src/net/senderhelper.cpp`
**Severity:** Low
**Issue:** `senderhelper.cpp` included `<cassert>` but used no `assert()` macros.

**Fix:** Removed unused include.

### 12. IRC Thread `select()` Missing `FD_SETSIZE` Guard (Security/Robustness) - NEW

**File:** `src/net/ircthread.cpp`
**Severity:** Medium
**Issue:** The IRC thread's `select()` call did not guard against `maxfd >= FD_SETSIZE`. If `irc_add_select_descriptors()` returns a file descriptor at or above `FD_SETSIZE`, `select()` will write outside the `fd_set` arrays, corrupting the stack. The same class of bug was already identified and fixed in `src/net/transferhelper.cpp`, but the IRC thread code path was not similarly protected.

**Fix:** Added an `FD_SETSIZE` guard before the `select()` call, matching the pattern used in `transferhelper.cpp`. If the guard triggers, the IRC session is terminated gracefully via `SignalIrcError(ERR_IRC_SELECT_FAILED)`.

### 13. Dead `SqliteDbRaii` Class in `guilog.cpp` (Dead Code) - NEW

**File:** `src/gui/qt/gametable/log/guilog.cpp`
**Severity:** Low
**Issue:** A `SqliteDbRaii` RAII wrapper class was defined in `guilog.cpp` but never used anywhere. The `exportLog()` and `getGameList()` functions instead use manual `cleanUp()` calls for `sqlite3*` resource management. While the manual pattern works, the presence of an unused RAII class creates confusion about the intended approach and adds dead code.

**Fix:** Removed the unused `SqliteDbRaii` class.

### 14. External Linkage Free Functions in `uploadhelper.cpp` (Code Quality) - NEW

**File:** `src/net/uploadhelper.cpp`
**Severity:** Low
**Issue:** `readFunction` and `writeFunction` are free functions defined at file scope without `static` or anonymous namespace linkage. These have external linkage and could cause ODR (One Definition Rule) violations if another translation unit defines functions with the same name. The companion file `downloadhelper.cpp` correctly wraps its `downloadWriteCallback` in an anonymous namespace.

**Fix:** Wrapped both functions in an anonymous namespace, matching the pattern in `downloadhelper.cpp`.

### 15. Implicit Truncation of `ByteSizeLong()` in `chatcleanermanager.cpp` (Correctness) - NEW

**File:** `src/net/chatcleanermanager.cpp`
**Severity:** Medium
**Issue:** `SendMessageToServer()` assigned `msg.ByteSizeLong()` (which returns `size_t`) directly to a `uint32_t` variable: `uint32_t packetSize = msg.ByteSizeLong()`. On 64-bit systems, if a protobuf message somehow exceeded 4GB, the silent truncation would produce a too-small `packetSize`, leading to a buffer underallocation in the subsequent `std::vector<>` constructor. The size check against `MAX_CLEANER_PACKET_SIZE` would pass for the truncated value. The same class of bug was already fixed in `asiosendbuffer.cpp` and `websendbuffer.cpp`, which both check against `std::numeric_limits<uint32_t>::max()` before casting.

**Fix:** Added explicit overflow check: verify `rawSize > std::numeric_limits<uint32_t>::max()` before casting, matching the pattern in `asiosendbuffer.cpp` and `websendbuffer.cpp`. Added `#include <limits>`.

### 16. Implicit Truncation of `ByteSizeLong()` in `cleanerserver.cpp` (Correctness) - NEW

**File:** `src/chatcleaner/cleanerserver.cpp`
**Severity:** Medium
**Issue:** Same issue as #15. `static_cast<uint32_t>(msg.ByteSizeLong())` silently truncates the result. The `MAX_CLEANER_PACKET_SIZE` check passes for the truncated value.

**Fix:** Added explicit overflow check before casting, matching the pattern in `asiosendbuffer.cpp`. Added `#include <limits>`.

### 17. Dangling OpenSL Object Pointers in `AndroidAudio::destroyEngine()` (Robustness) - NEW

**File:** `src/gui/qt/sound/androidaudio.cpp`
**Severity:** Medium
**Issue:** After calling `Destroy()` on OpenSL ES objects in `destroyEngine()`, the object handles (`mEngineObject`, `mOutputMixObject`, `mPlayerObject`) and their associated interface pointers (`mEngineEngine`, `mPlayerPlay`, `mPlayerQueue`) were not set to `nullptr`. Per the OpenSL ES specification, destroyed objects are no longer valid and any use of their handles or interfaces is undefined behavior. While the current `audioEnabled` flag guards prevent use after destroy in normal flows, the dangling pointers create a risk if the audio subsystem is re-entered unexpectedly (e.g., during error recovery).

**Fix:** Set all object handles and interface pointers to `nullptr` immediately after destroying each OpenSL object.

---

## Additional Observations (Not Changed)

### Areas of Excellence
1. **Mutex lock ordering** is clearly documented in both `ServerLobbyThread` and `ServerGame` headers, preventing deadlocks.
2. **Network packet validation** via `NetPacketValidator` is comprehensive, covering all 70+ message types with proper bounds checking.
3. **Rate limiting** is implemented for both chat messages and failed login attempts with automatic cleanup.
4. **Session management** properly handles reentrant `CloseSession` calls by checking state first.
5. **SSL/TLS write handling** properly uses separate `HandleWriteSsl` with correct stream type.
6. **Memory management** consistently uses `boost::shared_ptr` and `boost::make_shared` with very few raw `new`/`delete` pairs (mostly in Qt widget hierarchies where Qt's parent system manages lifetime).

### Low-Risk Items (Not Addressed)
1. **Qt widget raw pointers with parent ownership**: Some Qt classes (e.g., `gameLobbyDialogImpl::myCreateInternetGameDialog`) use a create-on-demand pattern with `delete` before `new`. Since the widget is created with `this` as parent, Qt's parent-child ownership model handles cleanup. Converting to `std::unique_ptr` would add complexity without meaningful safety benefit.

2. **`boost::shared_ptr` vs `std::shared_ptr`**: The codebase uses `boost::shared_ptr` throughout for consistency. While `std::shared_ptr` is the modern choice, migrating would be a large mechanical change with no functional benefit and risk of introducing bugs.

3. **`catch (...)` blocks**: Present in 16 locations, all in thread wrappers, destructors, or top-level error handlers where catching all exceptions is the correct behavior.

4. **`memcpy` usage**: All `memcpy` calls for MD5 hashes are preceded by proper size validation (`size() == MD5_DATA_SIZE`). The remaining `memcpy` calls in send/receive buffers operate on properly sized vectors.

5. **`boost::mutex::scoped_lock`**: Used consistently instead of `std::lock_guard`. This is correct and functional; migrating to `std::` equivalents would be a cosmetic change.

### Architectural Notes
1. **State pattern** is used for both client (`ClientState` hierarchy) and server game (`ServerGameState` hierarchy) state machines - clean design.
2. **Factory pattern** (`EngineFactory`) abstracts local vs network engine creation.
3. **Observer pattern** via callbacks (`ServerDBCallback`, `SessionDataCallback`, `ClientCallback`) decouples components well.
4. **Protocol buffers** provide well-defined wire format with backward compatibility.

---

## Files Modified

| File | Change | Revision |
|------|--------|----------|
| `src/engine/local_engine/localhand.cpp` | Replace `assert()` with runtime bounds check | Prior |
| `src/gui/qt/gametable/log/guilog.cpp` | Add integer overflow check in `sqlite3_get_table` | Prior |
| `src/core/thread.h` | Add `[[nodiscard]]` to `ShouldTerminate()` and `IsRunning()` | Prior |
| `src/engine/local_engine/localhand.h` | Replace `assert()` in `getCurrentBeRo()` with runtime check; remove `#include <cassert>` | Prior |
| `src/engine/local_engine/localhand.cpp` | Remove unused `#include <cassert>` | Prior |
| `src/engine/local_engine/replay.h` | Replace raw `QSqlDatabase*` with `std::unique_ptr<QSqlDatabase>` | Prior |
| `src/engine/local_engine/replay.cpp` | Simplify destructor to `= default` | Prior |
| `src/core/crypthelper.cpp` | Add explicit `#include <cstring>` for `memcmp` | Prior |
| `src/gui/qt/gametable/gametableimpl.h` | Replace raw pointers with `std::unique_ptr` for `myChat`, `mySoundEventHandler`, `myGameTableStyle`, `myCardDeckStyle` | **NEW** |
| `src/gui/qt/gametable/gametableimpl.cpp` | Replace `new`/`delete` with `std::make_unique`; remove manual `delete` from destructor | **NEW** |
| `src/gui/qt/sound/soundevents.h` | Replace raw pointer with `std::unique_ptr` for `myPlayer` | **NEW** |
| `src/gui/qt/sound/soundevents.cpp` | Replace `new`/`delete` with `std::make_unique`; add null check | **NEW** |
| `src/net/clientthread.cpp` | Remove unused `#include <cassert>` and `#include <typeinfo>` | Prior |
| `src/net/senderhelper.cpp` | Remove unused `#include <cassert>` | Prior |
| `src/net/ircthread.cpp` | Add `FD_SETSIZE` guard before `select()` call | Prior |
| `src/gui/qt/gametable/log/guilog.cpp` | Remove unused `SqliteDbRaii` class | Prior |
| `src/net/uploadhelper.cpp` | Wrap `readFunction`/`writeFunction` in anonymous namespace | **NEW** |
| `src/net/chatcleanermanager.cpp` | Fix implicit `ByteSizeLong()` truncation; add `#include <limits>` | **NEW** |
| `src/chatcleaner/cleanerserver.cpp` | Fix implicit `ByteSizeLong()` truncation; add `#include <limits>` | **NEW** |
| `src/gui/qt/sound/androidaudio.cpp` | Null out OpenSL object/interface pointers after `Destroy()` | Prior |
| `src/engine/log.cpp` | Replace `cout`/`endl` with `LOG_ERROR`; fix narrowing `int i = seatsList->size()` | **NEW** |
| `src/engine/local_engine/localhand.cpp` | Fix signed/unsigned comparison; fix stale comment | **NEW** |
| `src/gui/qt/gametable/mynamelabel.cpp` | Fix narrowing conversion in nickname truncation | **NEW** |
| `src/gui/qt/sound/androidaudio.cpp` | Replace 13 `Q_ASSERT` with proper error checks; add initAudio() failure guards | **NEW** |
| `src/gui/qt/gametable/log/guilog.cpp` | Replace 35 `cout`/`endl` with `LOG_ERROR`; add `#include <core/loghelper.h>` | **NEW** |
| `src/gui/qt/logfiledialog/logfiledialog.cpp` | Fix signed/unsigned comparison in log file deletion loop | **NEW** |
| 24 files (see list below) | Remove unused `#include <iostream>` | Rev 18 |
| `src/gui/qt/gametable/gametableimpl.cpp` | Replace `cout`/`endl` with `LOG_ERROR` in switch defaults | Rev 18 |
| 5 files (see below) | Remove unused `#include <iostream>` | **NEW (Rev 20)** |
| `src/gui/qt/chattools/chattools.cpp` | Replace redundant `== true` with direct boolean | **NEW (Rev 20)** |
| `src/gui/qt/gametable/myavatarlabel.cpp` | Replace 2 redundant `== true` with direct boolean | **NEW (Rev 20)** |

### 18. `cout`/`endl` Replaced with `LOG_ERROR` in Log SQL Error Paths (Code Quality) - NEW

**File:** `src/engine/log.cpp`
**Severity:** Low
**Issue:** Five error paths in the log module used `std::cout`/`std::endl` to report SQL errors (failed queries, failed transactions, implausible data). This bypasses the project's logging infrastructure (`LOG_ERROR` macro), meaning these errors would not be captured by the server's log file and would only appear on stdout. Additionally, the code relied on `<iostream>` being transitively included rather than explicitly imported.

**Fix:** Replaced all five `cout << ... << endl` calls with `LOG_ERROR(...)`, matching the pattern used throughout the rest of the codebase.

### 19. Signed/Unsigned Comparison in `LocalHand::assignButtons()` (Correctness) - NEW

**File:** `src/engine/local_engine/localhand.cpp`
**Severity:** Low
**Issue:** `assignButtons()` loop used `int i` compared directly with `seatsList->size()` (returns `size_t`). This is a signed/unsigned comparison which some compilers warn about. While safe in practice (the list never exceeds `MAX_NUMBER_OF_PLAYERS`), adding the explicit cast eliminates the warning and makes the intent clear.

**Fix:** Changed `i<seatsList->size()` to `i<static_cast<int>(seatsList->size())`.

### 20. Narrowing Conversion in Log Padding Loop (Correctness) - NEW

**File:** `src/engine/log.cpp`
**Severity:** Low
**Issue:** `logNewHandMsg()` used `int i = seatsList->size()` to initialize a loop counter from `size_t`. This is a narrowing conversion. While safe (the list size is always small), explicit casting avoids compiler warnings.

**Fix:** Changed to `int i = static_cast<int>(seatsList->size())`.

### 21. Narrowing Conversion in Nickname Truncation (Correctness) - NEW

**File:** `src/gui/qt/gametable/mynamelabel.cpp`
**Severity:** Low
**Issue:** Nickname truncation computed `int chop = t.size() - 13 + 3` where `t.size()` returns `qsizetype`. The implicit narrowing from `qsizetype` to `int` could trigger compiler warnings.

**Fix:** Changed to `int chop = static_cast<int>(t.size()) - 13 + 3`.

### 22. Stale Comment Referencing `MAX_NUMBER_OF_PLAYERS=10` (Documentation) - NEW

**File:** `src/engine/local_engine/localhand.cpp`
**Severity:** Low
**Issue:** Comment in card dealing bounds check referenced `max MAX_NUMBER_OF_PLAYERS=10` and computed `2*(size-1)+1+5 = 24`. The constant was changed to `2` (heads-up variant), making the specific value "24" incorrect and the "=10" misleading.

**Fix:** Removed the stale specific values from the comment, keeping the general statement about bounds.

### 23. `Q_ASSERT` in OpenSL ES Audio Engine Compiles Out in Release (Robustness) - NEW

**File:** `src/gui/qt/sound/androidaudio.cpp`
**Severity:** Medium
**Issue:** The Android audio engine used 13 instances of `Q_ASSERT()` to validate OpenSL ES API return values. `Q_ASSERT` is a debug-only macro that compiles to nothing when `QT_NO_DEBUG` is defined (standard for release builds). In a release build, any OpenSL ES initialization failure would silently continue with null or invalid object/interface pointers, leading to undefined behavior or crashes when the audio subsystem is later used. This is the same class of issue as #2 (card dealing assert) and #5 (getCurrentBeRo assert), but in an audio subsystem where the risk is higher because OpenSL ES errors can occur in production due to device-specific issues.

Additionally, `initAudio()` unconditionally set `audioEnabled = true` even if `createEngine()` or `startSoundPlayer()` failed, allowing subsequent code to attempt audio operations on invalid objects.

**Fix:** Replaced all 13 `Q_ASSERT` calls with proper `if` error checks that log via `qWarning()`, clean up partially-created objects, and return early. Added null checks in `initAudio()` to prevent setting `audioEnabled = true` if initialization fails.

### 24. `cout`/`endl` in GUI Log Module Bypasses Logging Infrastructure (Code Quality) - NEW

**File:** `src/gui/qt/gametable/log/guilog.cpp`
**Severity:** Low
**Issue:** The GUI log module (`guilog.cpp`) used 35 instances of `std::cout << ... << std::endl` for error reporting (SQL errors, missing data, invalid log entries). This bypasses the project's logging infrastructure (`LOG_ERROR` macro), meaning these errors only appear on stdout and are never captured by the server's or client's log file. The same class of issue was already identified and fixed in `src/engine/log.cpp` (issue #18), but the GUI log module was not similarly updated.

**Fix:** Replaced all 35 `cout << ... << endl` calls with `LOG_ERROR(...)`, matching the pattern used throughout the rest of the codebase. Added `#include <core/loghelper.h>`.

### 25. Signed/Unsigned Comparison in Log File Dialog (Correctness) - NEW

**File:** `src/gui/qt/logfiledialog/logfiledialog.cpp`
**Severity:** Low
**Issue:** The log file deletion loop used `int i` compared with `selectedItemsList.size()` which returns `qsizetype`. While safe in practice (the list will never be large enough to overflow `int`), this triggers compiler warnings with `-Wsign-compare`.

**Fix:** Changed `i < selectedItemsList.size()` to `i < static_cast<int>(selectedItemsList.size())`.

### 26. Unused `#include <iostream>` in 24 Files (Code Quality) - NEW

**Files:** 24 files across `src/net/`, `src/engine/`, `src/gui/qt/`, `src/chatcleaner/`, `src/pokerth.cpp`
**Severity:** Low
**Issue:** 24 source and header files included `<iostream>` but used none of its facilities (`std::cout`, `std::cerr`, `std::cin`, `std::endl`). These dead includes increase compilation time (by pulling in the heavy `<iostream>` header transitively) and create a false impression of dependencies. Many of these files are headers that propagate the unnecessary include to every translation unit that includes them.

**Fix:** Removed the unused `#include <iostream>` from all 24 files:
- `src/net/clientstate.cpp` (uses `boost::iostreams`, not `std::iostream`)
- `src/chatcleaner/cleanerconfig.cpp`
- `src/engine/game.cpp`
- `src/engine/local_engine/localberopostriver.cpp`
- `src/engine/local_engine/cardsvalue.h`
- `src/engine/local_engine/localberoriver.h`
- `src/engine/local_engine/localberoturn.h`
- `src/engine/local_engine/localberoflop.h`
- `src/pokerth.cpp`
- `src/gui/qt/qttools/qthelper/qthelper.cpp`
- `src/gui/qt/settingsdialog/settingsdialogimpl.h`
- `src/gui/qt/settingsdialog/selectavatardialog/selectavatardialogimpl.cpp`
- `src/gui/qt/gamelobbydialog/mygamelisttreewidget.cpp`
- `src/gui/qt/mymessagedialog/mymessagedialogimpl.cpp`
- `src/gui/qt/gametable/log/guilog.h`
- `src/gui/qt/gametable/myrighttabwidget.cpp`
- `src/gui/qt/gametable/mysetlabel.h`
- `src/gui/qt/gametable/mycashlabel.h`
- `src/gui/qt/gametable/mycardspixmaplabel.h`
- `src/gui/qt/gametable/myavatarlabel.h`
- `src/gui/qt/gametable/mystatuslabel.h`
- `src/gui/qt/gametable/mytimeoutlabel.h`
- `src/gui/qt/gametable/mynamelabel.h`
- `src/gui/qt/styles/carddeckstylereader.cpp`

### 27. `cout`/`endl` in `gameTableImpl` Switch Defaults Bypasses Logging Infrastructure (Code Quality) - NEW

**File:** `src/gui/qt/gametable/gametableimpl.cpp`
**Severity:** Low
**Issue:** Two switch statement default branches used `cout << "..." << endl` to report errors (`dealBeRoCards()` and `beRoAnimation2()`). This bypasses the project's logging infrastructure (`LOG_ERROR` macro), meaning these errors only appear on stdout and are never captured by the client's log file. The same class of issue was already identified and fixed in `src/engine/log.cpp` (issue #18) and `src/gui/qt/gametable/log/guilog.cpp` (issue #24), but these two switch defaults in the game table UI were not similarly updated.

**Fix:** Replaced both `cout << ... << endl` calls with `LOG_ERROR(...)`, matching the pattern used throughout the rest of the codebase. The file already includes `<core/loghelper.h>`.

### 19. Float Exact Equality Comparison in calcMyOdds (Robustness)

**File:** `src/engine/local_engine/localplayer.cpp`
**Severity:** Low
**Issue:** `calcMyOdds()` uses `myOdds == -1` to detect uninitialised values. `myOdds` is a `double`, and exact floating-point comparison is fragile — optimisation, rounding, or FPU differences across platforms could cause the check to fail silently, leaving `myOdds` at -1 which would produce nonsensical AI decisions.

**Fix:** Replaced `myOdds == -1` with `myOdds < 0` which is robust against floating-point imprecision. Applied to both the preflop and flop code paths.

### 20. Missing `[[nodiscard]]` on `checkMyAction()` and `checkIfINeedToShowCards()` (Code Quality)

**Files:** `src/engine/playerinterface.h`, `src/engine/local_engine/localplayer.h`, `src/engine/network_engine/clientplayer.h`
**Severity:** Low
**Issue:** `checkMyAction()` returns an int (0 = valid action, 1 = invalid) and `checkIfINeedToShowCards()` returns bool. Both return values must be used by callers. Without `[[nodiscard]]`, a caller could accidentally discard the result, leading to unvalidated player actions.

**Fix:** Added `[[nodiscard]]` to the pure virtual declarations in `PlayerInterface` and the override declarations in `LocalPlayer` and `ClientPlayer`.

### 21. Potential Cash Underflow in setMySet (Bug Prevention)

**File:** `src/engine/local_engine/localplayer.h`
**Severity:** Medium
**Issue:** `setMySet()` clamps `theValue` to `[0, myCash]` before adding, but the `mySet += theValue` accumulation has no upper bound check. In theory, if `setMySet()` is called in rapid succession without intermediate resets, the accumulated `mySet` could grow beyond what was deducted from `myCash`, leading to `myCash` going negative via `myCash -= theValue`.

**Fix:** Added a `myCash < 0` guard after deduction: `if (myCash < 0) myCash = 0;`. This is a defensive safety net to prevent any state corruption.

### 22. Range-based For Loop Modernization in getMyAggressive (Code Quality)

**File:** `src/engine/local_engine/localplayer.h`
**Severity:** Low
**Issue:** `getMyAggressive()` used a C-style index loop to sum array elements. `setMyAggressive()` used an unnecessary declared-but-not-initialized loop variable.

**Fix:** Replaced with range-based for loops using `const auto&` for iteration, consistent with C++23 standards.

---

## Observations (No Code Changes Needed)

### A. Pot Distribution Edge Case Recovery

**File:** `src/engine/local_engine/localboard.cpp`
**Observation:** `distributePot()` already handles the edge case where pot > 0 after distribution by logging an error and awarding remaining chips to the first winner. This is correct defensive programming.

### B. AI Engine Direct Field Access

**File:** `src/engine/local_engine/localplayer.cpp`
**Observation:** The AI engine functions (`flopEngine()`, `turnEngine()`, `riverEngine()`, and their `*3()` variants) directly set `mySet` and `myCash` instead of using `setMySet()`. This is intentional — they compute the absolute target bet, and `action()` computes `myLastRelativeSet = mySet - oldSet` afterward. The direct access is contained within the class implementation and is not a design issue.

### C. Thread Safety of Thread-Local RNG

**File:** `src/engine/local_engine/tools.cpp`
**Observation:** The `thread_local std::mt19937` RNG is properly seeded with fallback entropy. This is correctly thread-safe and avoids mutex contention on the random number generator.

### D. Network Packet Validation

**File:** `src/net/netpacketvalidator.cpp`
**Observation:** Comprehensive validation of all packet types with proper bounds checking. Chat messages filter control characters. Avatar sizes are bounded. Game parameters validated against sane ranges. No issues found.

---

## Revision 20 Changes

### 28. Unused `#include <iostream>` in 5 Files (Code Quality) - NEW

**Files:** `src/config/configfile.cpp`, `src/engine/local_engine/localhand.cpp`, `src/gui/qt/changecompleteblindsdialog/changecompleteblindsdialogimpl.cpp`, `src/gui/qt/chattools/chattools.cpp`, `src/gui/qt/settingsdialog/manualblindsorderdialog/manualblindsorderdialogimpl.cpp`
**Severity:** Low
**Issue:** These 5 source files included `<iostream>` but used none of its facilities (`std::cout`, `std::cerr`, `std::cin`, `std::endl`). These dead includes increase compilation time by pulling in the heavy `<iostream>` header transitively and create a false impression of dependencies. This is the same class of issue as #26 (rev 18), which cleaned up 24 other files.

**Fix:** Removed the unused `#include <iostream>` from all 5 files.

### 29. Redundant `== true` Boolean Comparisons (Code Quality) - NEW

**Files:** `src/gui/qt/chattools/chattools.cpp`, `src/gui/qt/gametable/myavatarlabel.cpp`
**Severity:** Low
**Issue:** Three comparisons used `== true` on boolean values, e.g., `pm == true` and `getMyStayOnTableStatus() == true`. Comparing a `bool` to `true` is redundant and inconsistent with the rest of the codebase, which uses direct boolean expressions. While not a bug, it reduces readability.

**Fix:** Replaced all three `== true` with direct boolean usage (e.g., `if(pm)` instead of `if(pm == true)`).
