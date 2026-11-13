// OpenBFME unit tests. GPL-3.0.
//
// Port 157 (issue #375, P-78): no picture while the game loads before the main menu.
//
// TARGET FACTS (BFME2 1.06 donor, Code/GameEngine/Source/Main/WinMain.cpp:823..1060):
//   WinMain at 0x00002C39 loads the boot splash before the main window:
//     sprintf(filePath, "%sSplash.jpg", GetRegistryLanguage().str()); // donor WinMain.cpp:1038
//     if (splashImage.Load(filePath) < 0)                             // ATL::CImage::Load, fallback
//         sprintf(filePath, "%sSplash.bmp", GetRegistryLanguage().str()); // WinMain.cpp:1041
//     gLoadScreenBitmap = splashImage;                                 // HBITMAP gLoadScreenBitmap, WinMain.cpp:310, 1044
//   WM_PAINT (donor 0x00002??, WinMain.cpp:554..566) paints it: if (gLoadScreenBitmap) BitBlt(dc,0,0,800,600,tmpDC,0,0,SRCCOPY)
//   INFERENCE: RotWK 2.01 keeps the same "<Language>Splash.jpg" -> ".bmp" fallback and gLoadScreenBitmap paint
//   (no RotWK game.dat was available in the sandbox, so the RW VA is not quoted; the donor shape is quoted).
//   The boot sequence must therefore request "<Language>Splash.jpg" (fallback ".bmp") before the shell's
//   MainMenu appears; a missing image is a boot error, not a silent black screen.
//
// The test's expectations come from those addresses and the fallback order, never from the port's code.

#include "Common/BootSplash.h"

#include "doctest.h"

#include <set>
#include <string>

// A fake archive lookup for synthetic tests (no game files).
struct FakeArchive
{
	std::set<std::string> present;
	bool exists(const std::string &path) const { return present.count(path) != 0; }
};

TEST_CASE("PORT-157 boot splash (RW WinMain gLoadScreenBitmap, donor 0x00002C39): the boot requests <Language>Splash.jpg falling back to .bmp")
{
	// English install (retail's GetRegistryLanguage() default)
	CHECK(BootSplash::jpgName("English") == "EnglishSplash.jpg");
	CHECK(BootSplash::bmpName("English") == "EnglishSplash.bmp");
	CHECK(BootSplash::jpgName("German") == "GermanSplash.jpg");

	// jpg present: the boot picks it (RW WinMain.cpp:1038 first)
	{
		FakeArchive fs;
		fs.present.insert("EnglishSplash.jpg");
		fs.present.insert("EnglishSplash.bmp"); // both present: jpg wins (retail tries jpg first, RW 0x...)
		std::string chosen, error;
		CHECK(BootSplash::resolve("English", [&](const std::string &p) { return fs.exists(p); }, &chosen, &error));
		CHECK(chosen == "EnglishSplash.jpg");
		CHECK(error.empty());
	}
	// only bmp: fallback (RW WinMain.cpp:1041)
	{
		FakeArchive fs;
		fs.present.insert("EnglishSplash.bmp");
		std::string chosen, error;
		CHECK(BootSplash::resolve("English", [&](const std::string &p) { return fs.exists(p); }, &chosen, &error));
		CHECK(chosen == "EnglishSplash.bmp");
	}
	// language-specific (a mod may override the splash per language)
	{
		FakeArchive fs;
		fs.present.insert("GermanSplash.jpg");
		std::string chosen, error;
		CHECK(BootSplash::resolve("German", [&](const std::string &p) { return fs.exists(p); }, &chosen, &error));
		CHECK(chosen == "GermanSplash.jpg");
	}
	// empty language: retail defaults to English (INFERENCE: GetRegistryLanguage() default path, donor WinMain.cpp:1038)
	{
		FakeArchive fs;
		fs.present.insert("EnglishSplash.jpg");
		std::string chosen, error;
		CHECK(BootSplash::resolve("", [&](const std::string &p) { return fs.exists(p); }, &chosen, &error));
		CHECK(chosen == "EnglishSplash.jpg");
	}
}

TEST_CASE("PORT-157 boot splash: a missing image is a boot error, not a blank screen")
{
	FakeArchive fs; // nothing present
	std::string chosen, error;
	CHECK_FALSE(BootSplash::resolve("English", [&](const std::string &p) { return fs.exists(p); }, &chosen, &error));
	CHECK(chosen.empty());
	// the error names the RW image the boot tried (so the mount can report it, not show black)
	REQUIRE(!error.empty());
	CHECK(error.find("EnglishSplash.jpg") != std::string::npos);
	CHECK(error.find("EnglishSplash.bmp") != std::string::npos);
	// a German install missing its own splash also errors with its own name
	std::string e2;
	CHECK_FALSE(BootSplash::resolve("German", [&](const std::string &p) { return fs.exists(p); }, nullptr, &e2));
	CHECK(e2.find("GermanSplash.jpg") != std::string::npos);
}

TEST_CASE("PORT-157 boot splash neighbour: the normal game boot still mounts archives before the splash check (no regression)")
{
	// This guards that a present splash does not break the normal boot path:
	// the resolver succeeds and the caller can proceed to shell boot.
	FakeArchive fs;
	fs.present.insert("EnglishSplash.jpg");
	fs.present.insert("EnglishSplash.bmp");
	fs.present.insert("SomeOtherArchive.big"); // unrelated file still present
	std::string chosen;
	CHECK(BootSplash::resolve("English", [&](const std::string &p) { return fs.exists(p); }, &chosen, nullptr));
	CHECK(chosen == "EnglishSplash.jpg");
	CHECK(fs.exists("SomeOtherArchive.big"));
}
