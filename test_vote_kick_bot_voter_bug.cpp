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

// Regression test: ServerGame::TimerVoteKick() used two different definitions
// of "player" for a vote-kick petition, so computer players were incorrectly
// counted as still-eligible voters even though they can never cast a vote.
//
// Background
// ----------
// src/net/servergame.cpp, ServerGame::InternalAskVoteKick():
//
//   line 609: size_t numPlayers =
//                 GetSessionManager().GetPlayerIdList(SessionData::Game).size();
//   line 626: numVotesToKick = ceil(numPlayers * 2.0 / 3.0);
//
// The kick threshold is derived from CONNECTED HUMAN sessions only
// (SessionManager::GetPlayerIdList walks m_sessionMap; bots have no session).
//
// src/net/servergame.cpp, ServerGame::TimerVoteKick():
//
//   line 259: const PlayerIdList playerIds(GetPlayerIdList());  <-- humans + bots
//   line 266: if (find(votedPlayerIds..., *player_i) == end) playersAllowedToVote++;
//   line 281: else if (votesRequiredToKick > playersAllowedToVote)  // abort
//
// ServerGame::GetPlayerIdList() (line 776) appends every computer player id
// from m_computerPlayerList. Only real sessions can vote (InternalVoteKick,
// line 690: byWhom->GetPlayerData()->GetUniqueId()). Bots can never vote, so
// counting them as "still allowed to vote" keeps playersAllowedToVote
// artificially high and the "no longer possible" abort never fires.
//
// The fix
// -------
// Build the eligible-voter list from the same human-only population used for
// the threshold:
//   const PlayerIdList playerIds(
//       GetSessionManager().GetPlayerIdList(SessionData::Game));
//
// This standalone test models the relevant subset of the petition state and
// checks both the buggy predicate (humans + bots) and the fixed one (humans
// only) across several scenarios.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

typedef std::vector<int> PlayerIdList;

struct Petition {
    int numVotesToKick;
    int numVotesInFavour;
    PlayerIdList votedIds;
};

static bool hasVoted(const PlayerIdList& votedIds, int playerId)
{
    return std::find(votedIds.begin(), votedIds.end(), playerId) != votedIds.end();
}

// Mirrors TimerVoteKick() lines 260-281. `eligibleIds` is the population being
// counted as "still allowed to vote"; a petition is aborted early (reason
// PETITION_END_NOT_ENOUGH_PLAYERS) when the remaining required votes exceed the
// number of eligible players who have not voted yet.
static bool shouldAbortNotEnoughPlayers(const Petition& p, const PlayerIdList& eligibleIds)
{
    const int votesRequiredToKick = p.numVotesToKick - p.numVotesInFavour;
    int playersAllowedToVote = 0;
    for (PlayerIdList::const_iterator it = eligibleIds.begin(); it != eligibleIds.end(); ++it) {
        if (!hasVoted(p.votedIds, *it))
            ++playersAllowedToVote;
    }
    return votesRequiredToKick > playersAllowedToVote;
}

static PlayerIdList combine(const PlayerIdList& humans, const PlayerIdList& bots)
{
    PlayerIdList all = humans;
    all.insert(all.end(), bots.begin(), bots.end());
    return all;
}

static int computeThreshold(size_t numHumans)
{
    return static_cast<int>(std::ceil(numHumans * 2.0 / 3.0));
}

static int failures = 0;

static void check(const char* label, bool ok)
{
    std::printf("  %-64s %s\n", label, ok ? "PASS" : "FAIL");
    if (!ok)
        ++failures;
}

struct Scenario {
    const char* name;
    size_t numHumansAtStart;
    PlayerIdList humans;
    PlayerIdList bots;
    int numVotesInFavour;
    PlayerIdList votedIds;
    bool expectedAbort;
};

int main()
{
    // 3 humans (ids 1,2,3) and 3 bots (ids 101,102,103). Kick target is id 2.
    // Threshold = ceil(3 * 2/3) = 2.
    const PlayerIdList humans3{1, 2, 3};
    const PlayerIdList bots3{101, 102, 103};

    const Scenario scenarios[] = {
        // All humans voted; only 1 in-favour vote, 2 needed. Bots keep the
        // buggy count high so the abort never fires.
        {
            "all humans voted, kick impossible (3 bots)",
            3, humans3, bots3, 1, {1, 2, 3},
            true
        },
        // Still winnable: only one human voted, one more in-favour vote needed
        // and two humans remain. Both versions correctly continue.
        {
            "kick still possible, one human left to vote",
            3, humans3, bots3, 1, {1},
            false
        },
        // No bots in the game: buggy and fixed populations are identical.
        {
            "all humans voted, no bots in game",
            3, humans3, {}, 1, {1, 2, 3},
            true
        },
        // One human left the game after the petition started; only two humans
        // remain and both have voted. Abort must fire.
        {
            "humans left, remaining humans already voted",
            3, {1, 2}, bots3, 1, {1, 2},
            true
        },
    };

    std::printf("Testing vote-kick eligible-voter population (bot voter bug):\n\n");

    for (const Scenario& s : scenarios) {
        Petition p;
        p.numVotesToKick = computeThreshold(s.numHumansAtStart);
        p.numVotesInFavour = s.numVotesInFavour;
        p.votedIds = s.votedIds;

        const PlayerIdList buggyPopulation = combine(s.humans, s.bots);
        const bool buggyAbort = shouldAbortNotEnoughPlayers(p, buggyPopulation);
        const bool fixedAbort = shouldAbortNotEnoughPlayers(p, s.humans);

        std::printf("%s\n", s.name);
        std::printf("  numVotesToKick=%d, numVotesInFavour=%d, expectedAbort=%s\n",
                    p.numVotesToKick, p.numVotesInFavour,
                    s.expectedAbort ? "yes" : "no");
        std::printf("  buggy (humans+ bots) abort=%s | fixed (humans only) abort=%s\n",
                    buggyAbort ? "yes" : "no", fixedAbort ? "yes" : "no");

        check("fixed predicate matches expected behavior", fixedAbort == s.expectedAbort);
    }

    std::printf("\nVerifying the bug is real (buggy diverges on bot-only scenario):\n");

    Petition p;
    p.numVotesToKick = computeThreshold(humans3.size());
    p.numVotesInFavour = 1;
    p.votedIds = PlayerIdList{1, 2, 3};

    const bool buggyAbort = shouldAbortNotEnoughPlayers(p, combine(humans3, bots3));
    const bool fixedAbort = shouldAbortNotEnoughPlayers(p, humans3);

    check("buggy keeps petition open (abort=false)", !buggyAbort);
    check("fixed aborts with NOT_ENOUGH_PLAYERS (abort=true)", fixedAbort);
    check("bug reproduced: buggy and fixed differ", buggyAbort != fixedAbort);

    if (failures) {
        std::printf("\nTEST FAILED (%d failure(s))\n", failures);
        return 1;
    }

    std::printf("\nTEST PASSED: vote-kick bot-voter bug reproduced and fix verified\n");
    return 0;
}
