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
#include "badwordcheck.h"

#include <QtCore>

BadWordCheck::BadWordCheck()
{
}

bool BadWordCheck::run(QString msg)
{
	QMutexLocker locker(&m_mutex);
	msg = msg.toLower();

	// Remove whitelisted phrases before scanning for bad words. An exception
	// allows only the bad word inside that phrase; testing it against the whole
	// message would let one allowed phrase (e.g. "all-in-idiot") exempt every
	// other occurrence of the bad word in the same message. Replacing each
	// occurrence with a space keeps text on either side from joining into a new
	// bad word across the removed phrase.
	for (const QString &exception : badWordsException) {
		if (exception.isEmpty()) {
			continue;
		}
		int exPos = msg.indexOf(exception);
		while (exPos != -1) {
			msg.replace(exPos, exception.length(), QStringLiteral(" "));
			exPos = msg.indexOf(exception, exPos + 1);
		}
	}

	for (const QString &bw : badWords) {
		if (!bw.isEmpty() && msg.contains(bw)) {
			return true;
		}
	}

	return false;
}
