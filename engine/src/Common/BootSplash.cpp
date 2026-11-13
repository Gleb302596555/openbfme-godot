// OpenBFME. GPL-3.0.
// See Common/BootSplash.h.

#include "Common/BootSplash.h"

namespace BootSplash
{

bool resolve(const std::string &language,
	const std::function<bool(const std::string &)> &exists,
	std::string *chosen,
	std::string *error)
{
	const std::string lang = language.empty() ? defaultLanguage() : language;
	const std::string jpg = jpgName(lang);
	const std::string bmp = bmpName(lang);
	if (exists && exists(jpg))
	{
		if (chosen)
			*chosen = jpg;
		return true;
	}
	if (exists && exists(bmp))
	{
		if (chosen)
			*chosen = bmp;
		return true;
	}
	if (error)
	{
		*error = "boot splash not found: '" + jpg + "' nor '" + bmp + "' is in the archives (RW WinMain gLoadScreenBitmap, donor 0x00002C39)";
	}
	return false;
}

} // namespace BootSplash
