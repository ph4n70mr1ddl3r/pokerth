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

#include "thread.h"
#include <engine/log.h>
#include <core/pokerthexception.h>
#include <boost/chrono/chrono.hpp>
#include <chrono>
#include <thread>
#include <exception>



// Helper class for thread creation.
class ThreadStarter
{
public:
	ThreadStarter(Thread &thread) : m_thread(thread) {}
	void operator()()
	{
		m_thread.MainWrapper();
	}

private:
	Thread &m_thread;
};

Thread::Thread()
	: m_isTerminatedSemaphore(0), m_shouldTerminateSemaphore(0)
{
}

Thread::~Thread() noexcept
{
	if (IsRunning()) {
		SignalTermination();
		m_isTerminatedSemaphore.wait();
		boost::mutex::scoped_lock lock(m_threadObjMutex);
		if (m_threadObj) {
			m_threadObj->join();
			m_threadObj.reset();
		}
	}
}

void
Thread::Run()
{
	boost::mutex::scoped_lock threadLock(m_threadObjMutex);

	// Create the boost thread object.
	if (!m_threadObj) {
		m_threadObj.reset(new boost::thread(ThreadStarter(*this)));
	}
}

void
Thread::SignalTermination()
{
	m_shouldTerminateFlag.store(true, std::memory_order_release);
	m_shouldTerminateSemaphore.post();
}

bool
Thread::Join(unsigned msecTimeout)
{
	if (!IsRunning())
		return true;

	bool tmpIsTerminated;
	if (msecTimeout == THREAD_WAIT_INFINITE) {
		// Wait infinitely.
		m_isTerminatedSemaphore.wait();
		tmpIsTerminated = true;
	} else {
		// Wait for the termination of the application code.
		tmpIsTerminated = m_isTerminatedSemaphore.try_wait_for(boost::chrono::milliseconds(msecTimeout));
	}

	if (tmpIsTerminated) {
		boost::mutex::scoped_lock lock(m_threadObjMutex);
		// Wait for "real" termination of the thread.
		if (m_threadObj) {
			m_threadObj->join();
			m_threadObj.reset();
		}
	}

	return tmpIsTerminated;
}

void
Thread::Msleep(unsigned msecs)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(msecs));
}

void
Thread::MainWrapper()
{
	try {
		this->Main();
	} catch (const PokerTHException &e) {
		LOG_ERROR("PokerTH exception in thread: " << e.what()
			<< " error=" << e.GetErrorId() << " osError=" << e.GetOsErrorCode());
	} catch (const std::exception& e) {
		LOG_ERROR("Exception in thread: " << e.what());
	} catch (...) {
		LOG_ERROR("Unknown exception in thread");
	}
	m_isTerminatedSemaphore.post();
}

bool
Thread::ShouldTerminate() const
{
	return m_shouldTerminateFlag.load(std::memory_order_acquire);
}

bool
Thread::IsRunning() const
{
	boost::mutex::scoped_lock threadLock(m_threadObjMutex);
	return m_threadObj != nullptr;
}

