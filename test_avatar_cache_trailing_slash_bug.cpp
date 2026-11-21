/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU Affero General Public License as            *
 * published by the Free Software Foundation, either version 3 of the        *
 * License, or (at your option) any later version.                           *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU Affero General Public License for more details.                       *
 *                                                                           *
 * You should have received a copy of the GNU Affero General Public License  *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************/

// Regression test: AvatarManager::RemoveOldAvatarCacheEntries() rejects every
// cached avatar when the configured cache dir has a trailing separator.
//
// Background
// ----------
// The default CacheDir written by ConfigFile has a trailing separator:
//     configfile.cpp:183    cacheDir += "cache/";       (POSIX)
//     configfile.cpp:154    cacheDir += "cache\\";      (Windows)
//
// AvatarManager::Init() stores fs::path(cacheDir).string() in m_cacheDir, and
// RemoveOldAvatarCacheEntries() (avatarmanager.cpp:465) reads it back and again
// applies fs::path(...).string(). std::filesystem::path::string() preserves the
// trailing separator, so `cacheDir` still ends in '/' (or '\\') at the point of
// the containment test on avatarmanager.cpp:494:
//
//     if (fileString.size() > cacheDir.size()
//      && fileString.compare(0, cacheDir.size(), cacheDir) == 0
//      && fileString[cacheDir.size()] == '/') { ... keepFile = true; ... }
//
// The bug
// -------
// For a real cached avatar such as
//     cacheDir  = "/home/user/.pokerth/cache/"
//     fileString= "/home/user/.pokerth/cache/d41d8cd9.png"
// the prefix comparison succeeds (the prefix includes the trailing '/'), but
// fileString[cacheDir.size()] is the FIRST CHARACTER OF THE MD5 FILENAME ('d'),
// not the separator the test expects. The condition is therefore always false,
// keepFile stays false, and every entry is pushed to removeList and erased from
// m_cachedAvatars. timeMap is never populated either, so the on-disk PNGs are
// left behind but forgotten in memory: avatars are re-downloaded every session.
// The Windows default additionally uses '\\', which the literal '/' test would
// reject even without the trailing-separator problem.
//
// The fix
// -------
// 1. Strip trailing separators from cacheDir after the fs::path round-trip, so
//    the character at cacheDir.size() is the separator that follows the cache
//    directory name in a cached file's path.
// 2. Accept both '/' and '\\' as that separator, since fs::path::string() emits
//    native separators.
//
// A sibling containment check in StoreAvatar() (avatarmanager.cpp:396-398)
// already avoids this problem by using fs::canonical(), which drops the
// trailing separator. This standalone test models both versions of the check
// (no Qt, Boost or filesystem required so the OOB/trailing-slash logic can be
// exercised deterministically on every platform).

#include <cstdio>
#include <cstring>
#include <string>

using std::string;

// ---------- models of the buggy code ----------

// fs::path(cacheDir).string() preserves any trailing separator.
static string normalizeBuggy(const string &cacheDir)
{
    return cacheDir;
}

// Exact condition from avatarmanager.cpp:494 (buggy version).
static bool isInCacheBuggy(const string &fileString, const string &cacheDirIn)
{
    const string cacheDir = normalizeBuggy(cacheDirIn);
    return fileString.size() > cacheDir.size()
        && fileString.compare(0, cacheDir.size(), cacheDir) == 0
        && fileString[cacheDir.size()] == '/';
}

// ---------- models of the fixed code ----------

static bool isSeparator(char c)
{
    return c == '/' || c == '\\';
}

// Strip trailing native separators (the fix applied after cachePath.string()).
static string normalizeFixed(const string &cacheDirIn)
{
    string cacheDir = cacheDirIn;
    while (!cacheDir.empty() && isSeparator(cacheDir.back()))
        cacheDir.pop_back();
    return cacheDir;
}

// Fixed containment check: separator-agnostic, accepts native separators.
static bool isInCacheFixed(const string &fileString, const string &cacheDirIn)
{
    const string cacheDir = normalizeFixed(cacheDirIn);
    if (cacheDir.empty())
        return false;
    return fileString.size() > cacheDir.size()
        && fileString.compare(0, cacheDir.size(), cacheDir) == 0
        && isSeparator(fileString[cacheDir.size()]);
}

// ---------- test harness ----------

static int failures = 0;

static void check(bool condition, const char *message)
{
    if (condition) {
        printf("PASS: %s\n", message);
    } else {
        printf("FAIL: %s\n", message);
        ++failures;
    }
}

int main()
{
    const string posixCache = "/home/user/.pokerth/cache/";
    const string posixFile = "/home/user/.pokerth/cache/d41d8cd9.png";
    const string siblingFile = "/home/user/.pokerth/cache_evil/d41d8cd9.png";

    printf("Testing AvatarManager trailing-separator avatar cache bug\n\n");

    // 1. Reproduce the bug: default POSIX cache dir with trailing '/'.
    check(!isInCacheBuggy(posixFile, posixCache),
          "bug reproduced: trailing-slash cache dir rejects a valid cached avatar");
    check(isInCacheFixed(posixFile, posixCache),
          "fixed check accepts a valid cached avatar with trailing-slash cache dir");

    // 2. Cache dir without trailing separator (the case that already worked).
    const string noSlashCache = "/home/user/.pokerth/cache";
    check(isInCacheBuggy(posixFile, noSlashCache),
          "baseline: no-trailing-slash cache dir was accepted by old check");
    check(isInCacheFixed(posixFile, noSlashCache),
          "fixed check still accepts a no-trailing-slash cache dir");

    // 3. Security property: a sibling directory sharing the prefix must be
    //    rejected. The trailing separator in the prefix must not make the
    //    check accept it.
    check(!isInCacheFixed(siblingFile, posixCache),
          "fixed check rejects a sibling cache_evil directory");
    check(!isInCacheFixed(siblingFile, noSlashCache),
          "fixed check rejects sibling directory with no-trailing-slash config");

    // 4. Multiple trailing separators.
    check(isInCacheFixed(posixFile, "/home/user/.pokerth/cache//"),
          "fixed check tolerates multiple trailing separators");

    // 5. Windows-style native paths with a trailing backslash.
    const string winCache = "C:\\Documents and Settings\\user\\.pokerth\\cache\\";
    const string winFile = "C:\\Documents and Settings\\user\\.pokerth\\cache\\d41d8cd9.png";
    const string winSibling = "C:\\Documents and Settings\\user\\.pokerth\\cache_evil\\d41d8cd9.png";
    check(!isInCacheBuggy(winFile, winCache),
          "bug reproduced: Windows cache dir with trailing '\\\\' rejects a valid avatar");
    check(isInCacheFixed(winFile, winCache),
          "fixed check accepts Windows paths with trailing backslash");
    check(!isInCacheFixed(winSibling, winCache),
          "fixed check rejects Windows sibling directory");

    // 6. Root / all-separator cache dir must be treated as unset: with the
    //    normalization it becomes empty, so RemoveOldAvatarCacheEntries()
    //    skips deletion entirely (conservative and safe).
    check(normalizeFixed("/").empty(),
          "root cache dir normalizes to empty (no deletion)");
    check(!isInCacheFixed(posixFile, "/"),
          "fixed check never claims a file is inside a root cache dir");

    // 7. A path exactly equal to the cache dir must not count as a file in it.
    check(!isInCacheFixed(noSlashCache, noSlashCache),
          "path equal to cache dir is not considered a cached file");

    if (failures > 0) {
        printf("\nTEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("\nTEST PASSED: trailing-separator avatar cache bug reproduced and fix verified\n");
    return 0;
}
