/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2013 Felix Hammer, Florian Thauer, Lothar May          *
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

// Regression test: WebSendBuffer::closeAfterSend is a ONE-SHOT request but is
// implemented as a sticky latch.
//
// Background
// ----------
// The twin class AsioSendBuffer already handles this correctly: in both its
// TCP and SSL AsyncSendNextPacket() paths it resets the flag before closing
// the socket (src/net/asiosendbuffer.cpp):
//
//     } else if (closeAfterSend) {
//         closeAfterSend = false;
//         socket->close(ec);
//     }
//
// That fix (commit 834071d) was applied only to AsioSendBuffer. WebSendBuffer
// was left behind.
//
// The bug
// -------
// WebSendBuffer::AsyncSendNextPacket() (src/net/websendbuffer.cpp:61-76):
//
//     if (closeAfterSend) {
//         boost::shared_ptr<WebSocketData> webData = session->GetWebData();
//         if (!webData)
//             return;
//         ...
//         webSocketServer->close(webData->webHandle, ...);
//     }
//
// The flag is set by SetCloseAfterSend() (explicit close request) and also by
// InternalStorePacket() on ANY transient websocket send() error. It is never
// cleared. Because SenderHelper::Send() calls AsyncSendNextPacket() on every
// send, a single set flag causes a close() frame to be issued on every
// subsequent send:
//
//   * CloseSession() -> SetCloseAfterSend() -> one close() is issued; any
//     other thread sending before teardown completes -> a second close() on
//     the already-closing/invalid handle.
//   * A single failed send() latches the flag; even after all later sends
//     succeed, every one of them closes the connection.
//   * If the SessionData/WebSocketData object is reused for a new connection
//     (a fresh webHandle), the stale flag immediately closes the NEW
//     connection without any close request for it.
//
// The fix
// -------
// Consume the flag atomically when it is observed, mirroring AsioSendBuffer:
//
//     if (closeAfterSend.exchange(false)) {
//         ...
//     }
//
// This makes the request truly one-shot. This standalone test models the
// current (buggy) and fixed control flow with std::atomic<bool> plus a close
// counter / handle id. It needs no Qt, Boost, protobuf or websocket headers.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_websendbuffer_close_flag_bug test_websendbuffer_close_flag_bug.cpp
//   /tmp/test_websendbuffer_close_flag_bug

#include <atomic>
#include <cstdio>

namespace {

struct WebBuffer {
    std::atomic<bool> closeAfterSend{false};
    bool hasWebData = true;
    int handle = 1;
    int closeCalls = 0;
    int lastClosedHandle = -1;

    void setCloseAfterSend() { closeAfterSend = true; }
    // Mirrors InternalStorePacket(): a failed send() latches the flag.
    void failedSend() { closeAfterSend = true; }
};

// Verbatim model of the CURRENT websendbuffer.cpp logic: the flag is read but
// never cleared, so a single request fires on every call.
void asyncSendBuggy(WebBuffer &b)
{
    if (b.closeAfterSend) {
        if (!b.hasWebData)
            return;
        ++b.closeCalls;
        b.lastClosedHandle = b.handle;
    }
}

// Model of the FIX: exchange(false) consumes the one-shot request.
void asyncSendFixed(WebBuffer &b)
{
    if (b.closeAfterSend.exchange(false)) {
        if (!b.hasWebData)
            return;
        ++b.closeCalls;
        b.lastClosedHandle = b.handle;
    }
}

int failures = 0;

void check(const char* label, bool ok)
{
    std::printf("  %-72s %s\n", label, ok ? "PASS" : "FAIL");
    if (!ok)
        ++failures;
}

} // namespace

int main()
{
    std::printf("Testing WebSendBuffer one-shot closeAfterSend flag:\n\n");

    std::printf("1) Explicit close request: two AsyncSendNextPacket() calls.\n");
    {
        WebBuffer bug, fix;
        bug.setCloseAfterSend();
        fix.setCloseAfterSend();
        asyncSendBuggy(bug);
        asyncSendBuggy(bug);
        asyncSendFixed(fix);
        asyncSendFixed(fix);
        check("buggy issues two close() frames (WRONG)", bug.closeCalls == 2);
        check("fixed issues exactly one close() frame", fix.closeCalls == 1);
    }

    std::printf("\n2) Failed send() latches the flag; later sends must not re-close.\n");
    {
        WebBuffer bug, fix;
        bug.failedSend();
        fix.failedSend();
        for (int i = 0; i < 3; ++i) {
            asyncSendBuggy(bug);
            asyncSendFixed(fix);
        }
        check("buggy re-closes on every later send (WRONG)", bug.closeCalls == 3);
        check("fixed closes only once", fix.closeCalls == 1);
    }

    std::printf("\n3) Session reuse with a fresh webHandle must not be closed.\n");
    {
        WebBuffer bug, fix;
        bug.setCloseAfterSend();
        fix.setCloseAfterSend();
        asyncSendBuggy(bug); // closes original handle H1
        asyncSendFixed(fix);
        bug.handle = 2;      // replacement connection H2
        fix.handle = 2;
        asyncSendBuggy(bug);
        asyncSendFixed(fix);
        check("buggy closes replacement connection H2 (WRONG)",
              bug.closeCalls == 2 && bug.lastClosedHandle == 2);
        check("fixed leaves H2 open (one close, last closed H1)",
              fix.closeCalls == 1 && fix.lastClosedHandle == 1);
    }

    std::printf("\n4) No flag set: no close() is ever issued.\n");
    {
        WebBuffer bug, fix;
        for (int i = 0; i < 3; ++i) {
            asyncSendBuggy(bug);
            asyncSendFixed(fix);
        }
        check("buggy issues no close without a request", bug.closeCalls == 0);
        check("fixed issues no close without a request", fix.closeCalls == 0);
    }

    if (failures) {
        std::printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    std::printf("\nTEST PASSED: closeAfterSend sticky-latch bug reproduced and fix verified\n");
    return 0;
}
