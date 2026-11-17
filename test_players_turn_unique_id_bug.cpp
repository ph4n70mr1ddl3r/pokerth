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

// Regression test for the wrong player-id stored on a "next player's turn"
// message in ClientStateRunHand::InternalHandlePacket (src/net/clientstate.cpp).
//
// The handler looks up the player by the unique id carried on the wire
// (netPlayersTurn.playerid()), but then stored the *seat index* into the BeRo:
//
//     bero->setCurrentPlayersTurnId(tmpPlayer->getMyID());   // BUG
//
// Every consumer of currentPlayersTurnId treats it as a unique id:
//   - Game::getCurrentPlayer() -> getPlayerByUniqueId()          (game.cpp:266)
//   - LocalBeRo/LocalBeRoPreflop -> getRunningPlayerIt(id)        (match getMyUniqueID)
// and every other writer stores getMyUniqueID()
//   (localhand.cpp:550, clienthand.cpp:358, localbero.cpp:232, ...).
//
// In a network game the GUI player is remapped to seat 0 while keeping the
// server-assigned unique id, and other seats need not match any player's unique
// id.  Storing the seat index therefore either resolves to no player
// (ERR_CURRENT_PLAYER_NOT_FOUND) or -- worse -- to a different player whose
// unique id happens to equal the seat index, silently giving the turn to the
// wrong seat.
//
// This test models both id spaces, the buggy and corrected handlers, and the
// downstream unique-id lookups.  It asserts the fixed handler always resolves
// the intended player and reproduces the divergence of the buggy handler.

#include <cstdio>
#include <string>
#include <vector>

struct Player {
    int myID;                 // seat index (0-based), what getMyID() returns
    unsigned int myUniqueID;  // server-assigned id, what getMyUniqueID() returns
    const char* name;
};

struct BeRo {
    unsigned int currentPlayersTurnId{0};

    void setCurrentPlayersTurnId(unsigned int id) { currentPlayersTurnId = id; }
    [[nodiscard]] unsigned int getCurrentPlayersTurnId() const { return currentPlayersTurnId; }
};

// Models Game::getPlayerByUniqueId(): lookup is by unique id, never by seat.
static const Player* getPlayerByUniqueId(const std::vector<Player>& players,
                                         unsigned int uniqueId)
{
    for (const auto& p : players)
        if (p.myUniqueID == uniqueId)
            return &p;
    return nullptr;
}

// The buggy write from clientstate.cpp:1833 -- stores the seat index.
static void handlePlayersTurnBuggy(BeRo& bero, const Player& tmpPlayer)
{
    bero.setCurrentPlayersTurnId(static_cast<unsigned int>(tmpPlayer.myID));
}

// The corrected write -- stores the unique id, like every other writer/consumer.
static void handlePlayersTurnFixed(BeRo& bero, const Player& tmpPlayer)
{
    bero.setCurrentPlayersTurnId(tmpPlayer.myUniqueID);
}

struct Scenario {
    const char* name;
    unsigned int wirePlayerId;   // netPlayersTurn.playerid()
    const char* expectedName;    // player who actually has the turn
};

int main()
{
    // A network game: seat ids are local (GUI player is seat 0) and unique ids
    // are server-assigned. Note seat 2 collides with Alice's unique id (2).
    const std::vector<Player> players = {
        {0, 7u, "Alice (GUI)"},
        {1, 12u, "Bob"},
        {2, 25u, "Carol"},
        {3, 2u, "Dave"},
    };

    const Scenario scenarios[] = {
        {"GUI player's turn", 7u, "Alice (GUI)"},
        {"Bob's turn", 12u, "Bob"},
        {"Carol's turn", 25u, "Carol"},
        {"Dave's turn (seat/unique overlap)", 2u, "Dave"},
    };

    int failures = 0;

    printf("Testing unique-id propagation for PlayersTurnMessage:\n\n");

    for (const auto& s : scenarios) {
        const Player* tmpPlayer = getPlayerByUniqueId(players, s.wirePlayerId);
        if (!tmpPlayer) {
            printf("  %-36s setup FAIL (wire id not found)\n", s.name);
            ++failures;
            continue;
        }

        BeRo fixedBero;
        handlePlayersTurnFixed(fixedBero, *tmpPlayer);
        const Player* fixedResolved =
            getPlayerByUniqueId(players, fixedBero.getCurrentPlayersTurnId());
        const bool fixedOk = fixedResolved && fixedResolved->name == s.expectedName;
        printf("  %-36s stored=%u -> %-12s [%s]\n",
               s.name, fixedBero.getCurrentPlayersTurnId(),
               fixedResolved ? fixedResolved->name : "<none>", fixedOk ? "PASS" : "FAIL");
        if (!fixedOk)
            ++failures;

        BeRo buggyBero;
        handlePlayersTurnBuggy(buggyBero, *tmpPlayer);
        const Player* buggyResolved =
            getPlayerByUniqueId(players, buggyBero.getCurrentPlayersTurnId());
        const bool buggyDiverges =
            !buggyResolved || buggyResolved->name != s.expectedName;
        printf("    buggy stored=%u -> %-12s [%s]\n",
               buggyBero.getCurrentPlayersTurnId(),
               buggyResolved ? buggyResolved->name : "<none>",
               buggyDiverges ? "bug reproduced" : "NO DIVERGENCE");
        if (!buggyDiverges)
            ++failures;
    }

    // The critical silent-corruption case: Carol (seat 2) gets the turn, but
    // the buggy seat index 2 resolves to Dave (unique id 2), a different player.
    const Player& carol = players[2];
    BeRo buggyBero;
    handlePlayersTurnBuggy(buggyBero, carol);
    const Player* wronglyResolving = getPlayerByUniqueId(players, buggyBero.getCurrentPlayersTurnId());
    const bool silentWrongPlayer = wronglyResolving && wronglyResolving->name == std::string("Dave");
    printf("\n  silent wrong-player case (buggy): seat %d -> unique id %u -> %s [%s]\n",
           carol.myID, buggyBero.getCurrentPlayersTurnId(),
           wronglyResolving ? wronglyResolving->name : "<none>",
           silentWrongPlayer ? "bug reproduced" : "NO DIVERGENCE");
    if (!silentWrongPlayer)
        ++failures;

    if (failures) {
        printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }

    printf("\nTEST PASSED: unique-id turn bug reproduced and fix verified\n");
    return 0;
}
