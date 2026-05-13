# PokerTH Code Review Report

**Date:** 2026-05-13  
**Revision:** 12  
**Reviewer:** AI Code Reviewer  
**Scope:** Full codebase (`src/` — 367 files, ~76K LOC)

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

### 1. Timing-Attack Vulnerability in HashBuf Comparison (Security) — Fixed in prior revision

**File:** `src/core/crypthelper.cpp`  
**Severity:** Medium  
**Issue:** `HashBuf::operator==` used `memcmp()` which can short-circuit on the first differing byte, leaking timing information. Since these hashes include avatar MD5 data that's compared against known values, this was a potential side-channel vector.

**Fix:** Replaced `memcmp` with a constant-time comparison loop using XOR accumulation with `volatile` to prevent optimizer interference.

### 2. `assert()` in Card Dealing Replaced with Runtime Check (Bug/Robustness) — Fixed in prior revision

**File:** `src/engine/local_engine/localhand.cpp`  
**Severity:** Medium  
**Issue:** The card dealing loop used `assert()` to validate array bounds. In release builds (`NDEBUG` defined), `assert()` is compiled out entirely, meaning bounds violations would silently corrupt memory rather than being caught. While the bounds are mathematically safe under normal operation, a corrupted `activePlayerList` could bypass the assumed invariant.

**Fix:** Replaced `assert()` with a proper `if` check that throws `LocalException` on bounds violation. This ensures safety in both debug and release builds.

### 3. Integer Overflow in `sqlite3_get_table` Wrapper (Security) — Fixed in prior revision

**File:** `src/gui/qt/gametable/log/guilog.cpp`  
**Severity:** Low  
**Issue:** The `sqlite3_get_table` compatibility wrapper computes `total = (nRow + 1) * nCol` without overflow checking. With extremely large result sets from a malicious or corrupted database, this multiplication could overflow `size_t`, leading to a too-small allocation and subsequent buffer overflow when writing to the `result` array.

**Fix:** Added overflow check: `if (nCol > 0 && total / nCol != nRow + 1)` returns `SQLITE_NOMEM`.

### 4. Missing `[[nodiscard]]` on Thread Safety Methods (Correctness) — Fixed in prior revision

**File:** `src/core/thread.h`  
**Severity:** Low  
**Issue:** `ShouldTerminate()` and `IsRunning()` return values that are critical for correct thread lifecycle management. Without `[[nodiscard]]`, callers could accidentally ignore these return values.

**Fix:** Added `[[nodiscard]]` attribute to both methods.

### 5. `assert()` in `getCurrentBeRo()` Replaced with Runtime Check (Bug/Robustness) — NEW

**File:** `src/engine/local_engine/localhand.h`  
**Severity:** Medium  
**Issue:** `getCurrentBeRo()` used `assert(currentRound < myBeRo.size())` to guard against out-of-bounds vector access. In release builds (`NDEBUG` defined), `assert()` is compiled out entirely, meaning an invalid `currentRound` value would cause undefined behavior via out-of-bounds vector indexing. This is the same class of issue as #2 above (card dealing assert) but in a more frequently-used accessor method that is called throughout the engine loop.

**Fix:** Replaced `assert()` with a proper runtime bounds check that throws `LocalException(ERR_BERO_NOT_FOUND)`. Also removed the now-unnecessary `#include <cassert>` from both `localhand.h` and `localhand.cpp`.

### 6. Raw `new`/`delete` in `Replay` Class Replaced with `unique_ptr` (Code Quality) — NEW

**File:** `src/engine/local_engine/replay.h`, `src/engine/local_engine/replay.cpp`  
**Severity:** Low  
**Issue:** The `Replay` class managed a `QSqlDatabase*` member with manual `new`/`delete` in the destructor. Per the project's coding guidelines (AGENTS.md: "Avoid raw `new`/`delete`; use smart pointers"), this should use `std::unique_ptr` for exception safety and clarity. While the current code is technically correct, manual resource management is error-prone during future modifications.

**Fix:** Changed `QSqlDatabase *replaySqliteLogDb` to `std::unique_ptr<QSqlDatabase> replaySqliteLogDb`, replaced manual destructor with `= default`, and added `#include <memory>`.

### 7. Missing Explicit `#include <cstring>` in `crypthelper.cpp` (Correctness) — NEW

**File:** `src/core/crypthelper.cpp`  
**Severity:** Low  
**Issue:** `crypthelper.cpp` uses `memcmp()` (in `HashBuf::operator<`) and relies on `<cstring>` being transitively included via OpenSSL headers. While this works with current builds, it's fragile — if the OpenSSL headers ever stop including `<cstring>`, or if building with a different SSL backend (gcrypt path), the `memcmp` call would fail to compile.

**Fix:** Added explicit `#include <cstring>` to the includes.

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
1. **Qt widget raw pointers**: Classes like `gameTableImpl`, `SoundEvents` use raw `new`/`delete` for member widgets. This is acceptable as Qt's parent-child ownership model and the explicit destructor cleanup makes this safe. Converting to `std::unique_ptr` would add complexity without meaningful safety benefit since the ownership is clear.

2. **`boost::shared_ptr` vs `std::shared_ptr`**: The codebase uses `boost::shared_ptr` throughout for consistency. While `std::shared_ptr` is the modern choice, migrating would be a large mechanical change with no functional benefit and risk of introducing bugs.

3. **`catch (...)` blocks**: Present in 16 locations, all in thread wrappers, destructors, or top-level error handlers where catching all exceptions is the correct behavior.

4. **`memcpy` usage**: All `memcpy` calls for MD5 hashes are preceded by proper size validation (`size() == MD5_DATA_SIZE`). The remaining `memcpy` calls in send/receive buffers operate on properly sized vectors.

5. **`boost::mutex::scoped_lock`**: Used consistently instead of `std::lock_guard`. This is correct and functional; migrating to `std::` equivalents would be a cosmetic change.

### Architectural Notes
1. **State pattern** is used for both client (`ClientState` hierarchy) and server game (`ServerGameState` hierarchy) state machines — clean design.
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
| `src/engine/local_engine/localhand.h` | Replace `assert()` in `getCurrentBeRo()` with runtime check; remove `#include <cassert>` | **NEW** |
| `src/engine/local_engine/localhand.cpp` | Remove unused `#include <cassert>` | **NEW** |
| `src/engine/local_engine/replay.h` | Replace raw `QSqlDatabase*` with `std::unique_ptr<QSqlDatabase>` | **NEW** |
| `src/engine/local_engine/replay.cpp` | Simplify destructor to `= default` | **NEW** |
| `src/core/crypthelper.cpp` | Add explicit `#include <cstring>` for `memcmp` | **NEW** |
