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

// A token is whitelisted only when an exception is anchored to a domain
// boundary inside it: at the start of the token, directly after a '.' (e.g.
// "www.pokerth.net"), or directly after an http(s) scheme. The text following
// the exception may only be empty, a path/query on the whitelisted host, or --
// for a prefix exception ending in '.' such as "http://www.esl." -- a single
// TLD-like label completing the host. An exception that already ends in a TLD
// (e.g. "pokerth.net") must not accept an extra label: "pokerth.net.xyz" is a
// different host, not the whitelisted site.
static bool isWhitelistedToken(const QString &token, const QStringList &exceptions)
{
	for (const QString &exception : exceptions) {
		if (exception.isEmpty()) {
			continue;
		}
		int exPos = token.indexOf(exception);
		while (exPos != -1) {
			const QString prefix = token.left(exPos);
			const bool domainBoundary = prefix.isEmpty()
					|| prefix.endsWith(QLatin1Char('.'))
					|| prefix == QLatin1String("http://")
					|| prefix == QLatin1String("https://");

			const QString suffix = token.mid(exPos + exception.length());
			bool validSuffix = true;
			if (!suffix.isEmpty()) {
				const QChar first = suffix.at(0);
				if (first == QLatin1Char('/') || first == QLatin1Char('?') || first == QLatin1Char('#')) {
					validSuffix = true;
				} else if (exception.endsWith(QLatin1Char('.'))) {
					// Only an incomplete prefix exception such as
					// "http://www.esl." needs a TLD label to complete it.
					// Consume just that one label; a path/query/fragment may
					// follow it on the whitelisted host, while another dot
					// would introduce a different host and stay rejected.
					const int labelStart = (first == QLatin1Char('.')) ? 1 : 0;
					int i = labelStart;
					while (i < suffix.length() && suffix.at(i).isLetterOrNumber()) {
						++i;
					}
					if (i == labelStart) {
						validSuffix = false;
					} else if (i < suffix.length()
							&& suffix.at(i) != QLatin1Char('/')
							&& suffix.at(i) != QLatin1Char('?')
							&& suffix.at(i) != QLatin1Char('#')) {
						validSuffix = false;
					}
				} else {
					validSuffix = false;
				}
			}

			if (domainBoundary && validSuffix) {
				return true;
			}
			exPos = token.indexOf(exception, exPos + 1);
		}
	}
	return false;
}

bool UrlCheck::run(QString msg)
{
	QMutexLocker locker(&m_mutex);
	msg = msg.toLower();

	// Inspect each whitespace-delimited token separately. Testing an exception
	// against the entire message would let one whitelisted token disable the
	// filter for every other URL in the same message. A token is skipped only
	// when it actually is a whitelisted URL, not when it merely contains an
	// exception string as a substring.
	const QStringList tokens = msg.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
	for (const QString &token : tokens) {
		if (isWhitelistedToken(token, urlExceptionStrings)) {
			continue;
		}
		for (const QString &url : urlStrings) {
			if (!url.isEmpty() && token.contains(url)) {
				return true;
			}
		}
	}
	return false;
}
