// OpenBFME + Ennorath one-folder package: the program the player double-clicks (owner 2026-11-08: "one exe, nothing else to install").
//
// The package holds no game or mod files. On the first start this program asks for the player's own folders and lays them out the way
// OpenBFME mounts them, with the bundled embeddable Python (no install) running tools/mod/aio_package.py:
//   * the Rise of the Witch-king folder the All In One BFME Launcher installed with Ennorath (one merged folder): the clean 2.01 files
//     (package original-RotWK) and the Ennorath files (package 05a64473-...) are hard-linked from it where they match the service's MD5,
//     the rest (the files the mod overwrote) is downloaded from the All In One service;
//   * Battle for Middle-earth II 1.06: the player's own folder, used as it is (aio_package.py lays out RotWK packages only).
// Then, and on every later start, it starts the game: game\Godot_v4.7.2-stable_win64.exe --path game\godot -- --mod-install=<Ennorath>
// with ROTWK_INSTALL / BFME2_INSTALL set to the laid-out folders.
//
// Build (MSVC x64, static runtime: nothing to install):
//   cl /nologo /O2 /MT /EHsc /DUNICODE /D_UNICODE launcher.cpp /Fe:OpenBFME.exe ole32.lib shell32.lib user32.lib
#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>

#include <cstdio>
#include <string>
#include <vector>

namespace
{
const wchar_t *kTitle = L"OpenBFME - Ennorath";
const wchar_t *kEnnorathGuid = L"05a64473-c7f5-4fff-8e00-f30e06db9d96";
const wchar_t *kRotwkLayout = L"Vanilla_(2.01)";

std::wstring g_dir; // the folder of this program, with a trailing backslash

bool exists(const std::wstring &p)
{
	return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool isDir(const std::wstring &p)
{
	const DWORD a = GetFileAttributesW(p.c_str());
	return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring lower(std::wstring s)
{
	for (auto &c : s)
	{
		c = (wchar_t)towlower(c);
	}
	return s;
}

void say(const std::wstring &text)
{
	wprintf(L"%ls\n", text.c_str());
	fflush(stdout);
}

int box(const std::wstring &text, UINT flags)
{
	return MessageBoxW(GetConsoleWindow(), text.c_str(), kTitle, flags | MB_SETFOREGROUND);
}

std::wstring pickFolder(const std::wstring &title)
{
	std::wstring out;
	IFileOpenDialog *dlg = nullptr;
	if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
	{
		return out;
	}
	DWORD opts = 0;
	dlg->GetOptions(&opts);
	dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
	dlg->SetTitle(title.c_str());
	if (SUCCEEDED(dlg->Show(GetConsoleWindow())))
	{
		IShellItem *item = nullptr;
		if (SUCCEEDED(dlg->GetResult(&item)))
		{
			PWSTR path = nullptr;
			if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
			{
				out = path;
				CoTaskMemFree(path);
			}
			item->Release();
		}
	}
	dlg->Release();
	return out;
}

// the settings file beside the program: key=value lines
std::wstring cfgPath()
{
	return g_dir + L"openbfme-paths.txt";
}

std::wstring cfgGet(const std::wstring &key)
{
	FILE *f = _wfopen(cfgPath().c_str(), L"r, ccs=UTF-8");
	if (!f)
	{
		return L"";
	}
	wchar_t line[2048];
	std::wstring out;
	while (fgetws(line, 2048, f))
	{
		std::wstring l(line);
		while (!l.empty() && (l.back() == L'\n' || l.back() == L'\r'))
		{
			l.pop_back();
		}
		if (l.rfind(key + L"=", 0) == 0)
		{
			out = l.substr(key.size() + 1);
		}
	}
	fclose(f);
	return out;
}

void cfgSet(const std::wstring &key, const std::wstring &value)
{
	std::vector<std::wstring> lines;
	if (FILE *f = _wfopen(cfgPath().c_str(), L"r, ccs=UTF-8"))
	{
		wchar_t line[2048];
		while (fgetws(line, 2048, f))
		{
			std::wstring l(line);
			while (!l.empty() && (l.back() == L'\n' || l.back() == L'\r'))
			{
				l.pop_back();
			}
			if (l.rfind(key + L"=", 0) != 0 && !l.empty())
			{
				lines.push_back(l);
			}
		}
		fclose(f);
	}
	lines.push_back(key + L"=" + value);
	if (FILE *f = _wfopen(cfgPath().c_str(), L"w, ccs=UTF-8"))
	{
		for (const auto &l : lines)
		{
			fwprintf(f, L"%ls\n", l.c_str());
		}
		fclose(f);
	}
}

// the All In One service's language code of a RotWK folder, from its lang\<language>.big
std::wstring languageOf(const std::wstring &rotwk)
{
	static const wchar_t *names[][2] = { { L"english", L"EN" }, { L"russian", L"RU" }, { L"german", L"DE" }, { L"french", L"FR" },
		{ L"spanish", L"ES" }, { L"italian", L"IT" }, { L"polish", L"PL" }, { L"dutch", L"NL" }, { L"norwegian", L"NO" },
		{ L"swedish", L"SV" }, { L"czech", L"CS" }, { L"chinese", L"ZH" }, { L"korean", L"KO" } };
	for (const auto &n : names)
	{
		if (exists(rotwk + L"\\lang\\" + n[0] + L".big"))
		{
			return n[1];
		}
	}
	return L"EN";
}

// runs the bundled Python on aio_package.py; its output goes to this console
bool layOut(const std::wstring &package, const std::wstring &corpus, const std::wstring &language, const std::wstring &fromLocal)
{
	std::wstring cmd = L"\"" + g_dir + L"python\\python.exe\" \"" + g_dir + L"tools\\mod\\aio_package.py\" " + package + L" \"" + corpus +
		L"\" --language=" + language;
	if (!fromLocal.empty())
	{
		cmd += L" \"--from-local=" + fromLocal + L"\"";
	}
	say(L"> " + cmd);
	STARTUPINFOW si{};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi{};
	std::vector<wchar_t> buf(cmd.begin(), cmd.end());
	buf.push_back(0);
	if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr, g_dir.c_str(), &si, &pi))
	{
		say(L"could not start the bundled Python (python\\python.exe)");
		return false;
	}
	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD code = 1;
	GetExitCodeProcess(pi.hProcess, &code);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return code == 0;
}

// the player's BFME2 folder remembered in openbfme-paths.txt, "" when none or gone
std::wstring bfme2Folder()
{
	const std::wstring f = cfgGet(L"BFME2_FOLDER");
	return !f.empty() && isDir(f) ? f : L"";
}

int fail(const std::wstring &why)
{
	say(L"");
	say(L"SETUP STOPPED: " + why);
	box(why + L"\n\nNothing was changed in your game folders. Run OpenBFME.exe again to retry.", MB_OK | MB_ICONERROR);
	return 1;
}

int setup()
{
	const std::wstring rotwkOut = g_dir + L"layouts\\games\\" + kRotwkLayout + L"\\install";
	const std::wstring modOut = g_dir + L"layouts\\mods\\Ennorath\\install";
	say(L"First start: OpenBFME lays out your game files beside this program (folder layouts). Your game folders are not changed.");
	say(L"");
	if (!exists(rotwkOut) || !exists(modOut))
	{
		box(L"Choose the folder of Rise of the Witch-king with Ennorath installed (the one the All In One BFME Launcher installed,\n"
		    L"for example C:\\ROTWK).\n\nFiles that match are linked from it; the files Ennorath replaced are downloaded from the All In One "
		    L"service (internet needed, a few minutes).",
		    MB_OK | MB_ICONINFORMATION);
		std::wstring merged = cfgGet(L"ROTWK_WITH_ENNORATH");
		if (merged.empty() || !isDir(merged))
		{
			merged = pickFolder(L"Rise of the Witch-king folder with Ennorath");
		}
		if (merged.empty())
		{
			return fail(L"No Rise of the Witch-king folder was chosen.");
		}
		if (!exists(merged + L"\\lotrbfme2ep1.exe") && !exists(merged + L"\\game.dat"))
		{
			return fail(L"This folder is not a Rise of the Witch-king install (no lotrbfme2ep1.exe / game.dat):\n" + merged);
		}
		cfgSet(L"ROTWK_WITH_ENNORATH", merged);
		const std::wstring lang = languageOf(merged);
		say(L"Rise of the Witch-king: " + merged + L" (language " + lang + L")");
		if (!layOut(L"original-RotWK", g_dir + L"layouts\\games", lang, merged))
		{
			return fail(L"Laying out Rise of the Witch-king 2.01 failed (see the text in this window).");
		}
		if (!layOut(kEnnorathGuid, g_dir + L"layouts\\mods", lang, merged))
		{
			return fail(L"Laying out Ennorath failed (see the text in this window).");
		}
	}
	if (bfme2Folder().empty())
	{
		// aio_package.py lays out RotWK packages only (it leaves other games' files out): BFME2 is the player's own 1.06 folder, used as it is
		std::wstring bfme2;
		const wchar_t *guesses[] = { L"C:\\Program Files (x86)\\BFME2", L"C:\\Program Files (x86)\\EA Games\\The Battle for Middle-earth (tm) II",
			L"C:\\Program Files (x86)\\Electronic Arts\\The Battle for Middle-earth (tm) II", L"C:\\BFME2" };
		for (const wchar_t *g : guesses)
		{
			if (bfme2.empty() && exists(std::wstring(g) + L"\\asset.dat") && exists(std::wstring(g) + L"\\ini.big"))
			{
				bfme2 = g;
			}
		}
		if (bfme2.empty())
		{
			box(L"OpenBFME also needs Battle for Middle-earth II 1.06 (the game itself, not a mod).\n\nChoose its folder "
			    L"(for example C:\\Program Files (x86)\\BFME2).",
			    MB_OK | MB_ICONINFORMATION);
			bfme2 = pickFolder(L"Battle for Middle-earth II folder");
			if (bfme2.empty())
			{
				return fail(L"No Battle for Middle-earth II folder was chosen.");
			}
			if (!exists(bfme2 + L"\\ini.big"))
			{
				return fail(L"This folder is not a Battle for Middle-earth II install (no ini.big):\n" + bfme2);
			}
		}
		cfgSet(L"BFME2_FOLDER", bfme2);
		say(L"Battle for Middle-earth II: " + bfme2);
	}
	say(L"Setup done.");
	return 0;
}
} // namespace

int wmain()
{
	SetConsoleOutputCP(CP_UTF8);
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	wchar_t self[MAX_PATH];
	GetModuleFileNameW(nullptr, self, MAX_PATH);
	g_dir = self;
	g_dir = g_dir.substr(0, g_dir.find_last_of(L'\\') + 1);

	const std::wstring rotwk = g_dir + L"layouts\\games\\" + kRotwkLayout + L"\\install";
	const std::wstring mod = g_dir + L"layouts\\mods\\Ennorath\\install";
	if (!exists(rotwk) || !exists(mod) || bfme2Folder().empty())
	{
		if (setup() != 0)
		{
			return 1;
		}
	}
	const std::wstring bfme2 = bfme2Folder();
	SetEnvironmentVariableW(L"ROTWK_INSTALL", rotwk.c_str());
	SetEnvironmentVariableW(L"BFME2_INSTALL", bfme2.c_str());

	const std::wstring godot = g_dir + L"game\\Godot_v4.7.2-stable_win64.exe";
	if (!exists(godot))
	{
		return fail(L"game\\Godot_v4.7.2-stable_win64.exe is missing: unpack the whole archive again.");
	}
	std::wstring cmd = L"\"" + godot + L"\" --path \"" + g_dir + L"game\\godot\" -- \"--mod-install=" + mod + L"\"";
	STARTUPINFOW si{};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi{};
	std::vector<wchar_t> buf(cmd.begin(), cmd.end());
	buf.push_back(0);
	if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, DETACHED_PROCESS, nullptr, g_dir.c_str(), &si, &pi))
	{
		return fail(L"The game could not be started.");
	}
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return 0;
}
