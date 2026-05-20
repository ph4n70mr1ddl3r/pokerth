# PokerTH Code Review Report

**Date:** 2026-05-19
**Revision:** 26
**Reviewer:** AI Code Reviewer
**Scope:** Full codebase (`src/` - 367 files, ~83K LOC) — incremental review from revision 25

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
| `src/gui/qt/settingsdialog/settingsdialogimpl.cpp` | Replace 4 error-path `qDebug()` with `qWarning()` | **NEW (Rev 22)** |
| `src/net/clientstate.cpp` | Replace error-path `qDebug()` with `qWarning()` for TLS handshake failure | **NEW (Rev 22)** |
| `src/gui/qt/logfiledialog/logfiledialog.cpp` | Replace error-path `qDebug()` with `qWarning()` for log upload failure | **NEW (Rev 22)** |
| `src/net/netpacketvalidator.cpp` | Fix signed/unsigned comparison in `ValidateListIntRange()` | **NEW (Rev 22)** |
| `src/engine/local_engine/localplayer.cpp` | Fix 4 narrowing `size_t` to `int` conversions | **NEW (Rev 22)** |
| `src/gui/qt/gametable/myavatarlabel.cpp` | Replace 2 redundant `== true` with direct boolean | **NEW (Rev 20)** |
| `src/net/clientthread.h` | Fix `std::accumulate` type mismatch: `0` → `0U` | **NEW (Rev 23)** |
| `src/gui/qt/settingsdialog/selectavatardialog/selectavatardialogimpl.cpp` | Fix signed/unsigned comparison; remove unused variable | **NEW (Rev 23)** |
| `src/gui/qt/logfiledialog/logfiledialog.cpp` | Fix signed/unsigned comparison in log file list iteration | **NEW (Rev 23)** |
| `src/gui/qt/sound/sdlplayer.cpp` | Replace 2 error-path `qDebug()` with `qWarning()` | **NEW (Rev 23)** |
| `src/pokerth.cpp` | Replace error-path `qDebug()` with `qWarning()` for missing translation | **NEW (Rev 23)** |
| `src/gui/qt/sound/androidaudio.cpp` | Replace error-path `qDebug()` with `qWarning()` for missing sound | **NEW (Rev 23)** |
| `src/net/clientthread.cpp` | Replace 2 error-path `qDebug()` with `qWarning()` for TLS handshake failures | **NEW (Rev 23)** |
| `src/gui/qt/mymessagedialog/mymessagedialogimpl.cpp` | Add size check before `.at(1)` on split results; remove unused `#include <cstdlib>` and `#include <fstream>` | **NEW (Rev 24)** |

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

---

## Revision 21 Changes

### 30. Empty String Guard for `blindsListString.remove()` (Bug Prevention) - NEW

**File:** `src/gui/qt/gamelobbydialog/gamelobbydialogimpl.cpp`
**Severity:** Medium
**Issue:** `updateDialogBlinds()` called `blindsListString.remove(blindsListString.length()-2, 2)` without checking if the string was empty. `QString::length()` returns `qsizetype` (signed 64-bit on 64-bit platforms), so `0 - 2 = -2` which is a valid negative index. While Qt's `QString::remove()` with a negative position is a no-op, this is an unclear intent and could be fragile if the Qt behavior ever changes or if the code is ported to a different string type.

**Fix:** Added a guard `if (blindsListString.length() >= 2)` before the `remove()` call.

### 31. `qDebug()` Used for Error Paths in WAV Parser (Code Quality) - NEW

**File:** `src/gui/qt/sound/androidsoundeffect.cpp`
**Severity:** Low
**Issue:** The Android WAV sound effect loader used `qDebug()` for 6 error conditions (file open failure, truncated header, invalid format, missing data chunk, invalid data length, data read mismatch). `qDebug()` is intended for debug/informational messages; `qWarning()` is the appropriate level for error conditions. Using the correct log level ensures that error messages are distinguishable from debug output and are treated appropriately by logging frameworks. This is the same class of issue as #23 (androidaudio.cpp Q_ASSERT replacement) — error conditions in the Android audio subsystem should be properly reported.

**Fix:** Replaced all 6 `qDebug()` calls on error paths with `qWarning()`. Also fixed a stray single-quote typo in one message: `"didn't read correct amount of data' :" → `"didn't read correct amount of data:"`.

### 32. `qDebug()` Used for Error Paths in Chat Cleaner Server (Code Quality) - NEW

**File:** `src/chatcleaner/cleanerserver.cpp`
**Severity:** Low
**Issue:** The chat cleaner server used `qDebug()` for 16 error conditions (invalid port, server start failure, buffer overflow, null socket, invalid packet size, parse failure, exception handling, client errors, send failures). Since the chat cleaner is a standalone server process, it doesn't use the `LOG_ERROR` infrastructure but should still distinguish error messages from informational debug output. `qWarning()` is the appropriate Qt log level for error conditions.

**Fix:** Replaced all 16 error-path `qDebug()` calls with `qWarning()`. The informational startup message `"The server is running on port %1."` and the debug socket state message were left as `qDebug()` since they are not error conditions.

### 33. Narrowing `qsizetype` to `int` in `sqlite3_get_table` Wrapper (Correctness) - NEW

**File:** `src/gui/qt/gametable/log/guilog.cpp`
**Severity:** Low
**Issue:** `sqlite3_get_table()` wrapper assigned `rows.size()` (returns `qsizetype`, 64-bit on 64-bit platforms) directly to `int nRow`. This is a narrowing conversion. While safe in practice (log files never have millions of rows), the explicit cast avoids compiler warnings and is consistent with the type safety standards applied in prior reviews.

**Fix:** Changed `int nRow = rows.size()` to `int nRow = static_cast<int>(rows.size())`.

---

## Revision 22 Changes

### 34. `qDebug()` Used for Config Error Paths in Settings Dialog (Code Quality) - NEW

**File:** `src/gui/qt/settingsdialog/settingsdialogimpl.cpp`
**Severity:** Low
**Issue:** The settings dialog used `qDebug()` for 4 config error conditions (game table style not found in list, game table style could not be loaded, card deck style not found in list, card deck style could not be loaded). The messages themselves contain `"Config ERROR"` in their text, clearly indicating they are error conditions, yet they are logged at debug level. `qWarning()` is the appropriate Qt log level for error conditions. This is the same class of issue as #31 (androidsoundeffect.cpp qDebug→qWarning) and #32 (cleanerserver.cpp qDebug→qWarning).

**Fix:** Replaced all 4 config error `qDebug()` calls with `qWarning()`.

### 35. `qDebug()` Used for TLS Handshake Failure in Client State (Code Quality) - NEW

**File:** `src/net/clientstate.cpp`
**Severity:** Low
**Issue:** `ClientStateStartConnect::HandleSslHandshake()` used `qDebug()` to report SSL handshake failure with error code and message. This is an error condition that leads to a `ClientException` being thrown. `qWarning()` is the appropriate Qt log level for error conditions. The successful handshake message was correctly left as `qDebug()` since it is informational.

**Fix:** Replaced the error-path `qDebug()` with `qWarning()`.

### 36. `qDebug()` Used for Log Upload Failure in Log File Dialog (Code Quality) - NEW

**File:** `src/gui/qt/logfiledialog/logfiledialog.cpp`
**Severity:** Low
**Issue:** The log file upload handler used `qDebug()` to output the server's error response message when log file processing failed on the server side. This is an error condition (the else branch of a success check). `qWarning()` is the appropriate Qt log level. The success-path `qDebug()` for the hash was correctly left as-is.

**Fix:** Replaced the error-path `qDebug()` with `qWarning()`.

### 37. Signed/Unsigned Comparison in `ValidateListIntRange()` (Correctness) - NEW

**File:** `src/net/netpacketvalidator.cpp`
**Severity:** Low
**Issue:** `ValidateListIntRange()` used `int i` compared directly with `l.size()` which returns `int` for protobuf's `RepeatedField`, but the pattern is inconsistent with the signed/unsigned fixes applied throughout the codebase. The comparison `int i < l.size()` is technically a signed/unsigned comparison if `size()` returns an unsigned type on any platform. Adding the explicit cast ensures consistency and eliminates any potential warning.

**Fix:** Changed `i < l.size()` to `i < static_cast<int>(l.size())`.

### 38. Narrowing Conversion of `size()` to `int` in AI Player (Correctness) - NEW

**File:** `src/engine/local_engine/localplayer.cpp`
**Severity:** Low
**Issue:** Four places in the AI player engine assigned `currentHand->getActivePlayerList()->size()` (returns `size_t`) directly to `int players`. This is a narrowing conversion from unsigned to signed. While safe in practice (the active player list never exceeds `MAX_NUMBER_OF_PLAYERS` which fits in `int`), the explicit cast avoids compiler warnings and is consistent with the type safety standards applied throughout the codebase (e.g., issue #19, #20, #21, #25, #33).

**Fix:** Changed all 4 instances to use `static_cast<int>(currentHand->getActivePlayerList()->size())`.

---

## Revision 23 Changes

### 39. `std::accumulate` Type Mismatch in `AveragePing()` (Correctness) - NEW

**File:** `src/net/clientthread.h`
**Severity:** Low
**Issue:** `AveragePing()` used `std::accumulate(pingValues.begin(), pingValues.end(), 0)` to compute the average of `std::list<unsigned>` ping values. The initial value `0` is `int`, which causes `std::accumulate` to use `int` as the accumulator type. If individual ping values are large (e.g., >1000ms) and there are 20 entries (SIZE_PING_BACKLOG), the sum could theoretically overflow `int`, causing undefined behavior. The subsequent division by `static_cast<unsigned>(pingValues.size())` would then operate on a corrupted value.

**Fix:** Changed the initial value from `0` to `0U` so the accumulation occurs in `unsigned`, matching the element type.

### 40. Signed/Unsigned Comparison in Avatar Dialog (Correctness) - NEW

**File:** `src/gui/qt/settingsdialog/selectavatardialog/selectavatardialogimpl.cpp`
**Severity:** Low
**Issue:** `refreshAvatarView()` used `int i` compared with `QStringList::size()` which returns `qsizetype` (64-bit signed on 64-bit platforms). This is a signed/unsigned width mismatch. The separate declaration `int i = 0` before the loop was also unnecessary.

**Fix:** Removed the separate `int i = 0` declaration, changed to inline `int i=0` in the for loop, and added `static_cast<int>(currentViewList.size())` for the comparison.

### 41. Signed/Unsigned Comparison in Log File Dialog (Correctness) - NEW

**File:** `src/gui/qt/logfiledialog/logfiledialog.cpp`
**Severity:** Low
**Issue:** The log file list iteration used `int i` compared with `QFileInfoList::size()` which returns `qsizetype`. This is the same class of signed/unsigned width mismatch as issue #40.

**Fix:** Changed to `for (int i=0; i < static_cast<int>(dbFilesList.size()); i++)` and removed the unnecessary separate `int i = 0` declaration.

### 42. `qDebug()` Used for SDL Audio Error Paths (Code Quality) - NEW

**File:** `src/gui/qt/sound/sdlplayer.cpp`
**Severity:** Low
**Issue:** Two SDL audio error conditions used `qDebug()`: `Mix_OpenAudio()` failure and `Mix_SetPosition()` failure. These are error conditions where audio subsystem initialization or spatial positioning fails. `qWarning()` is the appropriate Qt log level for error conditions. This is the same class of issue as #23 (androidaudio.cpp Q_ASSERT replacement), #31 (androidsoundeffect.cpp qDebug→qWarning), and #34-36 (settings/TLS/log qDebug→qWarning).

**Fix:** Replaced both error-path `qDebug()` calls with `qWarning()`.

### 43. `qDebug()` Used for Missing Translation Warning (Code Quality) - NEW

**File:** `src/pokerth.cpp`
**Severity:** Low
**Issue:** The application's main function used `qDebug()` to report that the locale translation file was not found. A missing translation means the UI will display in English rather than the user's preferred language. This is a warning condition, not informational debug output. `qWarning()` is the appropriate Qt log level.

**Fix:** Replaced `qDebug()` with `qWarning()` for the missing translation message.

### 44. `qDebug()` Used for Missing Sound Effect in Android Audio (Code Quality) - NEW

**File:** `src/gui/qt/sound/androidaudio.cpp`
**Severity:** Low
**Issue:** `reallyPlaySound()` used `qDebug()` to report that a requested sound effect was not found in the sound map. This is an error condition (the sound cannot be played). `qWarning()` is the appropriate Qt log level.

**Fix:** Replaced `qDebug()` with `qWarning()` for the missing sound message.

### 45. `qDebug()` Used for TLS Handshake Failure in SSL Info Callback (Code Quality) - NEW

**File:** `src/net/clientthread.cpp`
**Severity:** Low
**Issue:** The SSL info callback (`SslInfoCallback`) used `qDebug()` for TLS handshake exit failures (`ret == 0` and `ret < 0`). These indicate handshake failures - the same class of issue as #35 (TLS handshake failure in client state). The informational messages (loop progress, handshake start/done) were correctly left as `qDebug()`.

**Fix:** Replaced the two error-path `qDebug()` calls in the `SSL_CB_EXIT` branch with `qWarning()`.

---

## Revision 24 Changes

### 46. Unsafe `.split(",").at(1)` Without Size Check in Message Dialog (Robustness) - NEW

**File:** `src/gui/qt/mymessagedialog/mymessagedialogimpl.cpp`
**Severity:** Medium
**Issue:** Four functions (`exec()`, `show()`, `writeConfig()`, `checkIfMesssageWillBeDisplayed()`) parsed the `IfInfoMessageShowList` config entries by calling `tmpString.split(",").at(1)` and `.at(0)` without checking that the split produced at least 2 elements. If a config file is corrupted or manually edited with a malformed entry (e.g., missing comma, empty string), `QString::split(",")` returns a list with fewer than 2 elements, and `.at(1)` throws a `QString::OutOfBoundsException` (Qt's out-of-range exception), crashing the application. This is the same class of issue as the `cleanerserver.cpp` `checkreturn.at()` calls, which already have proper size guards.

**Fix:** Added `parts.size() >= 2` check before accessing `parts.at(1)` in all 4 locations. Split the result into a named `QStringList parts` variable to avoid calling `split()` twice per iteration. Also removed 2 unused includes (`<cstdlib>`, `<fstream>`).

---

## Revision 25 Changes

### 47. `qDebug()` Used for File I/O Error Paths in Chat Cleaner Config (Code Quality) - NEW

**File:** `src/chatcleaner/cleanerconfig.cpp`
**Severity:** Low
**Issue:** The chat cleaner config module used `qDebug()` for 4 error conditions: 3 file open failures ("Failed to open file for writing") and 1 config load failure ("Cannot update config file: Unable to load configuration."). These are error conditions where file I/O operations fail. `qWarning()` is the appropriate Qt log level for error conditions. This is the same class of issue as #31 (androidsoundeffect.cpp qDebug→qWarning), #32 (cleanerserver.cpp qDebug→qWarning), and #34-36 (settings/TLS/log qDebug→qWarning).

**Fix:** Replaced all 4 error-path `qDebug()` calls with `qWarning()`.

### 48. `qDebug()` Used for Atomic Config File Write Error Paths (Code Quality) - NEW

**File:** `src/config/configfile.cpp`
**Severity:** Low
**Issue:** The atomic config file writer (`atomicWriteFile()`) used `qDebug()` for 3 error conditions: temp file open failure, temp file write failure, and temp file rename failure. These are error conditions where the application cannot save its configuration. `qWarning()` is the appropriate Qt log level for error conditions. This is the same class of issue as #47 (cleanerconfig.cpp qDebug→qWarning).

**Fix:** Replaced all 3 error-path `qDebug()` calls with `qWarning()`.

### Files Modified This Revision

| File | Change | Revision |
|------|--------|----------|
| `src/chatcleaner/cleanerconfig.cpp` | Replace 4 error-path `qDebug()` with `qWarning()` | **NEW (Rev 25)** |
| `src/config/configfile.cpp` | Replace 3 error-path `qDebug()` with `qWarning()` | **NEW (Rev 25)** |

---

## Revision 26 Changes

### 49. Stray Backslash (Line Continuation) After Closing Brace (Bug) - NEW

**File:** `src/gui/qt/gamelobbydialog/gamelobbydialogimpl.cpp`
**Severity:** Medium
**Issue:** Line 605 had a stray `\` (backslash) at the end of the closing brace of the `if/else` block in `updateGameItem()`. In C/C++, a backslash at the end of a line is a line continuation character that joins the current line with the next. In this case, the backslash joined `}` with the following blank line, which is benign (the preprocessor produces `}` + blank + `if`). However, if someone were to insert a statement on the next line, it would be silently joined to the closing brace, potentially causing unexpected behavior. The stray backslash was likely a typo.

**Fix:** Removed the stray `\` from the end of line 605.

### 50. C-Style Cast for Qt Sort Order (Code Quality) - NEW

**File:** `src/gui/qt/gamelobbydialog/gamelobbydialogimpl.cpp`
**Severity:** Low
**Issue:** `updateGameItem()` used a C-style cast `(Qt::SortOrder)` to convert the integer config value to a `Qt::SortOrder` enum. C-style casts can silently perform dangerous conversions (like `const_cast` or `reinterpret_cast`). Per C++23 best practices, `static_cast` should be used instead for enum conversions.

**Fix:** Replaced `(Qt::SortOrder)` with `static_cast<Qt::SortOrder>(...)`.

### Files Modified This Revision

| File | Change | Revision |
|------|--------|----------|
| `src/gui/qt/gamelobbydialog/gamelobbydialogimpl.cpp` | Remove stray backslash `\` at end of closing brace | **NEW (Rev 26)** |
| `src/gui/qt/gamelobbydialog/gamelobbydialogimpl.cpp` | Replace C-style cast with `static_cast<Qt::SortOrder>` | **NEW (Rev 26)** |
