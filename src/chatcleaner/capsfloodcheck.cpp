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
#include "capsfloodcheck.h"

#include <QtCore>

#include <algorithm>

namespace {

bool isUpper(QChar c)
{
	return c >= QChar::fromLatin1('A') && c <= QChar::fromLatin1('Z');
}

bool isPunctuation(QChar c)
{
	return c == QLatin1Char('.') || c == QLatin1Char(',') || c == QLatin1Char(';')
			|| c == QLatin1Char(':') || c == QLatin1Char('!') || c == QLatin1Char('?')
			|| c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}')
			|| c == QLatin1Char('\'') || c == QLatin1Char('"');
}

} // namespace

CapsFloodCheck::CapsFloodCheck()
	: capsNumberToTrigger(0)
{
}

bool CapsFloodCheck::run(QString msg)
{
	QMutexLocker locker(&m_mutex);
	if (capsNumberToTrigger < 1) {
		return false;
	}

	// Count consecutive capitals within each whitespace-delimited token.
	// Stripping all spaces first (the old behaviour) merged separate short
	// words such as "NASA ESA CNN" into one long run and produced false
	// positives. Consecutive single-letter uppercase tokens are counted as a
	// single run as well, so the spaced-out caps evasion ("H E L L O") is
	// still detected.
	const QStringList tokens = msg.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
	int maxRun = 0;
	int singleLetterRun = 0;
	for (const QString &token : tokens) {
		int run = 0;
		int upperCount = 0;
		bool onlyUpperOrPunctuation = true;
		for (int i = 0; i < static_cast<int>(token.length()); ++i) {
			const QChar c = token.at(i);
			if (isUpper(c)) {
				++run;
				++upperCount;
				maxRun = std::max(maxRun, run);
			} else if (isPunctuation(c)) {
				run = 0;
			} else {
				run = 0;
				onlyUpperOrPunctuation = false;
			}
		}
		if (upperCount == 1 && onlyUpperOrPunctuation) {
			++singleLetterRun;
			maxRun = std::max(maxRun, singleLetterRun);
		} else {
			singleLetterRun = 0;
		}
	}

	return maxRun >= capsNumberToTrigger;
}
