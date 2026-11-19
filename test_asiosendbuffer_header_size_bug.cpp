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

// Regression test: AsioSendBuffer::InternalStorePacket() admitted serialized
// packets whose size was up to MAX_SEND_BUF_SIZE, but the send buffer is later
// asked to hold the packet PLUS the 4-byte NET_HEADER_SIZE. Because
// ReallocSendBuf() can never grow the buffer beyond MAX_SEND_BUF_SIZE, any
// packet with rawSize in [MAX_SEND_BUF_SIZE - NET_HEADER_SIZE + 1,
// MAX_SEND_BUF_SIZE] passes the guard but then makes EncodeToBuf() return -1.
// The packet is silently dropped (only a generic log line is emitted).
//
// The fix tightens the guard to:
//     rawSize > MAX_SEND_BUF_SIZE - NET_HEADER_SIZE
// so a packet is accepted only if its header + body actually fit.
//
// This standalone test models the exact sizing logic used by
// InternalStorePacket(), EncodeToBuf() and ReallocSendBuf() and checks both the
// buggy and fixed bounds at and around the boundary.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace {

constexpr std::size_t SEND_BUF_FIRST_ALLOC_CHUNKSIZE = 4096;
constexpr std::size_t MAX_SEND_BUF_SIZE = SEND_BUF_FIRST_ALLOC_CHUNKSIZE * 256; // 1 MiB
constexpr std::size_t NET_HEADER_SIZE = 4;

// Mirrors the guard in AsioSendBuffer::InternalStorePacket().
bool buggyGuardAdmits(std::size_t rawSize)
{
    return !(rawSize > MAX_SEND_BUF_SIZE ||
             rawSize > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()));
}

// The corrected guard: leave room for the network header.
bool fixedGuardAdmits(std::size_t rawSize)
{
    return !(rawSize > MAX_SEND_BUF_SIZE - NET_HEADER_SIZE ||
             rawSize > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()));
}

// Mirrors EncodeToBuf() + ReallocSendBuf(): reallocation is capped at
// MAX_SEND_BUF_SIZE, so a request larger than that can never be satisfied.
bool canActuallyEncode(std::size_t rawSize)
{
    const std::size_t required = rawSize + NET_HEADER_SIZE;
    return required <= MAX_SEND_BUF_SIZE;
}

int failures = 0;

void check(const char* label, bool ok)
{
    std::printf("  %-70s %s\n", label, ok ? "PASS" : "FAIL");
    if (!ok)
        ++failures;
}

} // namespace

int main()
{
    std::printf("Testing AsioSendBuffer packet-size guard (header off-by-4 bug):\n\n");

    // --- Normal, small packets: both guards behave identically and correctly.
    const std::size_t normalSizes[] = {1, 16, 384, 8192, 65536};
    for (std::size_t size : normalSizes) {
        std::printf("normal rawSize=%zu\n", size);
        check("buggy guard admits", buggyGuardAdmits(size));
        check("fixed guard admits", fixedGuardAdmits(size));
        check("packet is actually encodable", canActuallyEncode(size));
    }

    // --- The last size that fits exactly: body + header == MAX_SEND_BUF_SIZE.
    const std::size_t exactFit = MAX_SEND_BUF_SIZE - NET_HEADER_SIZE;
    std::printf("\nboundary rawSize=%zu (body + header exactly fills the buffer)\n", exactFit);
    check("buggy guard admits", buggyGuardAdmits(exactFit));
    check("fixed guard admits", fixedGuardAdmits(exactFit));
    check("packet is actually encodable", canActuallyEncode(exactFit));

    // --- The bug window: these sizes pass the buggy guard but cannot be encoded.
    std::printf("\nbug window rawSize in (%zu, %zu]:\n", exactFit, MAX_SEND_BUF_SIZE);
    for (std::size_t size = exactFit + 1; size <= MAX_SEND_BUF_SIZE; ++size) {
        std::printf("  rawSize=%zu\n", size);
        check("buggy guard admits it (WRONG)", buggyGuardAdmits(size));
        check("not actually encodable (dropped)", !canActuallyEncode(size));
        check("fixed guard rejects it", !fixedGuardAdmits(size));
    }

    // --- Beyond the maximum: both guards reject, nothing to encode.
    std::printf("\noversized rawSize=%zu\n", MAX_SEND_BUF_SIZE + 1);
    check("buggy guard rejects", !buggyGuardAdmits(MAX_SEND_BUF_SIZE + 1));
    check("fixed guard rejects", !fixedGuardAdmits(MAX_SEND_BUF_SIZE + 1));

    // --- Prove the bug is real: there exists a size the buggy guard accepts
    //     that cannot be encoded, while the fixed guard never accepts such a
    //     size.
    std::printf("\nverifying the bug is real:\n");
    bool foundDivergence = false;
    bool fixedNeverAcceptsUnencodable = true;
    for (std::size_t size = 0; size <= MAX_SEND_BUF_SIZE + 16; ++size) {
        const bool buggyAccepts = buggyGuardAdmits(size);
        const bool fixedAccepts = fixedGuardAdmits(size);
        const bool encodable = canActuallyEncode(size);
        if (buggyAccepts && !encodable)
            foundDivergence = true;
        if (fixedAccepts && !encodable)
            fixedNeverAcceptsUnencodable = false;
    }
    check("buggy guard accepts an unencodable packet", foundDivergence);
    check("fixed guard never accepts an unencodable packet", fixedNeverAcceptsUnencodable);

    if (failures) {
        std::printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }

    std::printf("\nTEST PASSED: send-buffer header off-by-4 bug reproduced and fix verified\n");
    return 0;
}
