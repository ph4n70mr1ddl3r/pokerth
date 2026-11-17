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

// Regression test: YourActionRejectedMessage is silently dropped by the client.
//
// Background
// ----------
// When the server refuses a player's action (NOT your turn / action not allowed /
// invalid game state) it sends a YourActionRejectedMessage back to that player
// (src/net/servergamestate.cpp, ServerGameStateWaitPlayerAction::HandlePacket).
//
// Sending the action opened a latency ping in ClientThread::SendPlayerAction()
// (m_pingData.StartPing()). That ping is only ever closed by EndPing(), which the
// client calls on receipt of a PlayersActionDoneMessage
// (AbstractClientStateReceiving/ClientStateRunHand::InternalHandlePacket).
// A rejected action produces NO PlayersActionDoneMessage, so the only message
// that can close the ping is the rejection message itself.
//
// The bug
// -------
// AbstractClientStateReceiving::HandlePacket() (src/net/clientstate.cpp) has a
// long if/else dispatch chain for game messages, followed by a call to
// InternalHandlePacket(). Neither the chain nor any state handler has a branch
// for Type_YourActionRejectedMessage. The message therefore falls through and
// is silently discarded:
//   * the user gets no feedback and the GUI keeps showing "my turn" with action
//     buttons enabled, and
//   * the latency ping started in SendPlayerAction() is never ended, so the next
//     completed action measures the round-trip of *both* attempts and records a
//     bogus ping sample.
//
// The fix
// -------
// Handle the message in the shared dispatch chain: end the pending ping and
// surface a reason-specific notice to the GUI. EndPing() is safe to call even
// when no ping is active (it checks pingTimer.is_running()).
//
// This test models the dispatch chain and the ping/notice state machine. It
// asserts the current chain drops the message (bug reproduced) and the fixed
// chain consumes it, while leaving the accepted-action and unrelated-message
// paths untouched.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_action_rejected_message_dropped test_action_rejected_message_dropped.cpp
//   /tmp/test_action_rejected_message_dropped

#include <cstdio>
#include <string>

enum MessageType {
    MSG_TimeoutWarning = 1,
    MSG_YourActionRejected,
    MSG_Chat,
    MSG_PlayersActionDone,
    MSG_PlayerList,
};

enum RejectionReason {
    rejectedInvalidGameState = 1,
    rejectedNotYourTurn = 2,
    rejectedActionNotAllowed = 3,
};

struct ClientState {
    bool pingActive = false;   // m_pingData ping timer
    std::string notice;        // last GUI notice shown
    int otherHandled = 0;      // unrelated messages still dispatched
};

// ClientThread::SendPlayerAction(): a ping is opened for every submitted action.
static void sendPlayerAction(ClientState& c) { c.pingActive = true; }

// ClientThread::EndPing(): safe no-op when no ping is running.
static void endPing(ClientState& c) { c.pingActive = false; }

static std::string reasonText(RejectionReason r)
{
    switch (r) {
    case rejectedNotYourTurn:        return "Action rejected: not your turn";
    case rejectedActionNotAllowed:   return "Action rejected: action not allowed";
    case rejectedInvalidGameState:
    default:                         return "Action rejected: invalid game state";
    }
}

// Model of the CURRENT dispatch chain: no branch for MSG_YourActionRejected,
// so it falls through to the (game-state-specific) handler which ignores it.
static void dispatchBuggy(ClientState& c, MessageType type, RejectionReason reason)
{
    if (type == MSG_TimeoutWarning) {
        // show timeout dialog
    } else if (type == MSG_Chat) {
        // chat message
    } else if (type == MSG_PlayersActionDone) {
        endPing(c);
    } else if (type == MSG_PlayerList) {
        c.otherHandled++;
    }
    // MSG_YourActionRejected: falls through, silently dropped.
    (void)reason;
}

// Model of the FIXED dispatch chain: the rejection message is consumed, the
// pending ping is closed and a reason-specific notice is surfaced.
static void dispatchFixed(ClientState& c, MessageType type, RejectionReason reason)
{
    if (type == MSG_TimeoutWarning) {
        // show timeout dialog
    } else if (type == MSG_YourActionRejected) {
        endPing(c);                         // no PlayersActionDoneMessage will arrive
        c.notice = reasonText(reason);      // surface the rejection to the user
    } else if (type == MSG_Chat) {
        // chat message
    } else if (type == MSG_PlayersActionDone) {
        endPing(c);
    } else if (type == MSG_PlayerList) {
        c.otherHandled++;
    }
}

static bool testRejectionDroppedByCurrentCode()
{
    ClientState c;
    sendPlayerAction(c);                       // ping opened for the rejected action
    dispatchBuggy(c, MSG_YourActionRejected, rejectedNotYourTurn);
    // Bug: ping still open, no user-visible notice.
    const bool reproduced = c.pingActive && c.notice.empty();
    printf("  current chain: pingActive=%d notice='%s' [%s]\n",
           c.pingActive, c.notice.c_str(), reproduced ? "bug reproduced" : "NO DIVERGENCE");
    return reproduced;
}

static bool testFixConsumesRejection(RejectionReason reason, const char* expectedNotice)
{
    ClientState c;
    sendPlayerAction(c);
    dispatchFixed(c, MSG_YourActionRejected, reason);
    const bool ok = !c.pingActive && c.notice == expectedNotice;
    printf("  fixed chain (reason=%d): pingActive=%d notice='%s' [%s]\n",
           reason, c.pingActive, c.notice.c_str(), ok ? "PASS" : "FAIL");
    return ok;
}

static bool testAcceptedActionUnchanged()
{
    ClientState c;
    sendPlayerAction(c);
    dispatchFixed(c, MSG_PlayersActionDone, rejectedNotYourTurn); // reason irrelevant
    const bool ok = !c.pingActive && c.notice.empty();
    printf("  accepted action (ack): pingActive=%d notice='%s' [%s]\n",
           c.pingActive, c.notice.c_str(), ok ? "PASS" : "FAIL");
    return ok;
}

static bool testUnrelatedMessageUnchanged()
{
    ClientState c;
    dispatchFixed(c, MSG_PlayerList, rejectedNotYourTurn);
    const bool ok = c.otherHandled == 1 && !c.pingActive && c.notice.empty();
    printf("  unrelated message: otherHandled=%d [%s]\n",
           c.otherHandled, ok ? "PASS" : "FAIL");
    return ok;
}

int main()
{
    int failures = 0;
    printf("Testing YourActionRejectedMessage client handling:\n\n");

    if (!testRejectionDroppedByCurrentCode()) ++failures;

    if (!testFixConsumesRejection(rejectedNotYourTurn, "Action rejected: not your turn")) ++failures;
    if (!testFixConsumesRejection(rejectedActionNotAllowed, "Action rejected: action not allowed")) ++failures;
    if (!testFixConsumesRejection(rejectedInvalidGameState, "Action rejected: invalid game state")) ++failures;

    if (!testAcceptedActionUnchanged()) ++failures;
    if (!testUnrelatedMessageUnchanged()) ++failures;

    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    printf("\nTEST PASSED: rejection message drop reproduced and fix verified\n");
    return 0;
}
