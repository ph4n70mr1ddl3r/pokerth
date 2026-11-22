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
// Regression test: ServerAcceptHelper configured a truncated TLS 1.2 cipher list.
//
// src/net/serveraccepthelper.h built its TLS 1.2 cipher list as
//     std::string ciphers = "ECDHE-RSA-AES128-GCM-SHA256:...";
// The literal "..." is not a valid OpenSSL cipher token. OpenSSL silently
// ignores it, so only a single TLS 1.2 cipher (ECDHE-RSA-AES128-GCM-SHA256)
// ends up configured instead of the intended full list that the WebSocket
// server uses in serveracceptwebhelper.cpp. This needlessly rejects TLS 1.2
// clients that do not support that one exact cipher.
//
// This test reads the cipher string directly out of the real source file and
// asks OpenSSL (loaded dynamically via dlopen, since dev headers are optional)
// how many TLS 1.2 ciphers it configures. It fails on the buggy source (1) and
// passes once the full cipher list is restored (many).
//
// Build/run from the repository root:
//   g++ -O2 -std=c++23 -o test_tls_cipher_list_bug test_tls_cipher_list_bug.cpp -ldl
//   ./test_tls_cipher_list_bug

#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <string>

typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_method_st SSL_METHOD;
typedef struct ssl_cipher_st SSL_CIPHER;
typedef struct stack_st STACK;

using TlsServerMethodFn = const SSL_METHOD* (*)();
using SslCtxNewFn = SSL_CTX* (*)(const SSL_METHOD*);
using SslCtxFreeFn = void (*)(SSL_CTX*);
using SslCtxSetCipherListFn = int (*)(SSL_CTX*, const char*);
using SslCtxGetCiphersFn = const STACK* (*)(const SSL_CTX*);
using SkNumFn = int (*)(const STACK*);
using SkValueFn = SSL_CIPHER* (*)(const STACK*, int);
using CipherGetNameFn = const char* (*)(const SSL_CIPHER*);

static std::string findSourceFile()
{
    const char* candidates[] = {
        "src/net/serveraccepthelper.h",
        "../src/net/serveraccepthelper.h",
        "../../src/net/serveraccepthelper.h",
    };
    for (const char* path : candidates) {
        std::ifstream f(path);
        if (f.good())
            return path;
    }
    return "";
}

static bool extractCipherList(const std::string& path, std::string& out)
{
    std::ifstream f(path);
    if (!f.good())
        return false;

    const std::string marker = "ciphers = \"";
    std::string line;
    while (std::getline(f, line)) {
        size_t pos = line.find(marker);
        if (pos == std::string::npos)
            continue;
        size_t begin = pos + marker.size();
        size_t end = line.find('"', begin);
        if (end == std::string::npos)
            return false;
        out = line.substr(begin, end - begin);
        return !out.empty();
    }
    return false;
}

int main()
{
    const std::string sourcePath = findSourceFile();
    if (sourcePath.empty()) {
        printf("FAIL: could not locate src/net/serveraccepthelper.h\n");
        return 2;
    }

    std::string ciphers;
    if (!extractCipherList(sourcePath, ciphers)) {
        printf("FAIL: could not extract cipher list from %s\n", sourcePath.c_str());
        return 2;
    }
    printf("Source: %s\n", sourcePath.c_str());
    printf("Cipher list in source: %s\n\n", ciphers.c_str());

    void* ssl = dlopen("libssl.so.3", RTLD_LAZY);
    if (!ssl) ssl = dlopen("libssl.so", RTLD_LAZY);
    if (!ssl) {
        printf("SKIP: libssl not available (%s)\n", dlerror());
        return 77;
    }

    auto tlsServerMethod = reinterpret_cast<TlsServerMethodFn>(dlsym(ssl, "TLS_server_method"));
    auto sslCtxNew = reinterpret_cast<SslCtxNewFn>(dlsym(ssl, "SSL_CTX_new"));
    auto sslCtxFree = reinterpret_cast<SslCtxFreeFn>(dlsym(ssl, "SSL_CTX_free"));
    auto setCipherList = reinterpret_cast<SslCtxSetCipherListFn>(dlsym(ssl, "SSL_CTX_set_cipher_list"));
    auto getCiphers = reinterpret_cast<SslCtxGetCiphersFn>(dlsym(ssl, "SSL_CTX_get_ciphers"));
    auto skNum = reinterpret_cast<SkNumFn>(dlsym(ssl, "OPENSSL_sk_num"));
    auto skValue = reinterpret_cast<SkValueFn>(dlsym(ssl, "OPENSSL_sk_value"));
    auto cipherName = reinterpret_cast<CipherGetNameFn>(dlsym(ssl, "SSL_CIPHER_get_name"));
    if (!tlsServerMethod || !sslCtxNew || !sslCtxFree || !setCipherList || !getCiphers || !skNum || !skValue || !cipherName) {
        printf("SKIP: required OpenSSL symbols not found\n");
        dlclose(ssl);
        return 77;
    }

    SSL_CTX* ctx = sslCtxNew(tlsServerMethod());
    if (!ctx) {
        printf("SKIP: could not create SSL_CTX\n");
        dlclose(ssl);
        return 77;
    }

    int setResult = setCipherList(ctx, ciphers.c_str());
    printf("SSL_CTX_set_cipher_list() returned: %d\n", setResult);

    int tls12Count = 0;
    if (setResult == 1) {
        const STACK* stack = getCiphers(ctx);
        int total = skNum(stack);
        printf("Configured ciphers (%d):\n", total);
        for (int i = 0; i < total; ++i) {
            const char* name = cipherName(static_cast<SSL_CIPHER*>(skValue(stack, i)));
            // TLS 1.3 suite names start with "TLS_"; everything else is TLS 1.2.
            bool isTls13 = name && strncmp(name, "TLS_", 4) == 0;
            printf("  %s%s\n", name ? name : "(null)", isTls13 ? "  [TLS 1.3]" : "");
            if (!isTls13)
                ++tls12Count;
        }
    }
    printf("\nTLS 1.2 ciphers configured: %d (expected more than 1)\n", tls12Count);

    sslCtxFree(ctx);
    dlclose(ssl);

    if (setResult != 1 || tls12Count <= 1) {
        printf("\nTEST FAILED: cipher list from serveraccepthelper.h configures too few TLS 1.2 ciphers.\n");
        printf("The literal '...' is not a cipher token; it silently truncates the list to a\n");
        printf("single cipher, rejecting compatible TLS 1.2 clients. Restore the full list.\n");
        return 1;
    }

    printf("\nTEST PASSED: full TLS 1.2 cipher list is configured\n");
    return 0;
}
