// OpenBFME unit tests (doctest). GPL-3.0.
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "Common/PathIO.h"

#include <fstream>

#ifdef OPENBFME_TEST_COVERAGE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" IMAGE_DOS_HEADER __ImageBase;

// tools/test_coverage_map.py: the offsets of the flags set so far, into a file; every process runs the static initializers, so they are kept apart from what its tests run
static void coverageFlags(const char *path, bool clear)
{
	const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(reinterpret_cast<const char *>(&__ImageBase) + __ImageBase.e_lfanew);
	const IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
	for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
	{
		if (std::memcmp(section->Name, ".sancov\0", 8) != 0)
		{
			continue;
		}
		auto *flags = reinterpret_cast<std::uint8_t *>(&__ImageBase) + section->VirtualAddress;
		if (path)
		{
			std::FILE *out = std::fopen(path, "wb");
			if (!out)
			{
				std::fprintf(stderr, "cannot open coverage output: %s\n", path);
				std::_Exit(1);
			}
			for (std::uint32_t at = 0; at < section->Misc.VirtualSize; ++at)
			{
				if (flags[at])
				{
					std::fwrite(&at, sizeof(at), 1, out);
				}
			}
			std::fclose(out);
		}
		if (clear)
		{
			std::memset(flags, 0, section->Misc.VirtualSize);
		}
	}
}

// registered in main, so it runs before the static destructors
static void writeCoverage()
{
	coverageFlags(std::getenv("OPENBFME_COVERAGE_OUT"), false);
}
#endif

#if defined(_MSC_VER)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

#include <cstdio>
#include <cstdlib>

// OPENBFME_CRASH_STACK=1: an access violation, stack overflow or illegal instruction prints its stack (function, file, line from the PDB) to stderr
// before doctest reports the test case as CRASHED, which names no place. A vectored handler sees the exception first; the process goes on to doctest's.
static LONG WINAPI crashStack(EXCEPTION_POINTERS *info)
{
	const DWORD code = info->ExceptionRecord->ExceptionCode;
	if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_ILLEGAL_INSTRUCTION)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}
	static bool printed = false;
	if (printed)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}
	printed = true;
	HANDLE process = GetCurrentProcess(), thread = GetCurrentThread();
	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
	SymInitialize(process, nullptr, TRUE);
	CONTEXT context = *info->ContextRecord;
	if (context.Rip == 0)
	{
		// a call through a null pointer: the caller's return address is on top of the stack
		context.Rip = *reinterpret_cast<const DWORD64 *>(context.Rsp);
		context.Rsp += 8;
	}
	STACKFRAME64 frame = {};
	frame.AddrPC.Offset = context.Rip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = context.Rbp;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Offset = context.Rsp;
	frame.AddrStack.Mode = AddrModeFlat;
	std::fprintf(stderr, "CRASH STACK: exception 0x%08lX at 0x%llX\n", code, (unsigned long long)context.Rip);
	for (int i = 0; i < 48 && StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr); ++i)
	{
		alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 512] = {};
		auto *symbol = reinterpret_cast<SYMBOL_INFO *>(buffer);
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = 511;
		DWORD64 displacement = 0;
		const bool named = SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol) != FALSE;
		IMAGEHLP_LINE64 line = {};
		line.SizeOfStruct = sizeof(line);
		DWORD column = 0;
		const bool lined = SymGetLineFromAddr64(process, frame.AddrPC.Offset, &column, &line) != FALSE;
		const DWORD64 module = SymGetModuleBase64(process, frame.AddrPC.Offset);
		std::fprintf(stderr, "  #%02d %s  %s:%lu  rva 0x%llX%s\n", i, named ? symbol->Name : "?", lined ? line.FileName : "?", lined ? line.LineNumber : 0ul,
			(unsigned long long)(frame.AddrPC.Offset - (module ? module : (DWORD64)GetModuleHandleW(nullptr))), module == (DWORD64)GetModuleHandleW(nullptr) || !module ? " (exe)" : "");
	}
	std::fflush(stderr);
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

static int testMain(int argc, char **argv)
{
#ifdef OPENBFME_TEST_COVERAGE
	coverageFlags(std::getenv("OPENBFME_COVERAGE_STARTUP"), true);
	std::atexit(writeCoverage);
#endif
#if defined(_MSC_VER)
	if (const char *stack = std::getenv("OPENBFME_CRASH_STACK"); stack && *stack && *stack != '0')
	{
		AddVectoredExceptionHandler(1, crashStack);
	}
#endif
	doctest::Context context(argc, argv);
	context.setAsDefaultForAssertsOutOfTestCases();
	const auto &options = *doctest::getContextOptions();
	std::ofstream output;
	// Doctest's default --out handling opens a narrow path, which uses the Windows ACP.
	if (!options.quiet && options.out.size())
	{
		output.open(PathIO::native(options.out.c_str()));
		if (!output)
		{
			std::fprintf(stderr, "cannot open test output: %s\n", options.out.c_str());
			return 1;
		}
		context.setCout(&output);
	}
	return context.run();
}

int main(int argc, char **argv)
{
	return PathIO::runMain(argc, argv, testMain);
}
