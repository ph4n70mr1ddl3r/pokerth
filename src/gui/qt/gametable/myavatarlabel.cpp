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
 *                                                                           *
 *                                                                           *
 * Additional permission under GNU AGPL version 3 section 7                  *
 *                                                                           *
 * If you modify this program, or any covered work, by linking or            *
 * combining it with the OpenSSL project's OpenSSL library (or a             *
 * modified version of that library), containing parts covered by the        *
 * terms of the OpenSSL or SSLeay licenses, the authors of PokerTH           *
 * (Felix Hammer, Florian Thauer, Lothar May) grant you additional           *
 * permission to convey the resulting work.                                  *
 * Corresponding Source for a non-source form of such a combination          *
 * shall include the source code for the parts of OpenSSL used as well       *
 * as that of the covered work.                                              *
 *****************************************************************************/
#include "myavatarlabel.h"

#include "gametableimpl.h"
#include "gametablestylereader.h"
#include "session.h"
#include "playerinterface.h"
#include "game.h"
#include "mymessagedialogimpl.h"
#include "chattools.h"
#include <QSysInfo>
#include <core/loghelper.h>

using namespace std;

MyAvatarLabel::MyAvatarLabel(QGroupBox* parent)
	: QLabel(parent), voteRunning(false), transparent(false), myUniqueId(0), myPingState(1), myAvgPing(-1), myMinPing(-1), myMaxPing(-1)
{

	myContextMenu = new QMenu(this);
	action_EditTip = new QAction(QIcon(":/gfx/user_properties.png"), tr("Add/Edit/Remove tooltip"), myContextMenu);
	myContextMenu->addAction(action_EditTip);
	action_VoteForKick = new QAction(QIcon(":/gfx/list_remove_user.png"), tr("Start vote to kick this player"), myContextMenu);
	myContextMenu->addAction(action_VoteForKick);
	action_IgnorePlayer = new QAction(QIcon(":/gfx/im-ban-user.png"), tr("Ignore Player"), myContextMenu);
	myContextMenu->addAction(action_IgnorePlayer);
	action_UnignorePlayer = new QAction(QIcon(":/gfx/dialog_ok_apply.png"), tr("Unignore Player"), myContextMenu);
	myContextMenu->addAction(action_UnignorePlayer);
	action_ReportBadAvatar = new QAction(QIcon(":/gfx/emblem-important.png"), tr("Report inappropriate avatar"), myContextMenu);
	myContextMenu->addAction(action_ReportBadAvatar);

	connect( action_VoteForKick, SIGNAL ( triggered() ), this, SLOT ( sendTriggerVoteOnKickSignal() ) );
	connect( action_IgnorePlayer, SIGNAL ( triggered() ), this, SLOT ( putPlayerOnIgnoreList() ) );
	connect( action_UnignorePlayer, SIGNAL ( triggered() ), this, SLOT ( removePlayerFromIgnoreList() ) );
	connect( action_ReportBadAvatar, SIGNAL ( triggered() ), this, SLOT ( reportBadAvatar() ) );
	connect( action_EditTip, SIGNAL( triggered() ), this, SLOT ( startEditTip() ) );
}


MyAvatarLabel::~MyAvatarLabel() noexcept
{
}

void MyAvatarLabel::contextMenuEvent ( QContextMenuEvent *event )
{
	if (!myW || !myW->getSession()) {
		LOG_ERROR("MyAvatarLabel::contextMenuEvent - myW or session is null");
		return;
	}
	
	boost::shared_ptr<Game> currentGame = myW->getSession()->getCurrentGame();
	if (!currentGame) {
		LOG_ERROR("MyAvatarLabel::contextMenuEvent - currentGame is null");
		return;
	}
	if (myW->getSession()->isNetworkClientRunning()) {

		boost::shared_ptr<PlayerInterface> humanPlayer = currentGame->getSeatsList()->front();
		//only active players are allowed to start a vote
		if(humanPlayer->getMyActiveStatus()) {

			PlayerListConstIterator it_c;
			int activePlayerCounter=0;
			PlayerList seatList = currentGame->getSeatsList();
			for (it_c=seatList->begin(); it_c!=seatList->end(); ++it_c) {
				if((*it_c)->getMyActiveStatus()) activePlayerCounter++;
			}
			GameInfo info(myW->getSession()->getClientGameInfo(myW->getSession()->getClientCurrentGameId()));

			if(activePlayerCounter > 2 && !voteRunning && info.data.gameType != GAME_TYPE_RANKING) {
				setVoteOnKickContextMenuEnabled(true);
			} else {
				setVoteOnKickContextMenuEnabled(false);
			}

			action_IgnorePlayer->setEnabled(true);
			action_UnignorePlayer->setDisabled(true);
			action_EditTip->setEnabled(true);
			int j=0;
			for (it_c=seatList->begin(); it_c!=seatList->end(); ++it_c) {

				if(myId == j) {
					if(j == 0) {
						action_EditTip->setDisabled(true);
					}
					if(!myW || !myW->myStartWindow || !myW->myStartWindow->getSession()) return;
					if(myW->myStartWindow->getSession()->getGameType() == Session::GAME_TYPE_NETWORK) {
						action_EditTip->setDisabled(true);
					}

					if(myW->getSession()->getGameType() == Session::GAME_TYPE_INTERNET && !((*it_c)->getMyAvatar().empty()) ) {
						action_ReportBadAvatar->setVisible(true);
					} else {
						action_ReportBadAvatar->setVisible(false);
					}

					if(playerIsOnIgnoreList(QString::fromUtf8((*it_c)->getMyName().c_str()))) {
						action_UnignorePlayer->setEnabled(true);
						action_IgnorePlayer->setDisabled(true);
					}
				}
				j++;
			}

			int i=0;
			for (it_c=seatList->begin(); it_c!=seatList->end(); ++it_c) {

				//also inactive player which stays on table can be voted to kick
				if(myContextMenuEnabled && myId != 0 && myId == i && (*it_c)->getMyType() != PLAYER_TYPE_COMPUTER && ( (*it_c)->getMyActiveStatus() || (*it_c)->getMyStayOnTableStatus() ) )
					showContextMenu(event->globalPos());

				i++;
			}
		}
	}
}

void MyAvatarLabel::showContextMenu(const QPoint &pos)
{

	myContextMenu->popup(pos);
}

void MyAvatarLabel::setPlayerRating(QString playerInfo)
{
	if (!myW || !myW->myStartWindow || !myW->myStartWindow->getSession()) return;
	int found=0;
	QStringList playerInfoList=playerInfo.split("\"", Qt::KeepEmptyParts, Qt::CaseSensitive), tipInfo;
	boost::shared_ptr<Game> currentGame = myW->myStartWindow->getSession()->getCurrentGame();
	if (!currentGame) return;
	PlayerList seatsList = currentGame->getSeatsList();
	std::list<std::string> tipsList = myW->getMyConfig()->readConfigStringList("PlayerTooltips");
	std::list<std::string> result;
	std::string separator="(!#$%)";
	// Bug #319: https://github.com/pokerth/pokerth/issues/319
	if (playerInfoList.size() < 2) return;
	QString playerName = playerInfoList.at(0);
	playerName = QUrl::fromPercentEncoding(playerName.toUtf8());

	for(auto iterator = tipsList.begin(); iterator != tipsList.end(); ++iterator) {
		tipInfo=QString::fromUtf8(iterator->c_str()).split("(!#$%)", Qt::KeepEmptyParts, Qt::CaseSensitive);
		if(tipInfo.size() >= 2 && tipInfo.at(0)==playerName) {
			result.push_back(tipInfo.at(0).toUtf8().constData()+separator+tipInfo.at(1).toUtf8().constData()+separator+playerInfoList.at(1).toUtf8().constData()+separator);
			found=1;
		} else if(tipInfo.size() >= 3) {
			result.push_back(tipInfo.at(0).toUtf8().constData()+separator+tipInfo.at(1).toUtf8().constData()+separator+tipInfo.at(2).toUtf8().constData()+separator);
		}
	}
	if(found==0) {
		result.push_back(playerName.toUtf8().constData()+separator+separator+playerInfoList.at(1).toUtf8().constData()+separator);
	}
	myW->getMyConfig()->writeConfigStringList("PlayerTooltips", result);
	myW->getMyConfig()->writeBuffer();
	refreshStars();
}


void MyAvatarLabel::startChangePlayerTip(QString playerName)
{
}

void MyAvatarLabel::refreshStars()
{
	if (!myW || !myW->myStartWindow) return;
	auto session = myW->myStartWindow->getSession();
	if (!session) return;
	QString fontSize("12");
	QString fontFamily(myW->getMyGameTableStyle()->getFont1String());

#ifdef _WIN32
	fontSize = "10";
#else
#ifdef __APPLE__
	fontSize = "7";
#else
	fontSize = "12";
#endif
#endif

	boost::shared_ptr<Game> curGame = session->getCurrentGame();
	if (!curGame) return;
	PlayerListConstIterator it_c;
	int seatPlace = 0;
	PlayerList seatsList = curGame->getSeatsList();
	for (seatPlace=0,it_c=seatsList->begin(); it_c!=seatsList->end(); ++it_c, seatPlace++) {
		for(int i=1; i<=5; i++)myW->playerStarsArray[i][seatPlace]->setText("");
		if(!myW || !myW->myStartWindow || !myW->myStartWindow->getSession()) return;
		if(myW->myStartWindow->getSession()->getGameType() == Session::GAME_TYPE_INTERNET && (*it_c)->getMyType() != PLAYER_TYPE_COMPUTER) {
			if((*it_c)->getMyStayOnTableStatus() == true && (*it_c)->getMyName()!="" && seatPlace!=0) {

				// Bug #319: https://github.com/pokerth/pokerth/issues/319
				QString playerName = QString::fromUtf8((*it_c)->getMyName().c_str());
				int playerStars=getPlayerRating(playerName);
				playerName = QString(QUrl::toPercentEncoding(playerName));

				for(int i=1; i<=5; i++) {
					myW->playerStarsArray[i][seatPlace]->setText("<a style='color: #"+myW->getMyGameTableStyle()->getRatingStarsColor()+"; "+fontFamily+" font-size: "+fontSize+"px; text-decoration: none;' href='"+QString::fromUtf8((*it_c)->getMyName().c_str())+"\""+QString::number(i)+"'>&#9734;</a>");
				}
				for(int i=1; i<=playerStars; i++) {
					myW->playerStarsArray[i][seatPlace]->setText("<a style='color: #"+myW->getMyGameTableStyle()->getRatingStarsColor()+"; "+fontFamily+" font-size: "+fontSize+"px; text-decoration: none;' href='"+QString::fromUtf8((*it_c)->getMyName().c_str())+"\""+QString::number(i)+"'>&#9733;</a>");
				}
			}
		}
	}
}

void MyAvatarLabel::refreshTooltips()
{
	if (!myW || !myW->myStartWindow) return;
	auto session = myW->myStartWindow->getSession();
	if (!session) return;
	boost::shared_ptr<Game> currentGame = session->getCurrentGame();
	if (!currentGame) return;
	PlayerListConstIterator it_c;
	int seatPlace = 0;
	PlayerList seatsList = currentGame->getSeatsList();
	for (seatPlace=0,it_c=seatsList->begin(); it_c!=seatsList->end(); ++it_c, seatPlace++) {
		if((*it_c)->getMyStayOnTableStatus() == true || (*it_c)->getMyActiveStatus()) {
			bool computerPlayer = false;
			if((*it_c)->getMyType() == PLAYER_TYPE_COMPUTER) {
				computerPlayer = true;
			}
			if(!computerPlayer && getPlayerTip(QString::fromUtf8((*it_c)->getMyName().c_str()))!="" && seatPlace!=0) {
				myW->playerTipLabelArray[(*it_c)->getMyID()]->setText(QString("<a style='text-decoration: none; color: #"+myW->getMyGameTableStyle()->getPlayerInfoHintTextColor()+"; font-size: 14px; font-weight: bold; font-family:serif;' href=\'")+QString::fromUtf8((*it_c)->getMyName().c_str())+"\'>i</a>");
				myW->playerTipLabelArray[(*it_c)->getMyID()]->setToolTip( getPlayerTip(QString::fromUtf8((*it_c)->getMyName().c_str())) );
				myW->playerAvatarLabelArray[(*it_c)->getMyID()]->setToolTip( getPlayerTip(QString::fromUtf8((*it_c)->getMyName().c_str())) );
			} else {
				myW->playerTipLabelArray[(*it_c)->getMyID()]->setText("");
				myW->playerTipLabelArray[(*it_c)->getMyID()]->setToolTip("");
				myW->playerAvatarLabelArray[(*it_c)->getMyID()]->setToolTip("");
			}
		} else {
			myW->playerTipLabelArray[(*it_c)->getMyID()]->setText("");
			myW->playerTipLabelArray[(*it_c)->getMyID()]->setToolTip("");
			myW->playerAvatarLabelArray[(*it_c)->getMyID()]->setToolTip("");

		}


	}
	refreshStars();
}

int MyAvatarLabel::getPlayerRating(QString playerName)
{
	std::list<std::string> tipsList = myW->getMyConfig()->readConfigStringList("PlayerTooltips");
	QStringList playerInfo;
	QString result="0";
	for(auto iterator = tipsList.begin(); iterator != tipsList.end(); ++iterator) {
		playerInfo=QString::fromUtf8(iterator->c_str()).split("(!#$%)", Qt::KeepEmptyParts, Qt::CaseSensitive);
		if(playerInfo.size() >= 3 && playerInfo.at(0)==playerName) {
			result=playerInfo.at(2);
			break;
		}
	}
	return result.toInt();
}


QString MyAvatarLabel::getPlayerTip(QString playerName)
{
	std::list<std::string> tipsList = myW->getMyConfig()->readConfigStringList("PlayerTooltips");
	QStringList playerInfo;
	for(auto iterator = tipsList.begin(); iterator != tipsList.end(); ++iterator) {
		playerInfo=QString::fromUtf8(iterator->c_str()).split("(!#$%)", Qt::KeepEmptyParts, Qt::CaseSensitive);
		if(playerInfo.size() >= 2 && playerInfo.at(0)==playerName)return playerInfo.at(1);
	}
	return QString("");
}


void MyAvatarLabel::setPlayerTip()
{
}


void MyAvatarLabel::sendTriggerVoteOnKickSignal()
{
	if (!myW) return;
	myW->triggerVoteOnKick(myId);
}

void MyAvatarLabel::setEnabledContextMenu(bool b)
{
	myContextMenuEnabled = b;
}

void MyAvatarLabel::setVoteOnKickContextMenuEnabled(bool b)
{
	action_VoteForKick->setEnabled(b);
}

void MyAvatarLabel::setPixmap ( const QPixmap &pix, const bool trans)
{

	myPixmap = pix.scaled(50,50, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	transparent = trans;
	update();

}

void MyAvatarLabel::setPixmapAndCountry ( const QPixmap &pix,QString countryString, int seatPlace, const bool trans)
{

	QPixmap resultAvatar(pix.scaled(50,50, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	QPainter painter(&resultAvatar);
	int showCountryFlags = myW->getMyConfig()->readConfigInt("ShowCountryFlagInAvatar");
	if(showCountryFlags && !countryString.isEmpty()) {
		if(seatPlace<=2||seatPlace>=8) {
			painter.drawPixmap(resultAvatar.width()-(QPixmap(countryString)).width(),resultAvatar.height()-(QPixmap(countryString)).height(),QPixmap(countryString));
		} else {
			painter.drawPixmap(resultAvatar.width()-(QPixmap(countryString)).width(),0,QPixmap(countryString));
		}
	}
	painter.end();
	myPixmap = resultAvatar;
	transparent = trans;
	update();

}

void MyAvatarLabel::paintEvent(QPaintEvent*)
{
	if (!myW || !myW->myStartWindow) return;

	QPainter painter(this);
	if(transparent)
		painter.setOpacity(0.4);
	else
		painter.setOpacity(1.0);

	//hide avatar if player is on ignore list
	boost::shared_ptr<Session> mySession = myW->myStartWindow->getSession();
	if(!mySession) return;
	if(!playerIsOnIgnoreList(QString::fromUtf8(mySession->getClientPlayerInfo(myUniqueId).playerName.c_str()))) {
		painter.drawPixmap(0,0,myPixmap);
	} else if(myW->getMyConfig()->readConfigInt("DontHideAvatarsOfIgnored")) {
		painter.drawPixmap(0,0,myPixmap);
	}

	if(myW->getMyConfig()->readConfigInt("ShowPingStateInAvatar")) {
		//paint ping state color for network clients
		if(mySession->isNetworkClientRunning() && myId == 0) {
			QColor pingColor;
			if(myAvgPing >= 0 && myAvgPing <= 1000) {
#if QT_VERSION >= 0x060600
				pingColor = QColor::fromString("green");
			} else if(myAvgPing > 1000 && myAvgPing <= 2000 ) {
				pingColor = QColor::fromString("yellow");
			} else if(myAvgPing > 2000) {
				pingColor = QColor::fromString("red");
			} else {
				pingColor = QColor::fromString("white");
#else
				pingColor.setNamedColor("green");
			} else if(myAvgPing > 1000 && myAvgPing <= 2000 ) {
				pingColor.setNamedColor("yellow");
			} else if(myAvgPing > 2000) {
				pingColor.setNamedColor("red");
			} else {
				pingColor.setNamedColor("white");
#endif
			}
			QColor pen = pingColor.darker(200);
			//		pen.setAlpha(130);
			painter.setPen(pen);
			QColor brush = pingColor;
			//		brush.setAlpha(130);
			painter.setBrush(brush);
			painter.setRenderHint(QPainter::Antialiasing);
			painter.drawEllipse(1, 39, 10, 10);
		}
	}
}

void MyAvatarLabel::refreshPing(unsigned minPing, unsigned avgPing, unsigned maxPing)
{
	if (!myW) return;
	myMinPing = minPing;
	myAvgPing = avgPing;
	myMaxPing = maxPing;
	this->update();

	if(myW->getMyConfig()->readConfigInt("ShowPingStateInAvatar")) {
		QString toolTip = "<b>"+tr("Server response times")+"</b>";
		toolTip.append("<br>"+tr("Average: ")+QString("%1").arg(myAvgPing)+tr("ms"));
		toolTip.append("<br>"+tr("Minimum: ")+QString("%1").arg(myMinPing)+tr("ms"));
		toolTip.append("<br>"+tr("Maximum: ")+QString("%1").arg(myMaxPing)+tr("ms"));
		this->setToolTip(toolTip);
	} else {
		this->setToolTip("");
	}
}

bool MyAvatarLabel::playerIsOnIgnoreList(QString playerName)
{

	list<std::string> playerIgnoreList = myW->getMyConfig()->readConfigStringList("PlayerIgnoreList");
	for(auto it1= playerIgnoreList.begin(); it1 != playerIgnoreList.end(); ++it1) {

		if(playerName == QString::fromUtf8(it1->c_str())) {
			return true;
		}
	}
	return false;
}


void MyAvatarLabel::putPlayerOnIgnoreList()
{
	if (!myW->getSession() || !myW->getSession()->getCurrentGame())
		return;
	QStringList list;
	PlayerListConstIterator it_c;
	PlayerList seatList = myW->getSession()->getCurrentGame()->getSeatsList();
	for (it_c=seatList->begin(); it_c!=seatList->end(); ++it_c) {
		list << QString::fromUtf8((*it_c)->getMyName().c_str());
	}

	if(!playerIsOnIgnoreList(list.at(myId))) {
		myMessageDialogImpl dialog(myW->getMyConfig(), this);
		if(dialog.exec(IGNORE_PLAYER_QUESTION, tr("You will no longer receive chat messages or game invitations from this user.<br>Do you really want to put player <b>%1</b> on ignore list?").arg(list.at(myId)), tr("PokerTH - Question"), QPixmap(":/gfx/im-ban-user_64.png"), QDialogButtonBox::Yes|QDialogButtonBox::No, false ) == QDialog::Accepted) {
			std::list<std::string> playerIgnoreList = myW->getMyConfig()->readConfigStringList("PlayerIgnoreList");
			playerIgnoreList.push_back(list.at(myId).toUtf8().constData());
			myW->getMyConfig()->writeConfigStringList("PlayerIgnoreList", playerIgnoreList);
			myW->getMyConfig()->writeBuffer();
			myW->getMyChat()->refreshIgnoreList();
		}
	}
}


void MyAvatarLabel::removePlayerFromIgnoreList()
{
	if (!myW->getSession() || !myW->getSession()->getCurrentGame())
		return;
	QStringList list;
	PlayerListConstIterator it_c;
	PlayerList seatList = myW->getSession()->getCurrentGame()->getSeatsList();
	for (it_c=seatList->begin(); it_c!=seatList->end(); ++it_c) {
		list << QString::fromUtf8((*it_c)->getMyName().c_str());
	}

	if(playerIsOnIgnoreList(list.at(myId))) {
		myMessageDialogImpl dialog(myW->getMyConfig(), this);
		if(dialog.exec(UNIGNORE_PLAYER_QUESTION, tr("You will receive chat messages and game invitations from this user again!<br>Do you really want to remove player <b>%1</b> from your ignore list?").arg(list.at(myId)), tr("PokerTH - Question"), QPixmap(":/gfx/dialog_ok_apply.png"), QDialogButtonBox::Yes|QDialogButtonBox::No, false ) == QDialog::Accepted) {
			std::list<std::string> playerIgnoreList = myW->getMyConfig()->readConfigStringList("PlayerIgnoreList");
			playerIgnoreList.remove(list.at(myId).toUtf8().constData());
			myW->getMyConfig()->writeConfigStringList("PlayerIgnoreList", playerIgnoreList);
			myW->getMyConfig()->writeBuffer();
			myW->getMyChat()->refreshIgnoreList();
		}
	}
}

void MyAvatarLabel::reportBadAvatar()
{
	if (!myW->getSession() || !myW->getSession()->getCurrentGame())
		return;

	boost::shared_ptr<Game> currentGame = myW->getSession()->getCurrentGame();
	int j=0;
	PlayerListConstIterator it_c;
	PlayerList seatList = currentGame->getSeatsList();
	for (it_c=seatList->begin(); it_c!=seatList->end(); ++it_c) {

		if(myId == j) {

			QString avatar = QString::fromUtf8((*it_c)->getMyAvatar().c_str());
			if(!avatar.isEmpty()) {

				QString nick = QString::fromUtf8((*it_c)->getMyName().c_str());
				int ret = MyMessageBox::question(this, tr("PokerTH - Question"),
												 tr("Are you sure you want to report the avatar of \"%1\" as inappropriate?").arg(nick), QMessageBox::Yes | QMessageBox::No);

				if(ret == QMessageBox::Yes) {
					QFileInfo fi(avatar);
					myW->getSession()->reportBadAvatar((*it_c)->getMyUniqueID(), fi.baseName().toStdString());
				}
			}
			break;
		}
		j++;
	}
}


void MyAvatarLabel::startEditTip()
{
	if (!myW->getSession() || !myW->getSession()->getCurrentGame())
		return;
	boost::shared_ptr<Game> currentGame = myW->getSession()->getCurrentGame();
	int j=0;
	PlayerListConstIterator it_c;
	PlayerList seatList = currentGame->getSeatsList();
	for (it_c=seatList->begin(); it_c!=seatList->end(); ++it_c) {
		if(myId == j) {
			QString nick = QString::fromUtf8((*it_c)->getMyName().c_str());
			startChangePlayerTip(nick);
			break;
		}
		j++;
	}
}
