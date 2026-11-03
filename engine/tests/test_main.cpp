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

static int testMain(int argc, char **argv)
{
#ifdef OPENBFME_TEST_COVERAGE
	coverageFlags(std::getenv("OPENBFME_COVERAGE_STARTUP"), true);
	std::atexit(writeCoverage);
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
