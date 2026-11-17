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

// Regression test: ClientStateSynchronizeStart silently drops the
// GameStartRejoinMessage, so a rejoining client never starts the game.
//
// Background
// ----------
// In src/net/clientstate.cpp a client that receives a StartEventMessage
// (normal OR rejoin) is moved into ClientStateSynchronizeStart
// (ClientStateWaitGame::InternalHandlePacket, line ~1330):
//
//     if (netStartEvent.starteventtype() == StartEventMessage::rejoinEvent)
//         Signal...(MSG_NET_GAME_CLIENT_SYNCREJOIN);
//     else
//         Signal...(MSG_NET_GAME_CLIENT_SYNCSTART);
//     client->SetState(ClientStateSynchronizeStart::Instance());
//
// ClientStateSynchronizeStart::TimerLoop (line ~1385) polls IsSynchronized()
// every 50 ms; once synchronized it sends StartEventAckMessage and moves to
// ClientStateWaitStart. That is the *only* scheduled exit from this state.
//
// The server does NOT hold the game-start packet until the client acks.
// AbstractServerGameStateRunning::HandleNewPlayer sends StartEventMessage
// immediately, and at the next hand boundary InitRejoiningPlayers/PerformRejoin
// sends the one and only GameStartRejoinMessage. There is no resend.
//
// The bug
// -------
// ClientStateSynchronizeStart::InternalHandlePacket (line ~1411) only forwards
// GameStartInitialMessage to the next state:
//
//     if (tmpPacket->GetMsg()->messagetype() ==
//             PokerTHMessage::Type_GameStartInitialMessage) {
//         client->UnsubscribeLobbyMsg();
//         client->SetState(ClientStateWaitStart::Instance());
//         client->GetState().HandlePacket(client, tmpPacket);
//     }
//
// A GameStartRejoinMessage arriving while the client is still synchronizing
// matches neither branch: it is consumed and discarded. The client later
// acks and reaches ClientStateWaitStart, but its only start message is gone.
// ClientStateWaitStart (which handles BOTH variants and calls InitGame() +
// moves to ClientStateWaitHand, lines ~1451-1515) never sees it, so the
// rejoining player's game never starts.
//
// The sibling handler ClientStateWaitStart::InternalHandlePacket already
// accepts both variants, which makes this a classic copy/paste omission.
//
// The fix
// -------
// Accept the rejoin variant in the SynchronizeStart forward path as well:
//
//     if (type == Type_GameStartInitialMessage
//             || type == Type_GameStartRejoinMessage) { ... forward ... }
//
// This test models the three relevant states and the two forwarding handlers
// (current vs fixed). It asserts that the current handler loses the rejoin
// packet (bug reproduced) and that the fixed handler delivers it to WaitStart
// where InitGame() runs. The normal-start path is included as a control and
// must behave identically in both versions.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_rejoin_start_dropped_bug test_rejoin_start_dropped_bug.cpp
//   /tmp/test_rejoin_start_dropped_bug

#include <cstdio>

enum class State {
    WaitGame,
    SynchronizeStart,
    WaitStart,
    WaitHand,
};

enum class Packet {
    StartEventRejoin,
    GameStartInitial,
    GameStartRejoin,
};

struct Client {
    State state = State::WaitGame;
    bool gameInitialized = false;
    int initGameCalls = 0;
    int lobbySubscriptions = 1;
};

// ClientStateWaitGame::InternalHandlePacket: both start-event variants move
// the client into the synchronize state.
static void waitGameHandlePacket(Client& c, Packet p)
{
    if (p == Packet::StartEventRejoin) {
        c.state = State::SynchronizeStart;
    }
}

// ClientStateWaitStart::InternalHandlePacket: the real handler (lines
// ~1451-1515) accepts both start variants, calls InitGame() and enters
// WaitHand. Here we only model the observable side effects.
static void waitStartHandlePacket(Client& c, Packet p)
{
    if (p == Packet::GameStartInitial || p == Packet::GameStartRejoin) {
        c.gameInitialized = true;
        ++c.initGameCalls;
        c.state = State::WaitHand;
    }
}

// Simulates ClientStateSynchronizeStart::TimerLoop: when the client is
// synchronized it acks and moves to WaitStart. A packet that was dropped while
// in SynchronizeStart is never replayed after this transition.
static void finishSynchronization(Client& c)
{
    if (c.state == State::SynchronizeStart) {
        c.lobbySubscriptions = 0; // UnsubscribeLobbyMsg()
        c.state = State::WaitStart;
    }
}

// The shipped ClientStateSynchronizeStart::InternalHandlePacket: forwards only
// GameStartInitialMessage. A GameStartRejoinMessage falls through and is lost.
static void syncStartHandlePacketBuggy(Client& c, Packet p)
{
    if (p == Packet::GameStartInitial) {
        c.lobbySubscriptions = 0;
        c.state = State::WaitStart;
        waitStartHandlePacket(c, p); // forward to the next state
    }
    // GameStartRejoin is silently discarded.
}

// The fixed handler: forwards both start message variants.
static void syncStartHandlePacketFixed(Client& c, Packet p)
{
    if (p == Packet::GameStartInitial || p == Packet::GameStartRejoin) {
        c.lobbySubscriptions = 0;
        c.state = State::WaitStart;
        waitStartHandlePacket(c, p); // forward to the next state
    }
}

struct Result {
    bool gameInitialized;
    State state;
    int initGameCalls;
};

// Full rejoin timeline: start event -> (still synchronizing) game-start packet
// -> synchronization completes. The packet handler differs by version.
static Result rejoinTimeline(void (*handler)(Client&, Packet))
{
    Client c;
    waitGameHandlePacket(c, Packet::StartEventRejoin); // -> SynchronizeStart
    handler(c, Packet::GameStartRejoin);                // arrives early
    finishSynchronization(c);                           // timer finally fires
    return {c.gameInitialized, c.state, c.initGameCalls};
}

static const char* stateName(State s)
{
    switch (s) {
    case State::WaitGame: return "WaitGame";
    case State::SynchronizeStart: return "SynchronizeStart";
    case State::WaitStart: return "WaitStart";
    case State::WaitHand: return "WaitHand";
    }
    return "?";
}

int main()
{
    int failures = 0;

    printf("Testing GameStartRejoinMessage drop in ClientStateSynchronizeStart:\n\n");

    // --- 1. bug reproduction: current handler loses the rejoin packet ---
    Result bug = rejoinTimeline(&syncStartHandlePacketBuggy);
    printf("  current: gameInitialized=%s state=%s initGameCalls=%d\n",
           bug.gameInitialized ? "true" : "false", stateName(bug.state),
           bug.initGameCalls);
    bool bugReproduced = !bug.gameInitialized && bug.state == State::WaitStart
                         && bug.initGameCalls == 0;
    printf("    [%s] rejoin packet lost, game never starts\n\n",
           bugReproduced ? "bug reproduced" : "NO DIVERGENCE");
    if (!bugReproduced) ++failures;

    // --- 2. fix verification: rejoin packet reaches WaitStart -> InitGame ---
    Result fixed = rejoinTimeline(&syncStartHandlePacketFixed);
    printf("  fixed:   gameInitialized=%s state=%s initGameCalls=%d\n",
           fixed.gameInitialized ? "true" : "false", stateName(fixed.state),
           fixed.initGameCalls);
    bool fixWorks = fixed.gameInitialized && fixed.state == State::WaitHand
                    && fixed.initGameCalls == 1;
    printf("    [%s] game initialized exactly once, entered WaitHand\n\n",
           fixWorks ? "PASS" : "FAIL");
    if (!fixWorks) ++failures;

    // --- 3. control: normal start must behave identically in both versions ---
    Client buggyNormal, fixedNormal;
    waitGameHandlePacket(buggyNormal, Packet::StartEventRejoin);
    waitGameHandlePacket(fixedNormal, Packet::StartEventRejoin);
    syncStartHandlePacketBuggy(buggyNormal, Packet::GameStartInitial);
    syncStartHandlePacketFixed(fixedNormal, Packet::GameStartInitial);
    bool controlOk = buggyNormal.gameInitialized && fixedNormal.gameInitialized
                     && buggyNormal.state == State::WaitHand
                     && fixedNormal.state == State::WaitHand
                     && buggyNormal.initGameCalls == 1
                     && fixedNormal.initGameCalls == 1;
    printf("  control (normal GameStartInitial): buggy=%s fixed=%s [%s]\n",
           buggyNormal.gameInitialized ? "started" : "stuck",
           fixedNormal.gameInitialized ? "started" : "stuck",
           controlOk ? "PASS" : "FAIL");
    if (!controlOk) ++failures;

    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }

    printf("\nTEST PASSED: rejoin start drop reproduced and fix verified\n");
    return 0;
}
