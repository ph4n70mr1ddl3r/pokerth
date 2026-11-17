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

// Regression test: stale/replayed MyActionRequestMessage crosses hand boundaries.
//
// Background
// ----------
// MyActionRequestMessage carries a handNum field (pokerth.proto:437). The client
// fills it with game->getCurrentHandID() (src/net/clientthread.cpp:188), and the
// packet validator checks only that it is non-zero
// (src/net/netpacketvalidator.cpp:725-738). It is therefore part of the wire
// contract that identifies which hand an action belongs to.
//
// The bug
// -------
// ServerGameStateWaitPlayerAction::InternalProcessPacket()
// (src/net/servergamestate.cpp:1563-1599) validates the game ID, the round
// (gamestate), that the sender is the current player, and that the action is
// legal -- but it never validates netMyAction->handnum() against
// curGame.getCurrentHandID(). A delayed, duplicated or maliciously replayed
// action request for hand N is therefore accepted during hand N+1 whenever the
// round happens to coincide (e.g. both actions are preflop). The stale action
// is applied with whatever stack/cards the player has in the new hand, which
// corrupts the game state and can be used to act out of turn across hands.
//
// The fix
// -------
// Reject the action when handnum does not match the server's current hand ID,
// using the existing ACTION_CODE_INVALID_STATE rejection path (the client now
// surfaces YourActionRejectedMessage). Legitimate actions carry the current
// hand number, so they are unaffected.
//
// This test models the validation logic of InternalProcessPacket. It asserts
// that the current logic accepts a stale hand-N request during hand N+1 (bug
// reproduced), that the fixed logic rejects it, and that legitimate actions,
// wrong-round actions and not-your-turn actions still behave correctly.
//
// Compile:
//   g++ -O2 -std=c++23 -Wall -Wextra -o /tmp/test_stale_hand_action test_stale_hand_action.cpp
//   /tmp/test_stale_hand_action

#include <cstdio>

enum PlayerActionCode {
    ACTION_CODE_VALID = 0,
    ACTION_CODE_INVALID_STATE = 1,
    ACTION_CODE_NOT_YOUR_TURN = 2,
    ACTION_CODE_NOT_ALLOWED = 3,
};

// The fields from MyActionRequestMessage that the server inspects.
struct ActionRequest {
    unsigned handNum;
    int gameState;      // round the client believes is active
    unsigned playerId;
};

// The server-side state the request is checked against.
struct ServerContext {
    unsigned currentHandId;
    int currentRound;
    unsigned currentPlayerId;
};

// Model of the CURRENT validation in InternalProcessPacket: game ID was already
// checked; the round, the current-player check and checkMyAction are evaluated,
// but handNum is ignored. checkMyAction is simplified to "always legal" here so
// the test isolates the hand-number gap.
static PlayerActionCode validateBuggy(const ServerContext& ctx, const ActionRequest& req)
{
    PlayerActionCode code = ACTION_CODE_VALID;

    if (ctx.currentRound != req.gameState)
        code = ACTION_CODE_INVALID_STATE;

    if (code == ACTION_CODE_VALID && ctx.currentPlayerId != req.playerId)
        code = ACTION_CODE_NOT_YOUR_TURN;

    // BUG: req.handNum is never compared to ctx.currentHandId.
    (void)ctx.currentHandId;

    return code;
}

// Model of the FIXED validation: the hand number must match as well.
static PlayerActionCode validateFixed(const ServerContext& ctx, const ActionRequest& req)
{
    PlayerActionCode code = ACTION_CODE_VALID;

    if (ctx.currentRound != req.gameState
            || req.handNum != ctx.currentHandId)
        code = ACTION_CODE_INVALID_STATE;

    if (code == ACTION_CODE_VALID && ctx.currentPlayerId != req.playerId)
        code = ACTION_CODE_NOT_YOUR_TURN;

    return code;
}

static const char* codeName(PlayerActionCode code)
{
    switch (code) {
    case ACTION_CODE_VALID:         return "VALID";
    case ACTION_CODE_INVALID_STATE: return "INVALID_STATE";
    case ACTION_CODE_NOT_YOUR_TURN: return "NOT_YOUR_TURN";
    case ACTION_CODE_NOT_ALLOWED:   return "NOT_ALLOWED";
    }
    return "UNKNOWN";
}

// A request from hand 7 replayed while the server runs hand 8, same round.
static bool testStaleHandAcceptedByCurrentCode()
{
    ServerContext ctx{8, 0 /* preflop */, 42};
    ActionRequest req{7, 0 /* preflop */, 42};

    const PlayerActionCode code = validateBuggy(ctx, req);
    const bool reproduced = (code == ACTION_CODE_VALID);
    printf("  current code, stale hand 7 in hand 8: %s [%s]\n",
           codeName(code), reproduced ? "bug reproduced" : "NO DIVERGENCE");
    return reproduced;
}

static bool testFixRejectsStaleHand()
{
    ServerContext ctx{8, 0, 42};
    ActionRequest req{7, 0, 42};

    const PlayerActionCode code = validateFixed(ctx, req);
    const bool ok = (code == ACTION_CODE_INVALID_STATE);
    printf("  fixed code, stale hand 7 in hand 8:   %s [%s]\n",
           codeName(code), ok ? "PASS" : "FAIL");
    return ok;
}

static bool testFixAcceptsCurrentHand()
{
    ServerContext ctx{8, 0, 42};
    ActionRequest req{8, 0, 42};

    const PlayerActionCode code = validateFixed(ctx, req);
    const bool ok = (code == ACTION_CODE_VALID);
    printf("  fixed code, current hand 8:           %s [%s]\n",
           codeName(code), ok ? "PASS" : "FAIL");
    return ok;
}

static bool testFixStillRejectsWrongRound()
{
    ServerContext ctx{8, 0, 42};
    ActionRequest req{8, 1 /* flop */, 42};

    const PlayerActionCode code = validateFixed(ctx, req);
    const bool ok = (code == ACTION_CODE_INVALID_STATE);
    printf("  fixed code, wrong round:              %s [%s]\n",
           codeName(code), ok ? "PASS" : "FAIL");
    return ok;
}

static bool testFixStillRejectsNotYourTurn()
{
    ServerContext ctx{8, 0, 42};
    ActionRequest req{8, 0, 99};

    const PlayerActionCode code = validateFixed(ctx, req);
    const bool ok = (code == ACTION_CODE_NOT_YOUR_TURN);
    printf("  fixed code, not your turn:            %s [%s]\n",
           codeName(code), ok ? "PASS" : "FAIL");
    return ok;
}

int main()
{
    int failures = 0;
    printf("Testing stale-hand MyActionRequestMessage validation:\n\n");

    if (!testStaleHandAcceptedByCurrentCode()) ++failures;

    if (!testFixRejectsStaleHand()) ++failures;
    if (!testFixAcceptsCurrentHand()) ++failures;
    if (!testFixStillRejectsWrongRound()) ++failures;
    if (!testFixStillRejectsNotYourTurn()) ++failures;

    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }
    printf("\nTEST PASSED: stale-hand action acceptance reproduced and fix verified\n");
    return 0;
}
