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
#include "urlcheck.h"

#include <QtCore>

UrlCheck::UrlCheck()
{
}

bool UrlCheck::run(QString msg)
{
	QMutexLocker locker(&m_mutex);
	msg = msg.toLower();

	// Remove whitelisted URLs from the message before looking for URL patterns.
	// The whole whitespace-delimited token is removed so that a prefix
	// exception such as "http://www.esl." also covers the trailing TLD.
	// Testing an exception against the entire original message (instead of the
	// specific URL it is meant to allow) would let a single whitelisted token
	// disable the filter for every other URL in the same message.
	for (const QString &exception : urlExceptionStrings) {
		if (exception.isEmpty()) {
			continue;
		}
		int exPos = msg.indexOf(exception);
		while (exPos != -1) {
			int start = exPos;
			while (start > 0 && !msg.at(start - 1).isSpace()) {
				--start;
			}
			int end = exPos + exception.length();
			while (end < msg.length() && !msg.at(end).isSpace()) {
				++end;
			}
			msg.remove(start, end - start);
			exPos = msg.indexOf(exception, start);
		}
	}

	for (const QString &url : urlStrings) {
		if (!url.isEmpty() && msg.contains(url)) {
			return true;
		}
	}
	return false;
}
