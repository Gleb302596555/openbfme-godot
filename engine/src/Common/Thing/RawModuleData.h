// OpenBFME. GPL-3.0.
//
// RawModuleData: the ModuleData of a module class whose typed data class is not ported yet. It
// keeps the module body's lines verbatim (file, source line, text) so the typed parser can be
// plugged in later without re-reading INI files. Acceptance stop S-070: while a class is raw its
// field VALUES are not parsed or validated; only the body's extent and the field NAMES are (the
// rows come from the binary's own table for that class, see RwGrammar.h).

#pragma once

#include "Common/INI.h"
#include "Common/Module.h"

#include <string>
#include <vector>

class RawModuleData : public ModuleData
{
public:
	struct Line
	{
		std::string file;
		int sourceLine = 0;
		std::string text;
		// TOP20 #21: the text with every value word that names a #define macro replaced by the macro's value, the way the INI scanners read a
		// number (INI::preprocessMacro, RW 0x42D0A9 / B1 INIPreprocessMacro.cpp:83-110: a word starting with a digit is kept, a known macro name
		// becomes its value); the comment (';' / '//') is dropped. The readers of numeric raw fields use it: BurningDeathTime =
		// BURNINGDEATH_DURATION_INFANTRY (RotWK's and Ennorath's _gamedata.inc) read as 0 from `text`. Math (#MULTIPLY ...) stays as written
		std::string resolved;
	};

	RawModuleData(std::string className, ModuleType type)
		: m_className(std::move(className))
		, m_type(type)
	{
	}

	const std::string &className() const { return m_className; }
	ModuleType moduleType() const { return m_type; }
	// every line of the body after the header line, including the closing End
	const std::vector<Line> &lines() const { return m_lines; }

	// Copies lines [first, last) of the INI's line array (see INI::lineTextAt).
	void capture(INI &ini, size_t first, size_t last)
	{
		for (size_t i = first; i < last; ++i)
		{
			Line l;
			l.file = ini.sourceFileAt(i);
			l.sourceLine = ini.sourceLineAt(i);
			l.text = ini.lineTextAt(i);
			l.resolved = resolveMacros(ini, l.text);
			m_lines.push_back(std::move(l));
		}
	}
	// see Line::resolved
	static std::string resolveMacros(INI &ini, const std::string &text);

private:
	std::string m_className;
	ModuleType m_type;
	std::vector<Line> m_lines;
};
