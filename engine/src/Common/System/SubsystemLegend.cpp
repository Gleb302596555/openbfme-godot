// OpenBFME. GPL-3.0.
//
// SubsystemLegend, loadIniFilesFromLegend and the RotWK subsystem INI load order.
// Citations are in SubsystemLegend.h and on each function.

#include "Common/SubsystemLegend.h"

#include "Common/AsciiString.h"

#include <algorithm>

const char *const kSubsystemLegendFile = "Data\\INI\\Default\\SubsystemLegendExpansion1.ini";

namespace
{
// B1 SubsystemLegend.cpp: TheSubsystemLegendLoaderNames, retail 0x012D7758. Only "INI".
const LookupListRec kLoaderNames[] = { { "INI", 0 }, { nullptr, 0 } };

SubsystemLegendEntry *asEntry(void *instance)
{
	return static_cast<SubsystemLegendEntry *>(instance);
}

void parseInitFile(INI *ini, void *instance, void *, const void *)
{
	INI::parseAsciiStringVectorAppend(ini, instance, &asEntry(instance)->initFile, nullptr);
}
void parseInitPath(INI *ini, void *instance, void *, const void *)
{
	INI::parseAsciiStringVectorAppend(ini, instance, &asEntry(instance)->initPath, nullptr);
}
void parseExtension(INI *ini, void *instance, void *, const void *)
{
	INI::parseAsciiStringVectorAppend(ini, instance, &asEntry(instance)->extension, nullptr);
}
void parseExcludePath(INI *ini, void *instance, void *, const void *)
{
	INI::parseAsciiStringVectorAppend(ini, instance, &asEntry(instance)->excludePath, nullptr);
}
void parseIncludePathCinematics(INI *ini, void *instance, void *, const void *)
{
	INI::parseAsciiStringVectorAppend(ini, instance, &asEntry(instance)->includePathCinematics, nullptr);
}
void parseLoader(INI *ini, void *instance, void *, const void *)
{
	INI::parseLookupList(ini, instance, &asEntry(instance)->loader, kLoaderNames);
}
void parseInitFileDebug(INI *ini, void *instance, void *, const void *)
{
	INI::parseAsciiString(ini, instance, &asEntry(instance)->initFileDebug, nullptr);
}

// RW LoadSubsystem field table at 0xBED7D0 (spec 3.4 item 1); B1 has the subset
// InitFile/InitPath/Extension/Loader/InitFileDebug (SubsystemLegend.cpp:44-57).
const FieldParse kLegendFieldParse[] = {
	{ "InitFile", parseInitFile, nullptr, 0 },
	{ "InitPath", parseInitPath, nullptr, 0 },
	{ "Extension", parseExtension, nullptr, 0 },
	{ "ExcludePath", parseExcludePath, nullptr, 0 },
	{ "IncludePathCinematics", parseIncludePathCinematics, nullptr, 0 },
	{ "Loader", parseLoader, nullptr, 0 },
	{ "InitFileDebug", parseInitFileDebug, nullptr, 0 },
	{ nullptr, nullptr, nullptr, 0 },
};

std::string normalizedSlashes(std::string s)
{
	std::replace(s.begin(), s.end(), '/', '\\');
	return s;
}

bool startsWithNoCase(const std::string &s, const std::string &prefix)
{
	return s.size() >= prefix.size() && AsciiStringUtil::compareNoCase(s.substr(0, prefix.size()), prefix) == 0;
}

bool isUnderCinematicsPath(const SubsystemLegendEntry &entry, const std::string &file)
{
	const std::string f = normalizedSlashes(file);
	for (const std::string &p : entry.includePathCinematics)
	{
		if (startsWithNoCase(f, normalizedSlashes(p)))
		{
			return true;
		}
	}
	return false;
}

// RW 0x0042C2B5: the vector walk, each item compared with RW 0x0042C0E7 (AsciiString::startsWithNoCase), true on the first prefix hit.
bool isExcluded(const std::vector<std::string> &excluded, const std::string &name)
{
	const std::string n = normalizedSlashes(name);
	for (const std::string &p : excluded)
	{
		if (startsWithNoCase(n, p))
		{
			return true;
		}
	}
	return false;
}

void loadFileChecked(INI &ini, const std::string &file, const SubsystemLoadOptions &options, SubsystemLoadReport *report)
{
	if (!options.collectErrors)
	{
		ini.load(file, INI_LOAD_OVERWRITE);
		return;
	}
	try
	{
		ini.load(file, INI_LOAD_OVERWRITE);
	}
	catch (const INIException &e)
	{
		if (report)
		{
			SubsystemLoadReport::FileError err;
			err.file = file;
			err.code = e.code();
			err.message = e.message();
			report->errors.push_back(err);
		}
	}
}
}

// B1 SubsystemLegend.cpp parseLoadSubsystem / parseSubsystemLegendDefinition.
void SubsystemLegend::registerBlock(INIBlockRegistry &registry)
{
	registry.registerBlock("LoadSubsystem", [this](INI *ini) { parseLoadSubsystem(ini); });
}

void SubsystemLegend::parseLoadSubsystem(INI *ini)
{
	SubsystemLegendEntry entry;
	entry.name = ini->getNextToken(nullptr);
	ini->initFromINI(&entry, kLegendFieldParse);
	m_entries.push_back(entry);
}

// RW 0x005B41D0: the entry list (the legend manager [0xDE337C], its head at [edi+0xC]), the name compare RW 0x004065AA (AsciiString's ==, so
// case-sensitive), the first match, else null. B1 SubsystemLegend.cpp findEntry.
const SubsystemLegendEntry *SubsystemLegend::findEntry(const std::string &name) const
{
	for (const SubsystemLegendEntry &e : m_entries)
	{
		if (e.name == name)
		{
			return &e;
		}
	}
	return nullptr;
}

// RW 0x005B4B9A SubsystemInterface::loadIniFilesFromLegend, slot 2 of every subsystem vtable (0xBD8430, 0xBD84E8, 0xBD8598, ...): the base class's
// legend loader, NOT ThreatFinderUpdate behaviour (S-7886: its "the load call returned false" branch is not reachable in the port). B1
// Libraries/Source/subsystem/SubsystemInterface.cpp:40-74. Its branches:
//   0x005B4BAA  the legend manager [0xDE337C] is null -> return false (no legend was read);
//   0x005B4BBF  the key is the subsystem's OWN name (this + 8), looked up through RW 0x005B41D0; no entry -> return false (0x005B4BF3);
//   0x005B4C06  ONE exclusion vector for both loops: every ExcludePath ([esi+0x34] .. [esi+0x38]) and, only when the cinematics flag
//               [0xDE3384] is off, every IncludePathCinematics ([esi+0x28] .. [esi+0x2C]);
//   0x005B4C4F  the load type is byte[this+4] ? 5 (INI_LOAD_RELOAD) : 1 (INI_LOAD_OVERWRITE);
//   0x005B4C69  every InitFile ([esi+4] .. [esi+8]): RW 0x0042C2B5 tests the name against that vector (the prefix compare RW 0x0042C0E7) and
//               a hit jumps to 0x005B4CA1, so the file is never loaded; otherwise RW 0x0042D97D loads it;
//   0x005B4CAE  every InitPath ([esi+0x10] .. [esi+0x14]): RW 0x0042E343, and the exclusion vector is passed to it, so the directory walk
//               prunes with the same names (no 0x0042C2B5 test of the directory itself);
//   0x005B4C99 / 0x005B4CD4  the return byte [ebp-0xD] becomes 1 ONLY when one of those two calls returned true.
// S-7886 (the one branch not ported): the InitFile call returns false when retail's prepFile cannot open the file, and the walk continues with the
// next name; the port's INI::load throws INIException(7) there instead, a contract every lane shares and test_ini_core.cpp pins.
bool SubsystemLegend::loadIniFilesFromLegend(const std::string &subsystemName, INI &ini, const SubsystemLoadOptions &options, SubsystemLoadReport *report) const
{
	const SubsystemLegendEntry *entry = findEntry(subsystemName);
	if (!entry)
	{
		return false; // RW 0x005B4BF3
	}

	// RW 0x005B4C06 .. 0x005B4C42: one vector, used by the InitFile test and by the InitPath walk alike.
	std::vector<std::string> excluded = entry->excludePath;
	if (!options.cinematics) // RW: the flag is [0xDE3384], the -cinematics command argument
	{
		for (const std::string &p : entry->includePathCinematics)
		{
			excluded.push_back(p);
		}
	}
	for (std::string &p : excluded)
	{
		p = normalizedSlashes(p);
	}

	bool loadedAny = false; // RW [ebp-0xD], cleared at 0x005B4BC6
	for (const std::string &f : entry->initFile)
	{
		// RW 0x005B4C69: the InitFile name is tested against the WHOLE exclusion vector, so an ExcludePath names a single InitFile as well as a
		// directory item (RotWK's own TheThingFactory excludes two files by name). Only the IncludePathCinematics part is reported.
		if (isExcluded(excluded, f))
		{
			if (report && !options.cinematics && isUnderCinematicsPath(*entry, f))
			{
				report->skippedCinematics.push_back(entry->name + ": " + f);
			}
			continue;
		}
		loadFileChecked(ini, f, options, report);
		loadedAny = true; // RW 0x005B4C99: a file the exclusion vector names never sets the byte
	}

	for (const std::string &d : entry->initPath)
	{
		INILoadDirectoryOptions dirOptions;
		dirOptions.excludePaths = excluded; // RW 0x005B4CAE: the same vector, as loadDirectory's prune list
		if (report && !options.cinematics)
		{
			for (const std::string &p : entry->includePathCinematics)
			{
				report->skippedCinematics.push_back(entry->name + ": directory " + p);
			}
		}
		if (options.collectErrors)
		{
			dirOptions.onFileError = [report](const std::string &file, const INIException &e) {
				if (report)
				{
					SubsystemLoadReport::FileError err;
					err.file = file;
					err.code = e.code();
					err.message = e.message();
					report->errors.push_back(err);
				}
			};
		}
		ini.loadDirectory(d, true, INI_LOAD_OVERWRITE, dirOptions);
		loadedAny = true; // RW 0x005B4CD4
	}
	return loadedAny;
}

void LoadSubsystemFile(INI &ini, const std::string &file, const SubsystemLoadOptions &options, SubsystemLoadReport *report)
{
	loadFileChecked(ini, file, options, report);
}

