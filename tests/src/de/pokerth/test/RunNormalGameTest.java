/*  PokerTH automated tests.
        Copyright (C) 2010 Lothar May

        This program is free software: you can redistribute it and/or modify
        it under the terms of the GNU Affero General Public License as
        published by the Free Software Foundation, either version 3 of the
        License, or (at your option) any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU Affero General Public License for more details.

        You should have received a copy of the GNU Affero General Public License
        along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

package de.pokerth.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.fail;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;

import org.junit.Test;

import de.pokerth.protocol.ProtoBuf.NetGameInfo;
import de.pokerth.protocol.ProtoBuf.PlayerResult;
import de.pokerth.protocol.ProtoBuf.StartEventAckMessage;
import de.pokerth.protocol.ProtoBuf.StartEventMessage;
import de.pokerth.protocol.ProtoBuf.NetGameInfo.EndRaiseMode;
import de.pokerth.protocol.ProtoBuf.NetGameInfo.NetGameType;
import de.pokerth.protocol.ProtoBuf.PokerTHMessage;
import de.pokerth.protocol.ProtoBuf.PokerTHMessage.PokerTHMessageType;
import de.pokerth.protocol.ProtoBuf.StartEventMessage.StartEventType;


public class RunNormalGameTest extends TestBase {

        @Test
        public void testRunNormalGameAsUser() throws Exception {
                userInit();

                PokerTHMessage msg;
                msg = receiveMessage();
                if (!msg.hasPlayerListMessage() || msg.getMessageType() != PokerTHMessageType.Type_PlayerListMessage) {
                        failOnErrorMessage(msg);
                        fail("Invalid message.");
                }

                Collection<Integer> l = new ArrayList<Integer>();
                NetGameInfo gameInfo = createGameInfo(NetGameType.normalGame, 10, 5, 5, EndRaiseMode.doubleBlinds, 0, 100, AuthUser + " run normal game", l, 10, 0, 2, 2000);
                sendMessage(createGameRequestMsg(
                                gameInfo,
                                "",
                                false));

                // Game list update (new game)
                msg = receiveMessage();
                if (!msg.hasGameListNewMessage() || msg.getMessageType() != PokerTHMessageType.Type_GameListNewMessage) {
                        failOnErrorMessage(msg);
                        fail("Invalid message.");
                }

                // Join game ack.
                msg = receiveMessage();
                if (!msg.hasJoinGameAckMessage() || msg.getMessageType() != PokerTHMessageType.Type_JoinGameAckMessage) {
                        failOnErrorMessage(msg);
                        fail("Could not create game!");
                }
                int gameId = msg.getJoinGameAckMessage().getGameId();

                // Game list update (player joined).
                msg = receiveMessage();
                if (!msg.hasGameListPlayerJoinedMessage() || msg.getMessageType() != PokerTHMessageType.Type_GameListPlayerJoinedMessage) {
                        failOnErrorMessage(msg);
                        fail("Invalid message.");
                }

                StartEventMessage startMsg = StartEventMessage.newBuilder()
                                .setGameId(gameId)
                                .setFillWithComputerPlayers(true)
                                .setStartEventType(StartEventType.startEvent)
                                .build();
                msg = PokerTHMessage.newBuilder()
                                .setMessageType(PokerTHMessageType.Type_StartEventMessage)
                                .setStartEventMessage(startMsg)
                                .build();
                sendMessage(msg);

                // Now the computer players should join.
                for (int i = 0; i < 9; i++) {
                        msg = receiveMessage();
                        if (!msg.hasGamePlayerJoinedMessage() || msg.getMessageType() != PokerTHMessageType.Type_GamePlayerJoinedMessage) {
                                failOnErrorMessage(msg);
                                fail("Invalid message.");
                        }
                        msg = receiveMessage();
                        if (!msg.hasGameListPlayerJoinedMessage() || msg.getMessageType() != PokerTHMessageType.Type_GameListPlayerJoinedMessage) {
                                failOnErrorMessage(msg);
                                fail("Invalid message.");
                        }
                }

                // Server should confirm start event.
                msg = receiveMessage();
                if (!msg.hasStartEventMessage() || msg.getMessageType() != PokerTHMessageType.Type_StartEventMessage) {
                        failOnErrorMessage(msg);
                        fail("Invalid message.");
                }
                // Acknowledge start event.
                StartEventAckMessage startAck = StartEventAckMessage.newBuilder()
                        .setGameId(gameId)
                        .build();
                msg = PokerTHMessage.newBuilder()
                        .setMessageType(PokerTHMessageType.Type_StartEventAckMessage)
                        .setStartEventAckMessage(startAck)
                        .build();
                sendMessage(msg);

                // Game list update (game now running).
                msg = receiveMessage();
                if (!msg.hasGameListUpdateMessage() || msg.getMessageType() != PokerTHMessageType.Type_GameListUpdateMessage) {
                        failOnErrorMessage(msg);
                        fail("Invalid message.");
                }

                msg = receiveMessage();
                if (!msg.hasHandStartMessage() || msg.getMessageType() != PokerTHMessageType.Type_HandStartMessage) {
                        failOnErrorMessage(msg);
                        fail("Invalid message.");
                }

                // Play game until end.
                while (true) {
                        msg = receiveMessage();
                        if (msg.hasPlayersTurnMessage()) {
                                // Ignore turn message.
                        } else if (msg.hasPlayersActionDoneMessage()) {
                                // Ignore action message.
                        } else if (msg.hasDealFlopCardsMessage() || msg.hasDealTurnCardMessage() || msg.hasDealRiverCardMessage()) {
                                // Ignore deal cards messages.
                        } else if (msg.hasEndOfHandShowCardsMessage() || msg.hasEndOfHandHideCardsMessage()) {
                                // Acknowledge hand start after hand is over.
                                msg = receiveMessage();
                                if (msg.hasHandStartMessage()) {
                                        // Next hand started.
                                } else if (msg.hasEndOfGameMessage()) {
                                        // Game is over.
                                        break;
                                } else {
                                        failOnErrorMessage(msg);
                                        fail("Invalid message.");
                                }
                        } else {
                                failOnErrorMessage(msg);
                                fail("Invalid message.");
                        }
                }

                // Verify game results.
                msg = receiveMessage();
                if (!msg.hasEndOfGameMessage() || msg.getMessageType() != PokerTHMessageType.Type_EndOfGameMessage) {
                        failOnErrorMessage(msg);
                        fail("Invalid message.");
                }

                // Final game list update (game closed).
                msg = receiveMessage();
                if (!msg.hasGameListRemoveMessage() || msg.getMessageType() != PokerTHMessageType.Type_GameListRemoveMessage) {
                        failOnErrorMessage(msg);
                        fail("Invalid message.");
                }
        }
}