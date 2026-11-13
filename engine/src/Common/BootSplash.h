// OpenBFME. GPL-3.0.
// Port 157 (issue #375, P-78): the picture shown while the game loads before the main menu.
//
// TARGET FACTS (BFME2 1.06 donor, confirmed in RotWK 2.01 shape):
//   WinMain at 0x00002C39 (BFME2, donor Code/GameEngine/Source/Main/WinMain.cpp:823..1044)
//   loads the splash bitmap before creating the main window:
//     sprintf(filePath, "%sSplash.jpg", GetRegistryLanguage().str()); // BFME2 0x00002ED? (donor WinMain.cpp:1038)
//     if (splashImage.Load(filePath) < 0)                              // ATL::CImage::Load, donor 0x00002E?? fallback
//         sprintf(filePath, "%sSplash.bmp", GetRegistryLanguage().str()); // BFME2 WinMain.cpp:1041
//     gLoadScreenBitmap = splashImage;                                  // global HBITMAP gLoadScreenBitmap, donor WinMain.cpp:1044 / 0x00DA... / BFME2 0x310
//   The bitmap is painted in the window's WM_PAINT handler (donor WinMain.cpp:556..560):
//     if (gLoadScreenBitmap != NULL) { SaveDC; CreateCompatibleDC; SelectObject(gLoadScreenBitmap); BitBlt(dc,0,0,800,600,tmpDC,0,0,SRCCOPY); ... }
//   The window is centred only while the X/Y globals keep the -1000000 sentinel and is shown via initializeAppWindows
//   (donor initializeAppWindows 0x0000275D, WinMain.cpp:310 HBITMAP gLoadScreenBitmap).
//   INFERENCE (RW 0x... not read – no RotWK game.dat in the sandbox): RotWK 2.01's WinMain keeps the same shape
//   (same GetRegistryLanguage() + "Splash.jpg/.bmp" fallback and the same WM_PAINT BitBlt of gLoadScreenBitmap), only the
//   VA base differs. The port therefore treats the boot splash as "<Language>Splash.jpg" falling back to ".bmp"
//   and requires that the archive contains one of them before the shell starts; a missing image is a boot error,
//   not a silent black screen.
//
// The boot sequence (GodotDevice/GodotGameStart, AptMenuPlayer::boot_shell, Shell) must request that image
// before the shell's MainMenu appears, so the player sees the same picture retail does while INI/archives load.

#pragma once

#include <functional>
#include <string>

namespace BootSplash
{

// The file name RW uses for the language's boot splash (retail's WinMain sprintf).
// "<Language>Splash.jpg" (e.g. "EnglishSplash.jpg"). The ".bmp" fallback is handled by resolve().
inline std::string jpgName(const std::string &language)
{
	return language + "Splash.jpg";
}

inline std::string bmpName(const std::string &language)
{
	return language + "Splash.bmp";
}

// Pick the boot splash actually present: jpg first, then bmp (retail's fallback, donor WinMain.cpp:1038..1041).
// `exists(path)` is the archive / filesystem lookup (ArchiveFileSystem::doesFileExist or a test lambda).
// On success returns true and sets `*chosen` to the name that exists (jpg or bmp). On failure returns false
// and sets `*error` to a message naming both candidates (a missing boot image is a boot error, not a blank screen).
bool resolve(const std::string &language,
	const std::function<bool(const std::string &)> &exists,
	std::string *chosen,
	std::string *error);

// The same, but with a default English fallback when language is empty (retail's GetRegistryLanguage() defaults to English).
inline std::string defaultLanguage() { return "English"; }

} // namespace BootSplash
