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
#include <ctime>
#include <chrono>
#include <thread>
#include <cstdint>

using namespace std;

namespace {
	thread_local std::mt19937 t_rng = []() {
		std::random_device rd;
		std::random_device::result_type seed;
		if (rd.entropy() > 0) {
			seed = rd();
		} else {
			LOG_ERROR("Random device has insufficient entropy, using combined fallback");
			auto now = std::chrono::high_resolution_clock::now();
			auto ns = now.time_since_epoch().count();
			auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
			uint64_t combined = static_cast<uint64_t>(ns) ^ (static_cast<uint64_t>(tid) << 32);
			combined ^= static_cast<uint64_t>(std::time(nullptr)) * 0x9e3779b97f4a7c15ULL;
			seed = static_cast<std::random_device::result_type>(combined ^ (combined >> 32));
		}
		return std::mt19937(seed);
	}();
}

void Tools::ShuffleArrayNonDeterministic(int *inout, unsigned count)
{
	shuffle(&inout[0], &inout[count], t_rng);
}

void Tools::GetRand(int minValue, int maxValue, unsigned count, int *out)
{
	if (!out || count == 0) {
		return;
	}
	uniform_int_distribution<int> dist(minValue, maxValue);
	for (unsigned i = 0; i < count; i++) {
		*out++ = dist(t_rng);
	}
}

bool Tools::ConstantTimeStringCompare(const std::string& a, const std::string& b)
{
	volatile unsigned char result = 0;
	constexpr size_t maxCompareLen = 256;
	size_t maxLen = maxCompareLen;
	for (size_t i = 0; i < maxLen; ++i) {
		unsigned char ca = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
		unsigned char cb = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
		result |= ca ^ cb;
	}
	return result == 0 && a.size() == b.size();
}

