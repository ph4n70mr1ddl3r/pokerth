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
#include "internetgamelogindialogimpl.h"
#include "configfile.h"
#include <tools.h>
#include <QtCore>
#include <QtGui>
#include <crypthelper.h>
#include <QHostInfo>
#include <QProcessEnvironment>
#include <random>

namespace {
	
	std::string getEncryptionKey() {
		QString machineId = QHostInfo::localHostName();
		QString userId = QProcessEnvironment::systemEnvironment().value("USER", 
						 QProcessEnvironment::systemEnvironment().value("USERNAME", "default"));
		QString combined = machineId + ":" + userId + ":pokerth";
		
		SHA1Buf hash;
		CryptHelper::SHA1Hash(
			reinterpret_cast<const unsigned char*>(combined.toUtf8().constData()),
			combined.toUtf8().size(),
			hash
		);
		
		return std::string(reinterpret_cast<const char*>(hash.GetData()), hash.GetDataSize());
	}
	
	bool encryptPassword(const QString& password, QString& encrypted) {
		std::string key = getEncryptionKey();
		std::string plainStr = password.toUtf8().constData();
		std::vector<unsigned char> cipher;
		
		if (!CryptHelper::AES128Encrypt(
			reinterpret_cast<const unsigned char*>(key.data()),
			static_cast<unsigned>(key.size()),
			plainStr,
			cipher)) {
			return false;
		}
		
		encrypted = QByteArray(
			reinterpret_cast<const char*>(cipher.data()),
			static_cast<int>(cipher.size())
		).toBase64();
		return true;
	}
	
	bool decryptPassword(const QString& encrypted, QString& password) {
		std::string key = getEncryptionKey();
		QByteArray cipherData = QByteArray::fromBase64(encrypted.toUtf8());
		
		std::string plainStr;
		if (!CryptHelper::AES128Decrypt(
			reinterpret_cast<const unsigned char*>(key.data()),
			static_cast<unsigned>(key.size()),
			reinterpret_cast<const unsigned char*>(cipherData.constData()),
			static_cast<unsigned>(cipherData.size()),
			plainStr)) {
			return false;
		}
		
		password = QString::fromUtf8(plainStr.c_str());
		return true;
	}
}

internetGameLoginDialogImpl::internetGameLoginDialogImpl(QWidget *parent, ConfigFile *c) :
	QDialog(parent), myConfig(c)
{
	setupUi(this);
	this->installEventFilter(this);
#ifdef ANDROID
	this->setWindowState(Qt::WindowFullScreen);
#endif
	//html stuff
	QString createAccount(QString("<a href='https://create-gaming-account.pokerth.net'>%1</a>").arg(tr("Create new user account")));
	label_createAnAccount->setText(createAccount);


	connect(lineEdit_password, SIGNAL(textEdited(QString)), this, SLOT(okButtonCheck()));
	connect(lineEdit_username, SIGNAL(textEdited(QString)), this, SLOT(okButtonCheck()));
}

int internetGameLoginDialogImpl::exec()
{
	lineEdit_username->setText(QString::fromUtf8(myConfig->readConfigString("MyName").c_str()));
	if(myConfig->readConfigInt("InternetSavePassword")) {
		checkBox_rememberPassword->setChecked(true);
		QString encrypted = QString::fromUtf8(myConfig->readConfigString("InternetLoginPassword").c_str());
		QString password;
		if (!decryptPassword(encrypted, password)) {
			password.clear();
		}
		lineEdit_password->setText(password);
	} else {
		checkBox_rememberPassword->setChecked(false);
		lineEdit_password->clear();
	}

	okButtonCheck();

	if(!checkBox_rememberPassword->isChecked() && lineEdit_password->text().isEmpty()) {
		lineEdit_password->setFocus();
	}

	return QDialog::exec();
}

void internetGameLoginDialogImpl::accept()
{
	myConfig->writeConfigInt("InternetLoginMode", 0);
	myConfig->writeConfigString("MyName", lineEdit_username->text().toUtf8().constData());
	if(checkBox_rememberPassword->isChecked()) {
		myConfig->writeConfigInt("InternetSavePassword", 1);
		QString encrypted;
		if (encryptPassword(lineEdit_password->text(), encrypted)) {
			myConfig->writeConfigString("InternetLoginPassword", encrypted.toUtf8().constData());
		} else {
			myConfig->writeConfigString("InternetLoginPassword", "");
		}
	} else {
		myConfig->writeConfigInt("InternetSavePassword", 0);
	}

	myConfig->writeBuffer();

	QDialog::accept();
}

void internetGameLoginDialogImpl::okButtonCheck()
{
	if(!lineEdit_password->text().isEmpty() && !lineEdit_username->text().isEmpty()) {
		pushButton_login->setEnabled(true);
	} else {
		pushButton_login->setEnabled(false);
	}
}


bool internetGameLoginDialogImpl::eventFilter(QObject *obj, QEvent *event)
{
#ifdef ANDROID
	QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

	//androi changes for return key behavior (hopefully useless from necessitas beta2)
	if (event->type() == QEvent::KeyPress && keyEvent->key() == Qt::Key_Return) {
		if(lineEdit_username->hasFocus()) {
			lineEdit_password->setFocus();
		}
		if(lineEdit_password->hasFocus()) {
			QTimer::singleShot(1000, this, SLOT(clickLoginButton()));
		}
		event->ignore();
		return false;
	} else {
		// pass the event on to the parent class
		return QDialog::eventFilter(obj, event);
	}
#else
	return QDialog::eventFilter(obj, event);
#endif
}
