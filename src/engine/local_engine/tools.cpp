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

#define NOMINMAX // for Windows

#ifdef ANDROID
#error This file is not for android.
#endif

#include "tools.h"
#include <core/loghelper.h>
#include <core/openssl_wrapper.h>
#include <random>
#include <memory>
#include <algorithm>


using namespace std;

static const unsigned int DEFAULT_SEED = 5489u;

thread_local std::unique_ptr<std::random_device> g_rand_state;

static inline void InitRandState()
{
	if (!g_rand_state) {
		g_rand_state = std::make_unique<std::random_device>();
	}
}

void Tools::ShuffleArrayNonDeterministic(int *inout, unsigned count)
{
	InitRandState();
	std::mt19937 rand((*g_rand_state)());
	shuffle(&inout[0], &inout[count], rand);
}

void Tools::GetRand(int minValue, int maxValue, unsigned count, int *out)
{
	InitRandState();
	std::uniform_int_distribution<> dist(minValue, maxValue);
	int *startPtr = out;
	for (unsigned i = 0; i < count; i++) {
		*startPtr++ = dist(*g_rand_state);
	}
}

