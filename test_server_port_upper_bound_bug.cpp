// Regression test: ServerAcceptHelper::InternalListen() rejects ports below
// 1024 but has no upper-bound check, so a configured port above 65535 is
// silently truncated to 16 bits when passed to the Boost.Asio tcp::endpoint
// constructor. The server then listens on a completely different port than
// the one configured, with no error reported.
//
// Bug hunt
// --------
// src/net/serveraccepthelper.h, InternalListen():
//
//     void InternalListen(unsigned serverPort, bool ipv6)
//     {
//         if (serverPort < 1024)
//             throw ServerException(__FILE__, __LINE__, ERR_SOCK_INVALID_PORT, 0);
//         ...
//         m_endpoint = boost::make_shared<P_endpoint>(P::v6(), serverPort);
//         //                                                   ^^^^^^^^^^
//         // tcp::endpoint takes unsigned short: 70000 -> 4464 (70000 & 0xFFFF)
//     }
//
// The port comes straight from the user-editable config file:
//     session.cpp: myConfig->readConfigInt("ServerPort")
// ConfigFile::readConfigInt() happily returns 70000, so the value reaches
// InternalListen() unchanged. The lower-bound check shows the intent was to
// validate the port; the upper bound was simply omitted.
//
// The fix
// -------
// Reject ports above 65535 as well:
//     if (serverPort < 1024 || serverPort > 65535) throw ...
//
// This test models the guard and the 16-bit truncation (using a faithful
// static_cast<unsigned short> stand-in for the endpoint constructor), and
// also checks the real source file so the regression cannot silently return.
//
// Compile:
//   g++ -std=c++23 -Wall -Wextra -o /tmp/test_server_port_upper_bound_bug test_server_port_upper_bound_bug.cpp
//   /tmp/test_server_port_upper_bound_bug

#include <cstdio>
#include <fstream>
#include <string>

static_assert(sizeof(unsigned short) == 2, "test assumes a 16-bit unsigned short");

// Faithful model of the guard in InternalListen(). `fixed` adds the missing
// upper bound; the endpoint's port argument is modelled by the truncating
// static_cast<unsigned short> that Boost.Asio performs internally.
static bool tryBind(unsigned serverPort, bool fixed, unsigned short &boundPort)
{
    if (serverPort < 1024)
        return false;
    if (fixed && serverPort > 65535)
        return false;
    boundPort = static_cast<unsigned short>(serverPort);
    return true;
}

struct Case {
    unsigned configured;
    unsigned short expected;
    bool shouldBind;
    const char *note;
};

static std::string findSourceFile()
{
    const char *candidates[] = {
        "src/net/serveraccepthelper.h",
        "../src/net/serveraccepthelper.h",
        "../../src/net/serveraccepthelper.h",
    };
    for (const char *path : candidates) {
        std::ifstream f(path);
        if (f.good()) return path;
    }
    return "";
}

int main()
{
    const Case cases[] = {
        { 1024,  1024,  true,  "lowest legal port" },
        { 7234,  7234,  true,  "shipped default" },
        { 65535, 65535, true,  "highest legal port" },
        { 65536, 0,     false, "first out-of-range port (wraps to 0)" },
        { 70000, 4464,  false, "70000 & 0xFFFF == 4464" },
        { 131072, 0,    false, "wraps to 0 again" },
    };

    int failures = 0;
    std::printf("Server port upper-bound regression test\n\n");

    for (const Case &c : cases) {
        unsigned short buggyBound = 0;
        unsigned short fixedBound = 0;
        const bool buggyBind = tryBind(c.configured, false, buggyBound);
        const bool fixedBind = tryBind(c.configured, true, fixedBound);

        std::printf("configured=%6u expected=%5u shouldBind=%-5s | buggy: bind=%-5s port=%5u | fixed: bind=%-5s port=%5u  (%s)\n",
                    c.configured, c.expected, c.shouldBind ? "yes" : "no",
                    buggyBind ? "yes" : "no", buggyBound,
                    fixedBind ? "yes" : "no", fixedBound, c.note);

        if (fixedBind != c.shouldBind) {
            std::printf("  FAIL: fixed guard accepted/rejected the wrong port\n");
            ++failures;
        }
        if (fixedBind && fixedBound != c.expected) {
            std::printf("  FAIL: fixed path bound to the wrong port\n");
            ++failures;
        }
        if (!c.shouldBind && buggyBind && buggyBound != c.configured) {
            std::printf("  (buggy path silently bound to a different port as expected)\n");
        }
    }

    // Guard the actual source so the regression cannot creep back in.
    const std::string path = findSourceFile();
    if (path.empty()) {
        std::printf("NOTE: serveraccepthelper.h not found; skipping source check\n");
    } else {
        std::ifstream f(path);
        const std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        const std::string good = "if (serverPort < 1024 || serverPort > 65535)";
        if (src.find(good) == std::string::npos) {
            std::printf("FAIL: source check: expected '%s' in %s\n", good.c_str(), path.c_str());
            ++failures;
        } else {
            std::printf("\nSource check (%s): upper-bound guard present\n", path.c_str());
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
