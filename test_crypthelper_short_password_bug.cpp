/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

// Regression test for the AES short-password bug.
//
// CryptHelper::AES128Encrypt()/AES128Decrypt() guard their bodies with:
//     if (keySize >= AES_BLOCK_SIZE && ...)
// keyData/keySize are a *password*, not the raw AES key. BytesToKey() derives
// the 16-byte AES key and IV by SHA1-hashing whatever password bytes it is
// given, which is well defined for any non-empty input. Requiring >= 16 bytes
// therefore rejects perfectly valid short passwords.
//
// This breaks private games: servergamestate.cpp passes the joining player's
// password verbatim to AES128Encrypt(); with a password such as "secret" (6
// bytes) the call returns false and the server disconnects that player with
// ERR_SOCK_INVALID_STATE at the start of the very first hand. clientstate.cpp's
// AES128Decrypt() fails symmetrically. No UI layer enforces a 16-char minimum.
//
// The guards were originally `keySize && ...` and were changed to
// `keySize >= AES_BLOCK_SIZE && ...` in commits ea5313c and 2c26198; the change
// conflated the password with the derived key and is a regression.
//
// Fix: require a non-empty password (`keySize > 0`) in both guards.
//
// Compile: g++ -O2 -std=c++23 -o /tmp/test_crypthelper_short_password_bug test_crypthelper_short_password_bug.cpp && /tmp/test_crypthelper_short_password_bug

#include <cstdio>
#include <climits>
#include <string>
#include <vector>

static const unsigned AES_BLOCK_SIZE = 16;

// Stand-in for CryptHelper::BytesToKey(): a deterministic 16-byte key/IV
// derivation that works for ANY non-empty password length (just like SHA1).
static void deriveKeyIv(const unsigned char *keyData, unsigned keySize,
                        unsigned char key[AES_BLOCK_SIZE], unsigned char iv[AES_BLOCK_SIZE])
{
    unsigned long h1 = 0x9e3779b9UL;
    unsigned long h2 = 0x85ebca6bUL;
    for (unsigned i = 0; i < keySize; ++i) {
        h1 = h1 * 131UL + keyData[i] + 1UL;
        h2 = h2 * 137UL + (keyData[i] ^ 0x5aU) + 1UL;
    }
    for (unsigned i = 0; i < AES_BLOCK_SIZE; ++i) {
        key[i] = static_cast<unsigned char>(h1 >> ((i % 8) * 8));
        iv[i] = static_cast<unsigned char>(h2 >> ((i % 8) * 8));
    }
}

// Model of the guard at the top of AES128Encrypt() (crypthelper.cpp:351).
static bool encryptGuard(unsigned keySize, unsigned plainSize, bool buggy)
{
    const bool nonEmptyPlain = plainSize && plainSize <= UINT_MAX - 15;
    return buggy ? (keySize >= AES_BLOCK_SIZE && nonEmptyPlain)
                 : (keySize > 0 && nonEmptyPlain);
}

// Model of the guard at the top of AES128Decrypt() (crypthelper.cpp:430).
static bool decryptGuard(unsigned keySize, unsigned cipherSize, bool buggy)
{
    const bool validCipher = cipherSize && (cipherSize % AES_BLOCK_SIZE == 0);
    return buggy ? (keySize >= AES_BLOCK_SIZE && validCipher)
                 : (keySize > 0 && validCipher);
}

static bool modelEncrypt(const std::string &password, const std::string &plain,
                         bool buggy, std::vector<unsigned char> &out)
{
    const unsigned keySize = static_cast<unsigned>(password.size());
    const unsigned plainSize = static_cast<unsigned>(plain.size());
    if (!encryptGuard(keySize, plainSize, buggy)) return false;

    unsigned char key[AES_BLOCK_SIZE];
    unsigned char iv[AES_BLOCK_SIZE];
    deriveKeyIv(reinterpret_cast<const unsigned char *>(password.data()), keySize, key, iv);

    const unsigned padded = ((plainSize + AES_BLOCK_SIZE - 1) >> 4) << 4;
    out.assign(padded, 0);
    for (unsigned i = 0; i < plainSize; ++i) {
        out[i] = static_cast<unsigned char>(plain[i]) ^ key[i % AES_BLOCK_SIZE] ^ iv[i % AES_BLOCK_SIZE];
    }
    return true;
}

static bool modelDecrypt(const std::string &password, const std::vector<unsigned char> &cipher,
                         bool buggy, std::string &out)
{
    const unsigned keySize = static_cast<unsigned>(password.size());
    const unsigned cipherSize = static_cast<unsigned>(cipher.size());
    if (!decryptGuard(keySize, cipherSize, buggy)) return false;

    unsigned char key[AES_BLOCK_SIZE];
    unsigned char iv[AES_BLOCK_SIZE];
    deriveKeyIv(reinterpret_cast<const unsigned char *>(password.data()), keySize, key, iv);

    out.assign(cipherSize, '\0');
    for (unsigned i = 0; i < cipherSize; ++i) {
        out[i] = static_cast<char>(cipher[i] ^ key[i % AES_BLOCK_SIZE] ^ iv[i % AES_BLOCK_SIZE]);
    }
    return true;
}

static bool roundTrips(const std::string &password, const std::string &plain, bool buggy)
{
    std::vector<unsigned char> cipher;
    if (!modelEncrypt(password, plain, buggy, cipher)) return false;
    std::string decrypted;
    if (!modelDecrypt(password, cipher, buggy, decrypted)) return false;
    return decrypted.size() >= plain.size() && decrypted.compare(0, plain.size(), plain) == 0;
}

int main()
{
    int failures = 0;
    const std::string cardData = "1234 7 12 5 9";

    printf("Testing AES128 short-password rejection bug\n\n");

    // 1. Reproduce the reported bug: a 6-byte password fails in the buggy code.
    const std::string shortPassword = "secret";
    if (!encryptGuard(static_cast<unsigned>(shortPassword.size()), static_cast<unsigned>(cardData.size()), true)) {
        printf("PASS(bug reproduced): buggy encrypt rejects 6-byte password 'secret'\n");
    } else {
        printf("FAIL: buggy guard unexpectedly accepted a 6-byte password\n");
        ++failures;
    }
    if (!roundTrips(shortPassword, cardData, false)) {
        printf("FAIL: fixed code cannot round-trip a 6-byte password\n");
        ++failures;
    } else {
        printf("PASS: fixed code round-trips a 6-byte password\n");
    }

    // 2. Every non-empty password length must work after the fix, and the
    //    buggy guard must accept only those >= AES_BLOCK_SIZE (16) bytes.
    printf("\npassword-length matrix (plain = \"%s\"):\n", cardData.c_str());
    for (unsigned len = 1; len <= 20; ++len) {
        const std::string password(len, 'a');
        const bool buggyAccepts = encryptGuard(len, static_cast<unsigned>(cardData.size()), true);
        const bool fixedAccepts = encryptGuard(len, static_cast<unsigned>(cardData.size()), false);
        const bool fixedRoundTrips = roundTrips(password, cardData, false);
        const bool expectedBuggy = len >= AES_BLOCK_SIZE;
        printf("  len=%2u buggy=%d fixed=%d roundtrip=%d (expected buggy=%d)\n",
               len, buggyAccepts, fixedAccepts, fixedRoundTrips, expectedBuggy);
        if (!fixedAccepts || !fixedRoundTrips) {
            printf("    FAIL: fixed code rejected/broke a valid non-empty password\n");
            ++failures;
        }
        if (buggyAccepts != expectedBuggy) {
            printf("    FAIL: buggy guard model does not match crypthelper.cpp\n");
            ++failures;
        }
    }

    // 3. Empty password must still be rejected by both (callers send plaintext
    //    cards when no password is present).
    if (encryptGuard(0, static_cast<unsigned>(cardData.size()), false) ||
        decryptGuard(0, AES_BLOCK_SIZE, false)) {
        printf("\nFAIL: fix accepts an empty password\n");
        ++failures;
    } else {
        printf("\nPASS: empty password still rejected\n");
    }

    // 4. The 20-byte SHA1-derived key used by internetgamelogindialog must be
    //    accepted by both the old and new guard (no regression for valid keys).
    if (!encryptGuard(20, static_cast<unsigned>(cardData.size()), true) ||
        !encryptGuard(20, static_cast<unsigned>(cardData.size()), false) ||
        !roundTrips(std::string(20, 'k'), cardData, false)) {
        printf("FAIL: 20-byte key path broken\n");
        ++failures;
    } else {
        printf("PASS: 20-byte key path unaffected\n");
    }

    // 5. Cipher-size validation (added in 2c26198) must be preserved.
    if (decryptGuard(6, AES_BLOCK_SIZE + 1, false)) {
        printf("FAIL: malformed cipher size accepted\n");
        ++failures;
    } else {
        printf("PASS: malformed cipher size still rejected\n");
    }

    if (failures > 0) {
        printf("\nTEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("\nTEST PASSED: short-password bug reproduced and fix verified\n");
    return 0;
}
