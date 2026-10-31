// SPDX-License-Identifier: GPL-2.0-or-later
// Vita adaptation; see license.txt and MODIFICATIONS.md.
#pragma once

#include "../ConditionalWait.h"
#include "VitaUidDiagnostics.h"
#include <psp2/kernel/threadmgr/mutex.h>
#include <psp2/kernel/threadmgr/cond.h>
#include <cstdio>
#include <cstdlib>

class VitaConditionalWait
{
	SceUID mutex;
	SceUID changed;
	SceUID waitersExited;
	bool busy = false;
	bool terminating = false;
	unsigned activeWaiters = 0;

	static void check(int result, const char * operation)
	{
		if (result < 0)
		{
			std::fprintf(stderr, "Vita native wait: %s failed kernel_error=0x%08x\n", operation, static_cast<unsigned>(result));
			VitaUidDiagnostics::report("native_wait_failure");
			std::abort();
		}
	}
	void lock() { check(sceKernelLockMutex(mutex, 1, nullptr), "lock"); }
	void unlock() { check(sceKernelUnlockMutex(mutex, 1), "unlock"); }

public:
	VitaConditionalWait()
	{
		mutex = sceKernelCreateMutex("vcmi-animation", 0, 0, nullptr);
		check(mutex, "create mutex");
		changed = sceKernelCreateCond("vcmi-animation-ready", 0, mutex, nullptr);
		check(changed, "create changed");
		waitersExited = sceKernelCreateCond("vcmi-animation-exit", 0, mutex, nullptr);
		check(waitersExited, "create exited");
	}
	VitaConditionalWait(const VitaConditionalWait &) = delete;
	VitaConditionalWait & operator=(const VitaConditionalWait &) = delete;
	~VitaConditionalWait()
	{
		requestTermination();
		check(sceKernelDeleteCond(waitersExited), "delete exited");
		check(sceKernelDeleteCond(changed), "delete changed");
		check(sceKernelDeleteMutex(mutex), "delete mutex");
	}
	void setBusy()
	{
		lock();
		busy = true;
		unlock();
	}
	void setFree()
	{
		lock();
		busy = false;
		check(sceKernelSignalCondAll(changed), "signal changed");
		unlock();
	}
	bool isBusy()
	{
		lock();
		const bool result = busy;
		unlock();
		return result;
	}
	void requestTermination()
	{
		lock();
		terminating = true;
		busy = false;
		check(sceKernelSignalCondAll(changed), "terminate changed");
		while (activeWaiters != 0)
			check(sceKernelWaitCond(waitersExited, nullptr), "wait for exit");
		unlock();
	}
	void waitWhileBusy()
	{
		lock();
		++activeWaiters;
		while (busy && !terminating)
			check(sceKernelWaitCond(changed, nullptr), "wait for changed");
		const bool terminate = terminating;
		if (--activeWaiters == 0)
			check(sceKernelSignalCondAll(waitersExited), "signal exit");
		unlock();
		if (terminate)
			throw TerminationRequestedException();
	}
};
