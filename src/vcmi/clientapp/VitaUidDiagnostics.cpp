// SPDX-License-Identifier: GPL-2.0-or-later
// Vita adaptation; see license.txt and MODIFICATIONS.md.
#include <pthread.h>
#include <semaphore.h>
#include <psp2/kernel/threadmgr/semaphore.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <iterator>
#include "../lib/vita/VitaUidDiagnostics.h"

namespace
{
struct Site { uintptr_t address = 0; unsigned kind = 0; unsigned count = 0; };
Site sites[256];
std::atomic_flag sitesLock = ATOMIC_FLAG_INIT;
std::atomic<unsigned> created{0}, deleted{0}, failures{0};
void record(unsigned kind, void * address)
{
	const auto key = reinterpret_cast<uintptr_t>(address);
	while (sitesLock.test_and_set(std::memory_order_acquire)) {}
	for (auto & site : sites)
	{
		if (site.address == 0 || (site.address == key && site.kind == kind))
		{
			site.address = key;
			site.kind = kind;
			++site.count;
			break;
		}
	}
	sitesLock.clear(std::memory_order_release);
}
}

namespace VitaUidDiagnostics
{
void report(const char * stage)
{
	Site snapshot[256];
	while (sitesLock.test_and_set(std::memory_order_acquire)) {}
	std::copy(std::begin(sites), std::end(sites), std::begin(snapshot));
	sitesLock.clear(std::memory_order_release);
	std::sort(std::begin(snapshot), std::end(snapshot), [](const Site & a, const Site & b) { return a.count > b.count; });
	std::fprintf(stderr, "Vita UID: stage=%s sem_created=%u sem_deleted=%u sem_failed=%u anchor=%p\n", stage, created.load(), deleted.load(), failures.load(), reinterpret_cast<void *>(&report));
	if (std::strcmp(stage, "native_wait_failure") != 0)
		return;
	for (unsigned i = 0; i < 20 && snapshot[i].count != 0; ++i)
		std::fprintf(stderr, "Vita UID site: kind=%u caller=0x%08x count=%u\n", snapshot[i].kind, static_cast<unsigned>(snapshot[i].address), snapshot[i].count);
}
}

extern "C" {
SceUID __real_sceKernelCreateSema(const char *, SceUInt, int, int, SceKernelSemaOptParam *);
int __real_sceKernelDeleteSema(SceUID);
int __real_pthread_mutex_lock(pthread_mutex_t *);
int __real_pthread_mutex_trylock(pthread_mutex_t *);
int __real_pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
int __real_pthread_rwlock_rdlock(pthread_rwlock_t *);
int __real_pthread_rwlock_wrlock(pthread_rwlock_t *);
int __real_sem_init(sem_t *, int, unsigned);

SceUID __wrap_sceKernelCreateSema(const char * name, SceUInt attr, int initial, int maximum, SceKernelSemaOptParam * option)
{
	const auto result = __real_sceKernelCreateSema(name, attr, initial, maximum, option);
	if (result >= 0) created.fetch_add(1, std::memory_order_relaxed);
	else failures.fetch_add(1, std::memory_order_relaxed);
	return result;
}
int __wrap_sceKernelDeleteSema(SceUID id)
{
	const int result = __real_sceKernelDeleteSema(id);
	if (result >= 0) deleted.fetch_add(1, std::memory_order_relaxed);
	return result;
}
int __wrap_pthread_mutex_lock(pthread_mutex_t * mutex)
{
	if (mutex && reinterpret_cast<intptr_t>(*mutex) >= -3 && reinterpret_cast<intptr_t>(*mutex) < 0)
		record(1, __builtin_return_address(0));
	return __real_pthread_mutex_lock(mutex);
}
int __wrap_pthread_mutex_trylock(pthread_mutex_t * mutex)
{
	if (mutex && reinterpret_cast<intptr_t>(*mutex) >= -3 && reinterpret_cast<intptr_t>(*mutex) < 0)
		record(6, __builtin_return_address(0));
	return __real_pthread_mutex_trylock(mutex);
}
int __wrap_pthread_mutex_init(pthread_mutex_t * mutex, const pthread_mutexattr_t * attr)
{
	record(2, __builtin_return_address(0));
	return __real_pthread_mutex_init(mutex, attr);
}
int __wrap_pthread_rwlock_rdlock(pthread_rwlock_t * mutex)
{
	if (mutex && *mutex == PTHREAD_RWLOCK_INITIALIZER) record(3, __builtin_return_address(0));
	return __real_pthread_rwlock_rdlock(mutex);
}
int __wrap_pthread_rwlock_wrlock(pthread_rwlock_t * mutex)
{
	if (mutex && *mutex == PTHREAD_RWLOCK_INITIALIZER) record(4, __builtin_return_address(0));
	return __real_pthread_rwlock_wrlock(mutex);
}
int __wrap_sem_init(sem_t * sem, int shared, unsigned value)
{
	record(5, __builtin_return_address(0));
	return __real_sem_init(sem, shared, value);
}
}
