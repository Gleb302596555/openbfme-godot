// OpenBFME test support: which engine functions a test process runs (owner 2026-11-24: the maps run reruns a matrix faction only when a change
// touches code that faction's processes ran). GPL-3.0.
//
// Built only with OPENBFME_FUNC_TRACE=ON (GCC / Clang, -finstrument-functions; the maps run's own build, never the game or the PC build). Every
// instrumented function calls __cyg_profile_func_enter; the first call of each function appends its offset from the image start (8 bytes, little
// endian) to $OPENBFME_FUNC_TRACE_OUT.<pid> at once, with write(2): a process that crashes or is killed has still recorded everything it ran up to
// then. tools/mod/matrix_deps.py turns the offsets into translation units (the binary's DWARF), the data those units read, and their headers.

#if defined(OPENBFME_FUNC_TRACE)

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

extern "C" char __executable_start; // the linker's symbol for the start of the image (PIE: the load address at run time)

namespace
{
constexpr std::size_t kSlots = std::size_t(1) << 20; // ~70,000 functions in the image: a table this size stays sparse
std::atomic<std::uintptr_t> g_seen[kSlots];          // static storage: zero before any code runs
std::atomic<int> g_fd{ -2 };                         // -2 not opened yet, -1 no output asked for (or it failed)

__attribute__((no_instrument_function)) int traceFd()
{
	int fd = g_fd.load(std::memory_order_acquire);
	if (fd != -2)
	{
		return fd;
	}
	// the first call comes from a static initializer, before any other thread exists
	const char *base = std::getenv("OPENBFME_FUNC_TRACE_OUT");
	int opened = -1;
	if (base && *base)
	{
		char path[4096];
		std::snprintf(path, sizeof(path), "%s.%ld", base, (long)getpid());
		opened = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
	}
	int expected = -2;
	if (!g_fd.compare_exchange_strong(expected, opened, std::memory_order_acq_rel))
	{
		if (opened >= 0)
		{
			close(opened);
		}
		return expected;
	}
	return opened;
}
} // namespace

extern "C" __attribute__((no_instrument_function)) void __cyg_profile_func_enter(void *fn, void *)
{
	const std::uintptr_t key = (std::uintptr_t)fn - (std::uintptr_t)&__executable_start + 1; // + 1: 0 marks an empty slot
	std::size_t h = (std::size_t)((key * 0x9E3779B97F4A7C15ull) >> 44) & (kSlots - 1);
	for (;;)
	{
		std::uintptr_t cur = g_seen[h].load(std::memory_order_relaxed);
		if (cur == key)
		{
			return; // seen: the common case, one load
		}
		if (cur == 0)
		{
			if (g_seen[h].compare_exchange_strong(cur, key, std::memory_order_relaxed))
			{
				const int fd = traceFd();
				if (fd >= 0)
				{
					const std::uint64_t off = key - 1;
					if (write(fd, &off, sizeof(off)) != (ssize_t)sizeof(off))
					{
						// a lost function makes the process's set incomplete: leave <out>.<pid>.broken, and matrix_deps.py trusts no set of this run
						char path[4096];
						const char *base = std::getenv("OPENBFME_FUNC_TRACE_OUT");
						std::snprintf(path, sizeof(path), "%s.%ld.broken", base ? base : "functrace", (long)getpid());
						const int marker = open(path, O_WRONLY | O_CREAT, 0644);
						if (marker >= 0)
						{
							close(marker);
						}
						g_fd.store(-1, std::memory_order_release);
					}
				}
				return;
			}
			if (cur == key)
			{
				return;
			}
		}
		h = (h + 1) & (kSlots - 1);
	}
}

extern "C" __attribute__((no_instrument_function)) void __cyg_profile_func_exit(void *, void *) {}

#endif
